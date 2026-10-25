import type { MorningPlan, MotionSnapshot } from '../types.js';
import { PEDOMETER_FRESH_MS } from './motion.js';

// Experimental demo thresholds, not a validated human-activity or arrival detector.
export const EXPERIMENTAL_DEPARTURE_THRESHOLDS = Object.freeze({
  motionFreshMs: 5_000,
  futureToleranceMs: 2_000,
  gpsFreshMs: 45_000,
  originRadiusMeters: 120,
  maximumAccuracyMeters: 25,
  minimumOutsideFixes: 3,
  minimumOutsideSpanMs: 10_000,
});

export interface Coordinates { latitude: number; longitude: number }
export interface SensorStream { deviceId: string; sessionId: string }
export interface DepartureLocationFix extends Coordinates, SensorStream {
  /** Unix epoch milliseconds; accuracy is the reported horizontal radius. */
  timestamp: number;
  accuracyMeters: number;
  speedMetersPerSecond?: number;
  speedAccuracyMetersPerSecond?: number;
  bearingDegrees?: number;
}
export type DepartureMotionSnapshot = Pick<MotionSnapshot, 'status' | 'lastSampleAt' | 'lastReceivedAt' | 'deviceId'> & {
  sessionId?: string | null;
  walking?: {
    thresholdMet: boolean;
    stepsSinceBaseline: number | null;
    lastSampleAt: number | null;
    lastReceivedAt: number | null;
  };
};
export interface DepartureAssessmentInput {
  plan: Pick<MorningPlan, 'leaveAt'>;
  now?: number;
  motion?: DepartureMotionSnapshot | null;
  /** Only supply when the plan's origin was explicitly resolved to coordinates. */
  origin?: Coordinates;
  originRadiusMeters?: number;
  fixes?: readonly DepartureLocationFix[];
  /** Defaults to the active motion device/session. Never mixes different recordings. */
  expectedStream?: SensorStream;
}
export interface DepartureAssessment {
  status: 'not-yet-time' | 'time-to-leave' | 'still-inactive' | 'activity-departure-unconfirmed' | 'outside-origin-confirmed';
  reason: string;
  timeToLeave: boolean;
  secondsUntilLeave: number;
  activity: 'active' | 'inactive' | 'unknown';
  location: 'unknown' | 'inside-origin' | 'outside-origin-unconfirmed' | 'outside-origin-confirmed';
  evidence: {
    motionStatus: MotionSnapshot['status'] | 'mismatched';
    walkingObserved: boolean;
    stepDelta: number | null;
    gps: {
      reason: string;
      usableFixCount: number;
      outsideFixCount: number;
      lastFixAt: number | null;
      distanceMeters: number | null;
    };
  };
}

/**
 * Pure assessment of when to prompt and what the phone sensors actually show.
 * Stationary describes the phone, not its owner's posture, sleep, or readiness.
 * GPS confirms at most that this device is outside a configured origin geofence;
 * it cannot establish that the person is on the intended route or has arrived.
 */
export function assessDeparture(input: DepartureAssessmentInput): DepartureAssessment {
  const now = input.now ?? Date.now();
  const leave = Date.parse(input.plan.leaveAt);
  if (!Number.isFinite(now) || !Number.isFinite(leave) || !/(Z|[+-]\d{2}:\d{2})$/.test(input.plan.leaveAt)) {
    throw new Error('Departure assessment requires a valid clock and leaveAt with a timezone offset.');
  }
  const threshold = EXPERIMENTAL_DEPARTURE_THRESHOLDS;
  const motion = input.motion;
  const stream = input.expectedStream ?? (motion?.deviceId && motion.sessionId
    ? { deviceId: motion.deviceId, sessionId: motion.sessionId } : undefined);
  const matches = Boolean(motion?.deviceId && (!input.expectedStream || (
    motion.deviceId === input.expectedStream.deviceId && motion.sessionId === input.expectedStream.sessionId
  )));
  const fresh = (sample: number | null | undefined, received: number | null | undefined, freshMs: number = threshold.motionFreshMs): boolean =>
    typeof sample === 'number' && Number.isFinite(sample) && typeof received === 'number' && Number.isFinite(received)
    && sample >= now - freshMs && sample <= now + threshold.futureToleranceMs
    && received >= now - freshMs && received <= now + threshold.futureToleranceMs;
  const motionFresh = matches && fresh(motion?.lastSampleAt, motion?.lastReceivedAt);
  const motionStatus: DepartureAssessment['evidence']['motionStatus'] = !motion ? 'unavailable'
    : !matches ? 'mismatched'
      : !motionFresh && ['moving', 'stationary'].includes(motion.status) ? 'stale' : motion.status;
  const walkingFresh = matches && fresh(motion?.walking?.lastSampleAt, motion?.walking?.lastReceivedAt, PEDOMETER_FRESH_MS);
  const steps = motion?.walking?.stepsSinceBaseline;
  const stepDelta = walkingFresh && typeof steps === 'number' && Number.isSafeInteger(steps) && steps >= 0 ? steps : null;
  const walkingObserved = Boolean(walkingFresh && stepDelta !== null && stepDelta > 0 && motion?.walking?.thresholdMet);
  const activity = motionStatus === 'moving' || walkingObserved ? 'active'
    : motionStatus === 'stationary' ? 'inactive' : 'unknown';
  const radius = input.originRadiusMeters ?? threshold.originRadiusMeters;
  if (!Number.isFinite(radius) || radius < 50 || radius > 5000) throw new Error('Origin radius must be between 50 and 5000 meters.');
  const gpsResult = assessLocation(input.origin, input.fixes ?? [], stream, now, radius);
  const timeToLeave = now >= leave;
  let status: DepartureAssessment['status'];
  let reason: string;
  if (gpsResult.location === 'outside-origin-confirmed') {
    status = 'outside-origin-confirmed';
    reason = 'Repeated fresh GPS fixes place the phone outside the configured origin. This does not prove the person is on route or has arrived.';
  } else if (!timeToLeave) {
    status = 'not-yet-time';
    reason = 'The calculated leave time has not arrived. Current activity does not establish departure.';
  } else if (activity === 'active') {
    status = 'activity-departure-unconfirmed';
    reason = 'It is time to leave and phone activity is detected. Movement or steps alone do not prove departure.';
  } else if (activity === 'inactive') {
    status = 'still-inactive';
    reason = 'It is time to leave; recent phone motion is below the movement threshold. This does not establish the person’s posture or readiness.';
  } else {
    status = 'time-to-leave';
    reason = 'It is time to leave. Activity evidence is unavailable or stale, so inactivity and departure are unknown.';
  }
  return {
    status, reason, timeToLeave, secondsUntilLeave: Math.ceil((leave - now) / 1000), activity,
    location: gpsResult.location,
    evidence: { motionStatus, walkingObserved, stepDelta, gps: gpsResult.evidence },
  };
}

function assessLocation(origin: Coordinates | undefined, fixes: readonly DepartureLocationFix[], stream: SensorStream | undefined, now: number, radius: number): {
  location: DepartureAssessment['location']; evidence: DepartureAssessment['evidence']['gps'];
} {
  const empty = (reason: string) => ({
    location: 'unknown' as const,
    evidence: { reason, usableFixCount: 0, outsideFixCount: 0, lastFixAt: null, distanceMeters: null },
  });
  if (!origin || !validCoordinates(origin)) return empty('Origin coordinates are unavailable; no geofence is inferred from an address.');
  if (!stream?.deviceId || !stream.sessionId) return empty('A device and recording session are required to assess GPS evidence.');
  if (!fixes.length) return empty('GPS evidence is absent or GPS is off. Location is unknown.');
  const t = EXPERIMENTAL_DEPARTURE_THRESHOLDS;
  const candidates = fixes.filter(fix => fix.deviceId === stream.deviceId && fix.sessionId === stream.sessionId
    && validCoordinates(fix) && Number.isFinite(fix.timestamp) && fix.timestamp >= now - t.gpsFreshMs
    && fix.timestamp <= now + t.futureToleranceMs && Number.isFinite(fix.accuracyMeters)
    && fix.accuracyMeters > 0 && fix.accuracyMeters <= t.maximumAccuracyMeters)
    .sort((left, right) => left.timestamp - right.timestamp);
  // Duplicate timestamps cannot manufacture repeated observations. If conflicting
  // positions share a timestamp, discard that timestamp entirely.
  const counts = new Map<number, number>();
  for (const fix of candidates) counts.set(fix.timestamp, (counts.get(fix.timestamp) ?? 0) + 1);
  const usable = candidates.filter(fix => counts.get(fix.timestamp) === 1);
  if (!usable.length) return empty('No fresh, accurate, uniquely timestamped GPS fixes match the active device and session.');
  const latest = usable.at(-1)!;
  const distanceMeters = distance(origin, latest);
  const base = { usableFixCount: usable.length, outsideFixCount: 0, lastFixAt: latest.timestamp, distanceMeters };
  if (distanceMeters + latest.accuracyMeters <= radius) {
    return { location: 'inside-origin', evidence: { ...base, reason: 'The latest accurate fix places the phone inside the configured origin geofence.' } };
  }
  if (distanceMeters - latest.accuracyMeters <= radius) {
    return { location: 'unknown', evidence: { ...base, reason: 'GPS uncertainty overlaps the origin boundary; departure is unknown.' } };
  }
  const outside: DepartureLocationFix[] = [];
  for (let index = usable.length - 1; index >= 0; index--) {
    const fix = usable[index]!;
    if (distance(origin, fix) - fix.accuracyMeters <= radius) break;
    outside.unshift(fix);
  }
  const confirmed = outside.length >= t.minimumOutsideFixes
    && latest.timestamp - outside[0]!.timestamp >= t.minimumOutsideSpanMs;
  return {
    location: confirmed ? 'outside-origin-confirmed' : 'outside-origin-unconfirmed',
    evidence: { ...base, outsideFixCount: outside.length, reason: confirmed
      ? 'Repeated accurate fixes outside the origin exceed the experimental count and duration thresholds.'
      : 'The phone may be outside the origin, but there are not enough repeated observations over time.' },
  };
}

function validCoordinates(value: Coordinates): boolean {
  return Number.isFinite(value.latitude) && Math.abs(value.latitude) <= 90
    && Number.isFinite(value.longitude) && Math.abs(value.longitude) <= 180;
}
function distance(left: Coordinates, right: Coordinates): number {
  const radians = Math.PI / 180;
  const latitude = (right.latitude - left.latitude) * radians;
  const longitude = (right.longitude - left.longitude) * radians;
  const a = Math.sin(latitude / 2) ** 2 + Math.cos(left.latitude * radians) * Math.cos(right.latitude * radians) * Math.sin(longitude / 2) ** 2;
  return 6_371_000 * 2 * Math.atan2(Math.sqrt(Math.min(1, a)), Math.sqrt(Math.max(0, 1 - a)));
}
