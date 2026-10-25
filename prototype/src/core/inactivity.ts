import type { MorningPlan } from '../types.js';
import type { TripContext } from './trip.js';
import { tripSatisfiesReminder } from './trip.js';

const MAX_GAP_MS = 2500;
export const INACTIVITY_THRESHOLDS = Object.freeze({ flatDegrees: 20, acceleration: 0.35, rotationRate: 0.15, maximumGapMs: MAX_GAP_MS });
type Sensor = 'accelerometer' | 'gravity' | 'gyroscope';
type Sample = { at: number; receivedAt: number; quiet: boolean };
const object = (v: unknown): v is Record<string, unknown> => !!v && typeof v === 'object' && !Array.isArray(v);
const finite = (v: unknown): v is number => typeof v === 'number' && Number.isFinite(v);
export interface InactivitySnapshot {
  status: 'unknown' | 'active' | 'counting' | 'qualified';
  observedSeconds: number; requiredSeconds: number; lastSampleAt: number | null; since: number | null;
  reason: string;
}

/** Continuous device stillness, never an inference that its owner is inactive. */
export class FlatInactivityTracker {
  private stream = '';
  private message = -1;
  private retired = new Set<string>();
  private samples: Partial<Record<Sensor, Sample>> = {};
  private since: number | null = null;
  private latestAt = -Infinity;
  private lastReceived = -Infinity;
  private reason = 'Waiting for fresh acceleration, gravity, and gyroscope readings.';

  ingest(body: unknown, now = Date.now()) {
    if (!object(body) || !Array.isArray(body.payload) || body.payload.length > 2000
      || typeof body.deviceId !== 'string' || !body.deviceId || body.deviceId.length > 200
      || typeof body.sessionId !== 'string' || !body.sessionId || body.sessionId.length > 200
      || !finite(body.messageId) || !Number.isSafeInteger(body.messageId) || body.messageId < 0) return;
    const stream = JSON.stringify([body.deviceId, body.sessionId]);
    if (this.retired.has(stream) || stream === this.stream && body.messageId <= this.message) return;
    const readings: { sensor: Sensor; at: number; quiet: boolean }[] = [];
    for (const raw of body.payload) {
      if (!object(raw) || !['accelerometer','gravity','gyroscope'].includes(String(raw.name)) || !finite(raw.time) || !object(raw.values)) continue;
      const at = raw.time / 1e6;
      if (at < now - MAX_GAP_MS || at > now + 2000 || raw.accuracy === 0) continue;
      const v = raw.values;
      if (![v.x,v.y,v.z].every(finite)) continue;
      const magnitude = Math.hypot(v.x as number,v.y as number,v.z as number);
      const sensor = raw.name as Sensor;
      if (sensor === 'gravity' && (magnitude < 0.1 || magnitude > 20)) continue;
      const quiet = sensor === 'gravity' ? Math.acos(Math.min(1, Math.abs(v.z as number)/magnitude))*180/Math.PI <= INACTIVITY_THRESHOLDS.flatDegrees
        : magnitude <= (sensor === 'accelerometer' ? INACTIVITY_THRESHOLDS.acceleration : INACTIVITY_THRESHOLDS.rotationRate);
      readings.push({sensor,at,quiet});
    }
    readings.sort((a,b)=>a.at-b.at);
    if (!readings.length || stream !== this.stream && readings.at(-1)!.at <= this.latestAt) return;
    if (stream !== this.stream) {
      if (this.stream) this.retired.add(this.stream);
      if (this.retired.size > 64) this.retired.delete(this.retired.values().next().value!);
      this.stream = stream; this.samples = {}; this.since = null;
    }
    this.message = body.messageId;
    if (now - this.lastReceived > MAX_GAP_MS) { this.since = null; this.reason = 'The sensor feed had a gap; the timer restarted.'; }
    this.lastReceived = now;
    for (const {sensor,at,quiet} of readings) {
      const prior = this.samples[sensor];
      if (prior && at <= prior.at) continue;
      if (prior && at-prior.at > MAX_GAP_MS) this.since = null;
      this.samples[sensor] = {at,receivedAt:now,quiet};
      this.latestAt = Math.max(this.latestAt,at);
      const all = this.current();
      if (all.length !== 3 || all.some(s=>at-s.at > MAX_GAP_MS)) {
        this.since = null; this.reason = 'Waiting for continuous readings from all three motion sensors.';
      } else if (all.some(s=>!s.quiet)) {
        this.since = null; this.reason = 'Phone movement or tilt reset the timer.';
      } else {
        this.since ??= at;
        this.reason = 'Fresh readings show a flat phone with little acceleration or rotation.';
      }
    }
  }

  snapshot(now = Date.now(), requiredSeconds = 300, notBefore = -Infinity): InactivitySnapshot {
    const all = this.current();
    const live = all.length === 3 && all.every(s=>now-s.at <= MAX_GAP_MS && s.at <= now+2000 && now-s.receivedAt <= MAX_GAP_MS);
    const lastSampleAt = all.length === 3 ? Math.min(...all.map(s=>s.at)) : null;
    const since = live && this.since !== null ? Math.max(this.since,notBefore) : null;
    const observedSeconds = since !== null && lastSampleAt !== null ? Math.max(0,(Math.min(now,lastSampleAt)-since)/1000) : 0;
    const status = !live ? 'unknown' : since === null ? 'active' : observedSeconds >= requiredSeconds ? 'qualified' : 'counting';
    return {status,observedSeconds,requiredSeconds,lastSampleAt,since,reason:!live ? 'Motion readings are missing or stale; no inactive time is credited.' : this.reason};
  }
  private current(): Sample[] { return [this.samples.accelerometer,this.samples.gravity,this.samples.gyroscope].filter((s):s is Sample=>!!s); }
}

export function inactivityWatchKey(plan: MorningPlan): string {
  return `${plan.id}:leave:flat-inactive:${plan.flatInactivity?.armedAt ?? 'disabled'}`;
}
export function inactivityDecision(plan: MorningPlan, inactivity: InactivitySnapshot, trip: TripContext, now = Date.now()) {
  const rule = plan.flatInactivity;
  const result = (status: string, shouldCall = false, satisfied = false) => ({ status, shouldCall, satisfied });
  if (!rule || !plan.armed || plan.event.status !== 'confirmed') return result('disabled');
  if (!Number.isFinite(Date.parse(rule.armedAt)) || !Number.isFinite(Date.parse(plan.leaveAt)) || !Number.isFinite(Date.parse(plan.event.startAt))) return result('unknown');
  if (now >= Date.parse(plan.event.startAt)) return result('expired');
  if (now < Math.max(Date.parse(plan.leaveAt),Date.parse(rule.armedAt))) return result('before-leave-time');
  if (tripSatisfiesReminder(trip,'leave')) return result('already-on-way-or-near-destination',false,true);
  if (trip.status !== 'still-at-origin' || !finite(trip.lastGpsSampleAt) || now-trip.lastGpsSampleAt>10000 || trip.lastGpsSampleAt>now+2000) return result('waiting-for-origin-location');
  if (!finite(inactivity.lastSampleAt) || now-inactivity.lastSampleAt>MAX_GAP_MS || inactivity.lastSampleAt>now+2000) return result('waiting-for-sensors');
  if (inactivity.status !== 'qualified' || inactivity.observedSeconds < rule.durationSeconds) return result(inactivity.status === 'unknown' ? 'waiting-for-sensors' : 'waiting-for-flat-inactivity');
  return result('call-due',true);
}
