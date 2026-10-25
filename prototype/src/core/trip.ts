import type { MorningPlan } from '../types.js';
import { locationContext } from './location-context.js';
import type { DepartureLocationFix } from './departure.js';

export interface TripContext {
  status: 'unknown' | 'still-at-origin' | 'moving-near-origin' | 'departed-origin' | 'approaching-destination' | 'moving-away-from-destination' | 'near-destination';
  summary: string;
  distanceToDestinationMeters: number | null;
  gpsMovement: string;
  destinationConfirmed: boolean;
  travelProgressConfirmed?: boolean;
  phonePosition: string;
  activity: string;
  observedAt: string;
  lastGpsSampleAt?: number | null;
}

/** GPS establishes geographic progress; phone pose and step counts cannot establish a destination. */
export function assessTrip(plan: MorningPlan, fixes: readonly DepartureLocationFix[], now = Date.now(),
  phonePosition = 'unknown', activity = 'unknown'): TripContext {
  const counts = new Map<string, number>();
  for (const f of fixes) { const key = `${f.deviceId}:${f.sessionId}:${f.timestamp}`; counts.set(key, (counts.get(key) ?? 0) + 1); }
  fixes = fixes.filter(f => counts.get(`${f.deviceId}:${f.sessionId}:${f.timestamp}`) === 1);
  const origin = plan.originCoordinates ? { ...plan.originCoordinates, label: plan.origin, radiusMeters: plan.originRadiusMeters ?? 200 } : undefined;
  const destination = plan.destinationCoordinates ? { ...plan.destinationCoordinates, label: plan.event.location, radiusMeters: plan.destinationRadiusMeters ?? 150 } : undefined;
  const start = locationContext(true, fixes, origin, now);
  const end = locationContext(true, fixes, destination, now);
  const base = { distanceToDestinationMeters: end.distanceFromPlaceMeters, gpsMovement: start.movement,
    destinationConfirmed: false, phonePosition, activity, observedAt: new Date(now).toISOString(), lastGpsSampleAt: start.current?.timestamp ?? null };
  const result = (status: TripContext['status'], summary: string, destinationConfirmed = false): TripContext => ({ ...base, status, summary, destinationConfirmed });
  // Indoor GPS can be too coarse for travel progress yet still wholly inside a
  // large campus boundary. Report that limited fact without relaxing arrival.
  if (origin && destination && start.current && start.status === 'imprecise' && start.current.accuracyMeters <= 100
    && now - start.current.timestamp <= 10000) {
    const r = Math.PI / 180;
    const distance = Math.hypot((start.current.longitude-origin.longitude)*r*Math.cos(origin.latitude*r), (start.current.latitude-origin.latitude)*r)*6371000;
    if (distance + start.current.accuracyMeters <= origin.radiusMeters) return result('still-at-origin',
      `GPS places the phone within the ${plan.origin} area, with about ${Math.round(start.current.accuracyMeters)} meters of reported uncertainty. Departure for ${plan.event.location} has not been observed.`);
  }
  if (!origin || !destination || !start.current || ['imprecise','waiting','uncertain'].includes(start.status)
    || ['imprecise','waiting','uncertain'].includes(end.status)) {
    return result('unknown', 'Fresh, accurate GPS or place boundaries are unavailable. Travel progress is unconfirmed.');
  }
  if (end.location === 'inside-origin' && end.fixCount >= 3 && end.spanSeconds >= 10) {
    // Every fix used for the arrival interval must fit within the destination.
    const stream = fixes.filter(f => f.deviceId === end.current?.deviceId && f.sessionId === end.current?.sessionId
      && f.timestamp >= now - 45000 && f.timestamp <= now + 2000 && f.accuracyMeters > 0 && f.accuracyMeters <= 25);
    if (stream.length >= 3 && stream.every(f => locationContext(true, [f], destination, now).location === 'inside-origin')) {
      return result('near-destination', `Repeated GPS fixes place the phone near ${plan.event.location}. This confirms the phone's area, not attendance.`, true);
    }
  }
  if (start.location === 'inside-origin') {
    return start.movement === 'moving'
      ? result('moving-near-origin', `GPS movement is visible within ${plan.origin}. You may be walking toward your car; departure for ${plan.event.location} is not yet verified.`)
      : result('still-at-origin', `GPS still places the phone near ${plan.origin}. Departure for ${plan.event.location} has not been observed.`);
  }
  if (start.location === 'outside-origin-confirmed') {
    if (end.movement === 'moving' && end.trend === 'toward') {
      const first = fixes.filter(f => f.deviceId === end.current?.deviceId && f.sessionId === end.current?.sessionId
        && f.timestamp >= now - 45000 && f.timestamp <= now + 2000 && f.accuracyMeters > 0 && f.accuracyMeters <= 25).sort((a,b)=>a.timestamp-b.timestamp)[0];
      const initialDistance = first ? locationContext(true, [first], destination, now).distanceFromPlaceMeters : null;
      const progress = first && initialDistance !== null && end.distanceFromPlaceMeters !== null
        ? initialDistance - end.distanceFromPlaceMeters - first.accuracyMeters - end.current!.accuracyMeters : 0;
      return { ...result('approaching-destination', `GPS shows movement away from ${plan.origin} and decreasing distance to ${plan.event.location}. This suggests progress toward the destination; the exact route is unverified.`),
        travelProgressConfirmed: progress >= 75 && end.fixCount >= 3 && end.spanSeconds >= 10 && now - end.current!.timestamp <= 10000 };
    }
    if (end.movement === 'moving' && end.trend === 'away') return result('moving-away-from-destination', `The phone has left ${plan.origin}, but GPS distance to ${plan.event.location} is increasing.`);
    return result('departed-origin', `Repeated GPS fixes place the phone outside ${plan.origin}. Travel toward ${plan.event.location} is not yet established.`);
  }
  return result('unknown', 'GPS is near a place boundary. Travel progress is unconfirmed.');
}

/** The due departure reminder is unnecessary only after arrival or substantial fresh directional progress. */
export function tripSatisfiesReminder(trip: TripContext, kind: 'wake' | 'leave'): boolean {
  return trip.destinationConfirmed || (kind === 'leave' && trip.status === 'approaching-destination' && trip.travelProgressConfirmed === true);
}
