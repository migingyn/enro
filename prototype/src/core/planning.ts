import { randomUUID } from 'node:crypto';
import type { AppState, EnroEvent, MorningPlan, RouteEstimate, TravelMode } from '../types.js';

export const MAX_JOB_LATENESS_MS = 2 * 60_000;
export const MAX_ROUTE_AGE_MS = 15 * 60_000;

interface PlanInput {
  event: EnroEvent;
  origin: string;
  travelMode: TravelMode;
  preparationMinutes: number;
  bufferMinutes: number;
  route: RouteEstimate;
  accessMinutes?: number;
}

export interface ScheduledJob {
  key: string;
  kind: 'wake' | 'leave';
  planId: string;
  dueAt: string;
}

/** Parse a real calendar instant; local dates and silently normalized dates are rejected. */
function explicitInstant(value: unknown, field: string): number {
  if (typeof value !== 'string') throw new Error(`${field} must include an explicit UTC offset.`);
  const match = /^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2})(?::(\d{2})(?:\.(\d{1,9}))?)?(Z|[+-]\d{2}:\d{2})$/.exec(value);
  if (!match) throw new Error(`${field} must be an ISO date and time with Z or a UTC offset.`);
  const [year, month, day, hour, minute, second] = match.slice(1, 7).map((part) => Number(part ?? 0));
  const calendar = new Date(Date.UTC(year!, month! - 1, day!, hour, minute, second));
  if (year! < 1000 || calendar.getUTCFullYear() !== year || calendar.getUTCMonth() !== month! - 1 ||
      calendar.getUTCDate() !== day || hour! > 23 || minute! > 59 || second! > 59) {
    throw new Error(`${field} contains an invalid calendar date or time.`);
  }
  const offset = match[8]!;
  if (offset !== 'Z') {
    const hours = Number(offset.slice(1, 3));
    const minutes = Number(offset.slice(4, 6));
    if (hours > 14 || minutes > 59 || (hours === 14 && minutes !== 0)) {
      throw new Error(`${field} contains an invalid UTC offset.`);
    }
  }
  const timestamp = Date.parse(value);
  if (!Number.isFinite(timestamp)) throw new Error(`${field} is not a valid instant.`);
  return timestamp;
}

function text(value: unknown, field: string, max = 1000): string {
  if (typeof value !== 'string' || !value.trim() || value.length > max) {
    throw new Error(`${field} must be nonempty text of at most ${max} characters.`);
  }
  return value.trim();
}

function bounded(value: unknown, field: string, maximum: number, minimum = 0): number {
  if (typeof value !== 'number' || !Number.isFinite(value) || value < minimum || value > maximum) {
    throw new Error(`${field} must be between ${minimum} and ${maximum}.`);
  }
  return value;
}

function clock(value: number): void {
  if (!Number.isFinite(value) || !Number.isFinite(new Date(value).getTime())) throw new Error('Invalid current time.');
}

/**
 * Route duration is a real provider estimate, never a fallback speed calculation.
 * Plans start disarmed. A past wake time may be displayed, but dueJobs will never
 * catch it up beyond two minutes. Arming remains an explicit caller action.
 */
export function createPlan(input: PlanInput, now = Date.now()): MorningPlan {
  clock(now);
  if (!input || !input.event) throw new Error('An event is required.');
  const event = input.event;
  if (event.status !== 'confirmed') throw new Error('Only confirmed events can be planned.');
  text(event.id, 'Event ID', 512);
  text(event.title, 'Event title');
  const location = text(event.location, 'Event location');
  const origin = text(input.origin, 'Origin');
  const start = explicitInstant(event.startAt, 'Event start');
  if (start <= now) throw new Error('Event start must be in the future.');
  const timeZone = text(event.timeZone, 'Event time zone', 100);
  try { new Intl.DateTimeFormat('en-US', { timeZone }).format(start); }
  catch { throw new Error('Event time zone must be a recognized time zone.'); }
  if (!['DRIVE', 'WALK', 'TRANSIT'].includes(input.travelMode)) throw new Error('Unsupported travel mode.');
  const preparationMinutes = bounded(input.preparationMinutes, 'Preparation minutes', 720);
  const bufferMinutes = bounded(input.bufferMinutes, 'Buffer minutes', 360);
  const route = input.route;
  if (!route || !['google-routes', 'osrm'].includes(route.provider)) throw new Error('A supported route provider estimate is required.');
  if (route.provider === 'osrm' && input.travelMode !== 'DRIVE') throw new Error('This OSRM connection supports driving only.');
  const accessMinutes = bounded(input.accessMinutes ?? 0, 'Time to reach the vehicle', 120);
  const durationSeconds = bounded(route.durationSeconds, 'Route duration seconds', 172_800);
  if (route.distanceMeters !== undefined) bounded(route.distanceMeters, 'Route distance meters', 50_000_000);
  const fetched = explicitInstant(route.fetchedAt, 'Route fetch time');
  if (fetched < now - MAX_ROUTE_AGE_MS || fetched > now + MAX_JOB_LATENESS_MS) {
    throw new Error('Route estimate must have been fetched within the last 15 minutes.');
  }
  const leave = start - durationSeconds * 1000 - (bufferMinutes + accessMinutes) * 60_000;
  const wake = leave - preparationMinutes * 60_000;
  return {
    id: randomUUID(), event: { ...structuredClone(event), location, timeZone }, origin,
    travelMode: input.travelMode, preparationMinutes, bufferMinutes, accessMinutes,
    route: { ...route }, wakeAt: new Date(wake).toISOString(), leaveAt: new Date(leave).toISOString(),
    createdAt: new Date(now).toISOString(), armed: false,
  };
}

function scheduledJobs(plans: MorningPlan[], jobs: AppState['jobs'], now: number, includePastEvents = false): ScheduledJob[] {
  clock(now);
  const found = new Map<string, ScheduledJob>();
  for (const plan of plans) {
    if (!plan.armed || plan.event.status !== 'confirmed' || !plan.id) continue;
    let eventTime: number;
    try { eventTime = explicitInstant(plan.event.startAt, 'Event start'); } catch { continue; }
    if (!includePastEvents && eventTime <= now) continue;
    for (const kind of plan.reminderKinds ?? ['wake', 'leave'] as const) {
      if (kind === 'leave' && plan.flatInactivity) continue;
      let timestamp: number;
      try { timestamp = explicitInstant(kind === 'wake' ? plan.wakeAt : plan.leaveAt, 'Scheduled time'); }
      catch { continue; }
      if (timestamp > now || timestamp >= eventTime) continue;
      const dueAt = new Date(timestamp).toISOString();
      const key = `${plan.id}:${kind}:${dueAt}`;
      // All recorded states, including failed/unknown, suppress automatic retries.
      if (!Object.hasOwn(jobs, key)) found.set(key, { key, kind, planId: plan.id, dueAt });
    }
  }
  return [...found.values()].sort((a, b) => Date.parse(a.dueAt) - Date.parse(b.dueAt) || a.key.localeCompare(b.key));
}

/** Pure selector. Persist `dispatching` before dialing; repeated reads do not reserve a job. */
export function dueJobs(plans: MorningPlan[], jobs: AppState['jobs'], now: number): ScheduledJob[] {
  return scheduledJobs(plans, jobs, now).filter((job) => now - Date.parse(job.dueAt) <= MAX_JOB_LATENESS_MS);
}

/** Pure selector for optional audit/persistence as state `missed`; never dial these. */
export function missedJobs(plans: MorningPlan[], jobs: AppState['jobs'], now: number): ScheduledJob[] {
  return scheduledJobs(plans, jobs, now, true).filter((job) => now - Date.parse(job.dueAt) > MAX_JOB_LATENESS_MS);
}
