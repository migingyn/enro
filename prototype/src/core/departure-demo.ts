import { assessDeparture, type DepartureLocationFix, type DepartureMotionSnapshot } from './departure.js';

/** Explicitly synthetic, fixed-clock exercise. Never writes to live sensor or scheduling state. */
export function departureDemo(input: {
  scenario: 'stationary' | 'pickup' | 'shaking' | 'walking' | 'departed-gps';
  travelMinutes: number; clockMinutes: number;
}) {
  const bufferMinutes = 10;
  const preparationMinutes = 45;
  const leaveClockMinutes = 30 - input.travelMinutes - bufferMinutes;
  const epoch = Date.parse('2026-10-24T18:00:00-07:00');
  const now = epoch + input.clockMinutes * 60_000;
  const stream = { deviceId: 'synthetic-demo-phone', sessionId: 'synthetic-demo-recording' };
  const isWalking = input.scenario === 'walking';
  const evidence: DepartureMotionSnapshot = {
    ...stream, status: ['walking', 'shaking'].includes(input.scenario) ? 'moving' : 'stationary',
    lastSampleAt: now, lastReceivedAt: now,
    walking: { thresholdMet: isWalking, stepsSinceBaseline: isWalking ? 6 : 0, lastSampleAt: now, lastReceivedAt: now },
  };
  // Arbitrary synthetic coordinates, not a geocode of the user's address.
  const origin = { latitude: 0, longitude: 0 };
  const fixes: DepartureLocationFix[] = input.scenario === 'departed-gps' ? [20_000, 10_000, 0].map(age => ({
    ...stream, latitude: 0.003, longitude: 0, accuracyMeters: 5, timestamp: now - age,
  })) : [];
  return {
    ...input, source: 'synthetic-demo', routeSource: 'manual-assumption', callsEnabled: false,
    origin: 'La Jolla Crossroads', destination: 'Price Center, UC San Diego',
    appointmentClockMinutes: 30, bufferMinutes, preparationMinutes,
    leaveClockMinutes, wakeClockMinutes: leaveClockMinutes - preparationMinutes,
    pickupPossible: input.scenario === 'pickup', walkingSteps: isWalking ? 6 : 0,
    assessment: assessDeparture({ plan: { leaveAt: new Date(epoch + leaveClockMinutes * 60_000).toISOString() },
      now, motion: evidence, origin, fixes, expectedStream: stream }),
  };
}
