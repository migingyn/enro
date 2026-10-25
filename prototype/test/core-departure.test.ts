import test from 'node:test';
import assert from 'node:assert/strict';
import { assessDeparture, EXPERIMENTAL_DEPARTURE_THRESHOLDS, type DepartureLocationFix, type DepartureMotionSnapshot } from '../src/core/departure.js';

const now = Date.parse('2026-10-24T18:00:00-07:00');
const plan = { leaveAt: new Date(now).toISOString() };
const origin = { latitude: 32.88, longitude: -117.23 };
const stream = { deviceId: 'phone-a', sessionId: 'recording-a' };
const stationary: DepartureMotionSnapshot = { ...stream, status: 'stationary', lastSampleAt: now, lastReceivedAt: now };
const fix = (age: number, latitude = origin.latitude + 0.003, overrides: Partial<DepartureLocationFix> = {}): DepartureLocationFix => ({
  ...stream, latitude, longitude: origin.longitude, accuracyMeters: 10, timestamp: now - age, ...overrides,
});

test('hypothetical 6PM context uses computed leave time, not event time or phone posture', () => {
  const later = assessDeparture({ plan: { leaveAt: new Date(now + 15 * 60_000).toISOString() }, now, motion: stationary });
  assert.equal(later.status, 'not-yet-time');
  assert.equal(later.secondsUntilLeave, 900);
  assert.equal(later.location, 'unknown');
  const due = assessDeparture({ plan, now, motion: stationary });
  assert.equal(due.status, 'still-inactive');
  assert.equal(due.timeToLeave, true);
  assert.match(due.reason, /phone motion/);
  assert.match(due.reason, /does not establish/);
});

test('absent and stale sensors mean unknown activity rather than still inactive', () => {
  assert.equal(assessDeparture({ plan, now }).status, 'time-to-leave');
  const result = assessDeparture({ plan, now, motion: { ...stationary, lastSampleAt: now - 6000 } });
  assert.equal(result.activity, 'unknown');
  assert.equal(result.evidence.motionStatus, 'stale');
  assert.equal(result.location, 'unknown');
});

test('movement and fresh step evidence show activity, never departure without GPS', () => {
  const moving = assessDeparture({ plan, now, motion: { ...stationary, status: 'moving' }, origin });
  assert.equal(moving.status, 'activity-departure-unconfirmed');
  assert.equal(moving.location, 'unknown');
  assert.match(moving.evidence.gps.reason, /GPS is off/);
  const walking = assessDeparture({ plan, now, motion: { ...stationary, walking: {
    thresholdMet: true, stepsSinceBaseline: 6, lastSampleAt: now, lastReceivedAt: now,
  } } });
  assert.equal(walking.activity, 'active');
  assert.equal(walking.evidence.walkingObserved, true);
  assert.equal(walking.location, 'unknown');
});

test('stale or mismatched step/motion evidence cannot establish current activity', () => {
  const stale = assessDeparture({ plan, now, motion: { ...stationary, status: 'unavailable', walking: {
    thresholdMet: true, stepsSinceBaseline: 6, lastSampleAt: now - 16000, lastReceivedAt: now,
  } } });
  assert.equal(stale.activity, 'unknown');
  assert.equal(stale.evidence.stepDelta, null);
  const different = assessDeparture({ plan, now, motion: { ...stationary, status: 'moving' }, expectedStream: { ...stream, sessionId: 'other' } });
  assert.equal(different.evidence.motionStatus, 'mismatched');
  assert.equal(different.activity, 'unknown');
});

test('pedometer evidence has a fifteen-second window and permits an unavailable baseline', () => {
  const recent = assessDeparture({ plan, now, motion: { ...stationary, status: 'unavailable', walking: {
    thresholdMet: true, stepsSinceBaseline: 6, lastSampleAt: now - 10000, lastReceivedAt: now - 10000,
  } } });
  assert.equal(recent.activity, 'active');
  const missing = assessDeparture({ plan, now, motion: { ...stationary, status: 'unavailable', walking: {
    thresholdMet: false, stepsSinceBaseline: null, lastSampleAt: now, lastReceivedAt: now,
  } } });
  assert.equal(missing.activity, 'unknown');
  assert.equal(missing.evidence.stepDelta, null);
});

test('three fresh accurate outside fixes over ten seconds establish only outside-origin', () => {
  const result = assessDeparture({ plan, now, motion: stationary, origin, fixes: [fix(12000), fix(6000), fix(0)] });
  assert.equal(result.status, 'outside-origin-confirmed');
  assert.equal(result.location, 'outside-origin-confirmed');
  assert.equal(result.evidence.gps.outsideFixCount, 3);
  assert.match(result.reason, /does not prove.*on route/);
});

test('GPS requires explicitly resolved origin and a device/session', () => {
  const fixes = [fix(12000), fix(6000), fix(0)];
  assert.equal(assessDeparture({ plan, now, motion: stationary, fixes }).location, 'unknown');
  assert.equal(assessDeparture({ plan, now, origin, fixes }).location, 'unknown');
  assert.equal(assessDeparture({ plan, now, origin, fixes, expectedStream: stream }).location, 'outside-origin-confirmed');
});

test('single fixes, short bursts, stale fixes and low accuracy never confirm outside-origin', () => {
  for (const fixes of [[fix(0)], [fix(2000), fix(1000), fix(0)], [fix(60000), fix(55000), fix(50000)], [fix(12000), fix(6000), fix(0)].map(item => ({ ...item, accuracyMeters: 80 }))]) {
    assert.notEqual(assessDeparture({ plan, now, motion: stationary, origin, fixes }).location, 'outside-origin-confirmed');
  }
});

test('geofence considers GPS accuracy radius instead of treating estimated center as certain', () => {
  const boundary = origin.latitude + 130 / 111195;
  const result = assessDeparture({ plan, now, motion: stationary, origin, fixes: [fix(12000, boundary, { accuracyMeters: 20 }), fix(6000, boundary, { accuracyMeters: 20 }), fix(0, boundary, { accuracyMeters: 20 })] });
  assert.equal(result.location, 'unknown');
  assert.match(result.evidence.gps.reason, /uncertainty/);
});

test('recordings/devices cannot combine outside fix counts, and repeated timestamps do not count', () => {
  const mixed = [fix(12000, undefined, { deviceId: 'other' }), fix(6000, undefined, { sessionId: 'other' }), fix(0)];
  assert.equal(assessDeparture({ plan, now, motion: stationary, origin, fixes: mixed }).location, 'outside-origin-unconfirmed');
  assert.notEqual(assessDeparture({ plan, now, motion: stationary, origin, fixes: [fix(12000), fix(12000), fix(0), fix(0)] }).location, 'outside-origin-confirmed');
});

test('latest accurate inside observation resets outside confirmation', () => {
  const result = assessDeparture({ plan, now, motion: stationary, origin, fixes: [fix(20000), fix(15000), fix(10000), fix(0, origin.latitude)] });
  assert.equal(result.location, 'inside-origin');
  assert.equal(result.status, 'still-inactive');
  assert.equal(result.evidence.gps.outsideFixCount, 0);
});

test('experimental thresholds are explicit and malformed clocks fail closed', () => {
  assert.equal(EXPERIMENTAL_DEPARTURE_THRESHOLDS.minimumOutsideFixes, 3);
  assert.throws(() => assessDeparture({ plan: { leaveAt: 'tomorrow at six' }, now }), /valid clock/);
  assert.throws(() => assessDeparture({ plan, now: NaN }), /valid clock/);
});
