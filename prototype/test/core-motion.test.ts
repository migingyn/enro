import assert from 'node:assert/strict';
import { test } from 'node:test';
import { MotionTracker, PEDOMETER_FRESH_MS } from '../src/core/motion.js';

const now = Date.parse('2026-10-24T21:00:10Z');
const sample = (at: number, x = 2, name = 'accelerometer') => ({ name, time: at * 1_000_000, values: { x, y: 0, z: 0 } });
const batch = (messageId: number, payload: unknown[], deviceId = 'phone', sessionId = 'recording') => ({ messageId, deviceId, sessionId, payload });
const steps = (at: number, count: number) => ({ name: 'pedometer', time: at * 1_000_000, values: { steps: count } });
const gravity = (at: number, x: number, y: number, z: number) => ({ name: 'gravity', time: at * 1_000_000, values: { x, y, z } });

test('starts unavailable and accepts official Sensor Logger nanosecond payloads', () => {
  const motion = new MotionTracker();
  assert.equal(motion.snapshot(now).status, 'unavailable');
  assert.deepEqual(motion.ingest(batch(0, [sample(now - 1500), sample(now - 200)]), now), { accepted: 2, ignored: 0 });
  const snapshot = motion.snapshot(now);
  assert.equal(snapshot.status, 'moving');
  assert.equal(snapshot.sampleCount, 2);
  assert.equal(snapshot.movementSamples, 2);
  assert.equal(snapshot.movementSeconds, 2);
  assert.equal(snapshot.lastSampleAt, now - 200);
  assert.equal(snapshot.latencyMs, 200);
  assert.equal(snapshot.deviceId, 'phone');
  assert.equal(snapshot.source, 'sensor-logger');
});

test('rotation and a single shake do not establish sustained motion', () => {
  const motion = new MotionTracker();
  assert.deepEqual(motion.ingest(batch(0, [sample(now - 3000, 100, 'gyroscope'), sample(now - 2000, 0.05), sample(now - 1900, 0.02)]), now), { accepted: 2, ignored: 1 });
  assert.equal(motion.snapshot(now).status, 'stationary');
  motion.ingest(batch(1, [sample(now - 1001, 12), sample(now - 999, 12)]), now);
  assert.equal(motion.snapshot(now).movementSeconds, 2);
  assert.equal(motion.snapshot(now).status, 'stationary');
});

test('motion status expires even while fresh stationary readings continue', () => {
  const motion = new MotionTracker();
  motion.ingest(batch(0, [sample(now - 1500), sample(now - 200)]), now);
  assert.equal(motion.snapshot(now + 4000).status, 'stationary');
  assert.equal(motion.snapshot(now + 4000).sampleCount, 2);
  assert.equal(motion.snapshot(now + 5000).status, 'stale');
  motion.ingest(batch(1, [sample(now + 5500, 0)]), now + 5500);
  assert.equal(motion.snapshot(now + 5500).status, 'stationary');
});

test('ignores stale/future/malformed samples and epoch seconds or milliseconds', () => {
  const motion = new MotionTracker();
  const invalid = [sample(now - 5001), sample(now + 2001), sample(now, NaN), sample(now, 201),
    { name: 'accelerometer', time: now, values: { x: 2, y: 0, z: 0 } },
    { name: 'accelerometer', time: now * 1_000_000, values: { x: 1 } }, null];
  assert.deepEqual(motion.ingest(batch(0, invalid), now), { accepted: 0, ignored: invalid.length });
  assert.equal(motion.snapshot(now).status, 'unavailable');
});

test('ignores duplicate messages, lower message IDs, duplicate and out-of-order sample times', () => {
  const motion = new MotionTracker();
  const body = batch(5, [sample(now - 1500)]);
  motion.ingest(body, now);
  assert.deepEqual(motion.ingest(body, now), { accepted: 0, ignored: 1 });
  assert.deepEqual(motion.ingest(batch(4, [sample(now - 200)]), now), { accepted: 0, ignored: 1 });
  assert.deepEqual(motion.ingest(batch(6, [sample(now - 1500), sample(now - 1600), sample(now - 200)]), now), { accepted: 1, ignored: 2 });
  assert.equal(motion.snapshot(now).sampleCount, 2);
});

test('call-start scoping excludes evidence from before a call', () => {
  const motion = new MotionTracker();
  motion.ingest(batch(0, [sample(now - 1500), sample(now - 200)]), now);
  assert.equal(motion.snapshot(now, now - 100).status, 'unavailable');
  motion.ingest(batch(1, [sample(now + 100)]), now + 100);
  assert.equal(motion.snapshot(now + 100, now - 100).status, 'stationary');
  assert.equal(motion.snapshot(now + 100, now - 100).sampleCount, 1);
  motion.ingest(batch(2, [sample(now + 1200)]), now + 1200);
  assert.equal(motion.snapshot(now + 1200, now - 100).status, 'moving');
});

test('never combines readings from different devices or sessions', () => {
  const motion = new MotionTracker();
  motion.ingest(batch(0, [sample(now - 1500)]), now);
  motion.ingest(batch(0, [sample(now - 200)], 'other-phone'), now);
  assert.equal(motion.snapshot(now).status, 'stationary');
  assert.equal(motion.snapshot(now).deviceId, 'other-phone');
  assert.equal(motion.snapshot(now).sampleCount, 1);
  motion.ingest(batch(0, [sample(now)], 'other-phone', 'new-session'), now);
  assert.equal(motion.snapshot(now).sampleCount, 1);
});

test('rejects malformed and oversized batches without adding evidence', () => {
  const motion = new MotionTracker();
  for (const body of [null, {}, [], { ...batch(0, [sample(now)]), messageId: -1 }, { ...batch(0, [sample(now)]), sessionId: '' }]) {
    assert.equal(motion.ingest(body, now).accepted, 0);
  }
  assert.deepEqual(motion.ingest(batch(0, Array.from({ length: 2001 }, () => sample(now))), now), { accepted: 0, ignored: 2001 });
  assert.equal(motion.snapshot(now).sampleCount, 0);
});

test('bounds retained readings and retains stale metadata after history expires', () => {
  const motion = new MotionTracker();
  for (let i = 0; i < 8; i++) {
    const at = now + i * 1000;
    motion.ingest(batch(i, Array.from({ length: 2000 }, (_, j) => sample(at + j / 2, 0))), at + 1000);
  }
  assert.ok(motion.snapshot(now + 9000).sampleCount <= 12_000);
  assert.equal(motion.snapshot(now + 200_000).sampleCount, 0);
  assert.equal(motion.snapshot(now + 200_000).status, 'stale');
});

test('acceleration and gravity without pedometer counts cannot meet the counter threshold', () => {
  const motion = new MotionTracker();
  motion.ingest(batch(0, [sample(now, 12), gravity(now, 0, 0, 9.81), steps(now, 100)]), now);
  motion.ingest(batch(1, [sample(now + 1500, 15), gravity(now + 1500, 0, 9.81, 0), steps(now + 1500, 100)]), now + 1500);
  const evidence = motion.snapshot(now + 1500, now);
  assert.equal(evidence.status, 'moving');
  assert.equal(evidence.gravity.pickupPossible, true);
  assert.equal(evidence.gravity.orientationChangeDegrees, 90);
  assert.equal(evidence.walking.stepsSinceBaseline, 0);
  assert.equal(evidence.walking.thresholdMet, false);
  assert.equal(evidence.walking.status, 'insufficient');
});

test('requires new steps after the first post-answer baseline, not pre-call history', () => {
  const motion = new MotionTracker();
  motion.ingest(batch(0, [steps(now - 4000, 80), steps(now - 1000, 100)]), now - 1000);
  assert.equal(motion.snapshot(now, now).walking.status, 'baseline-needed');
  motion.ingest(batch(1, [steps(now + 100, 108)]), now + 100);
  let evidence = motion.snapshot(now + 100, now).walking;
  assert.equal(evidence.baselineSteps, 108);
  assert.equal(evidence.baselineAt, now + 100);
  assert.equal(evidence.stepsSinceBaseline, 0);
  assert.equal(evidence.thresholdMet, false);
  motion.ingest(batch(2, [steps(now + 3100, 113)]), now + 3100);
  assert.equal(motion.snapshot(now + 3100, now).walking.thresholdMet, false);
  motion.ingest(batch(3, [steps(now + 4100, 114)]), now + 4100);
  evidence = motion.snapshot(now + 4100, now).walking;
  assert.equal(evidence.stepsSinceBaseline, 6);
  assert.equal(evidence.threshold, 6);
  assert.equal(evidence.thresholdMet, true);
  assert.equal(evidence.status, 'step-threshold-met');
  assert.equal(evidence.scopedTo, now);
});

test('unscoped dashboards never establish per-call walking and clock skew cannot import pre-answer steps', () => {
  const motion = new MotionTracker();
  motion.ingest(batch(0, [steps(now + 500, 10)]), now - 100);
  motion.ingest(batch(1, [steps(now + 1500, 16)]), now + 1500);
  assert.equal(motion.snapshot(now + 1500).walking.status, 'baseline-needed');
  assert.equal(motion.snapshot(now + 1500).walking.thresholdMet, false);
  const evidence = motion.snapshot(now + 1500, now).walking;
  assert.equal(evidence.baselineSteps, 16);
  assert.equal(evidence.stepsSinceBaseline, 0);
  assert.equal(evidence.thresholdMet, false);
});

test('stale pedometer data cannot confirm walking even with fresh acceleration', () => {
  const motion = new MotionTracker();
  motion.ingest(batch(0, [steps(now, 10)]), now);
  motion.ingest(batch(1, [steps(now + 3000, 16)]), now + 3000);
  assert.equal(motion.snapshot(now + 3000, now).walking.thresholdMet, true);
  const later = now + 3000 + PEDOMETER_FRESH_MS + 1;
  motion.ingest(batch(2, [sample(later - 1500), sample(later)]), later);
  const evidence = motion.snapshot(later, now);
  assert.equal(evidence.status, 'moving');
  assert.equal(evidence.walking.status, 'stale');
  assert.equal(evidence.walking.thresholdMet, false);
  assert.equal(evidence.sensors.pedometer.status, 'stale');
  assert.deepEqual(motion.ingest(batch(3, [steps(now + 3001, 22)]), later + 1), { accepted: 0, ignored: 1 });
});

test('duplicate messages and out-of-order pedometer times cannot inflate step progress', () => {
  const motion = new MotionTracker();
  motion.ingest(batch(0, [steps(now, 10)]), now);
  const payload = batch(1, [steps(now + 1000, 12)]);
  motion.ingest(payload, now + 1000);
  assert.deepEqual(motion.ingest(payload, now + 1000), { accepted: 0, ignored: 1 });
  assert.deepEqual(motion.ingest(batch(2, [steps(now + 1000, 20), steps(now + 500, 22)]), now + 1000), { accepted: 0, ignored: 2 });
  assert.equal(motion.snapshot(now + 1000, now).walking.stepsSinceBaseline, 2);
  assert.equal(motion.snapshot(now + 1000, now).walking.thresholdMet, false);
});

test('counter resets discard prior progress and require new steps from the reset baseline', () => {
  const motion = new MotionTracker();
  motion.ingest(batch(0, [steps(now, 100)]), now);
  motion.ingest(batch(1, [steps(now + 2500, 105)]), now + 2500);
  motion.ingest(batch(2, [steps(now + 3000, 0)]), now + 3000);
  let evidence = motion.snapshot(now + 3000, now).walking;
  assert.equal(evidence.status, 'counter-reset');
  assert.equal(evidence.resetCount, 1);
  assert.equal(evidence.stepsSinceBaseline, 0);
  assert.equal(evidence.baselineSteps, 0);
  motion.ingest(batch(3, [steps(now + 6000, 6)]), now + 6000);
  evidence = motion.snapshot(now + 6000, now).walking;
  assert.equal(evidence.stepsSinceBaseline, 6);
  assert.equal(evidence.thresholdMet, true);
});

test('long feed gaps and implausible cumulative jumps reset the walking baseline', () => {
  for (const discontinuity of [steps(now + 20_000, 16), steps(now + 1000, 2000)]) {
    const motion = new MotionTracker();
    motion.ingest(batch(0, [steps(now, 10)]), now);
    const at = discontinuity.time / 1_000_000;
    motion.ingest(batch(1, [discontinuity]), at);
    const evidence = motion.snapshot(at, now).walking;
    assert.equal(evidence.status, 'counter-reset');
    assert.equal(evidence.stepsSinceBaseline, 0);
    assert.equal(evidence.thresholdMet, false);
  }
});

test('a recording/session switch cannot reuse steps or revive an older stream', () => {
  const motion = new MotionTracker();
  motion.ingest(batch(0, [steps(now, 10)]), now);
  motion.ingest(batch(1, [steps(now + 3000, 16)]), now + 3000);
  assert.equal(motion.snapshot(now + 3000, now).walking.thresholdMet, true);
  motion.ingest(batch(0, [steps(now + 4000, 200)], 'phone', 'new-recording'), now + 4000);
  const evidence = motion.snapshot(now + 4000, now);
  assert.equal(evidence.sessionId, 'new-recording');
  assert.equal(evidence.walking.stepsSinceBaseline, 0);
  assert.equal(evidence.walking.thresholdMet, false);
  assert.deepEqual(motion.ingest(batch(2, [steps(now + 5000, 22)]), now + 5000), { accepted: 0, ignored: 1 });
  assert.equal(motion.snapshot(now + 5000, now).sessionId, 'new-recording');
});

test('different sensors at the same time are accepted independently; activity stays unknown', () => {
  const motion = new MotionTracker();
  const activity = { name: 'activity', time: now * 1_000_000, values: { walking: true, confidence: 'certain', secretValue: 'do-not-retain' } };
  assert.deepEqual(motion.ingest(batch(0, [sample(now, 0), steps(now, 100), gravity(now, 0, 0, 9.81), activity]), now), { accepted: 4, ignored: 0 });
  const evidence = motion.snapshot(now, now);
  assert.equal(evidence.sensors.accelerometer.status, 'live');
  assert.equal(evidence.sensors.pedometer.status, 'live');
  assert.equal(evidence.sensors.gravity.status, 'live');
  assert.equal(evidence.sensors.activity.status, 'live');
  assert.equal(evidence.activity.status, 'unknown-schema');
  assert.equal(evidence.activity.fieldTypes['values.walking'], 'boolean');
  assert.equal(evidence.activity.fieldTypes['values.confidence'], 'string');
  assert.equal(JSON.stringify(evidence).includes('do-not-retain'), false);
  assert.equal(evidence.walking.thresholdMet, false);
  assert.equal(motion.snapshot(now + PEDOMETER_FRESH_MS + 1).activity.status, 'stale');
});

test('location is ignored and raw coordinates never enter motion snapshots', () => {
  const motion = new MotionTracker();
  const location = { name: 'location', time: now * 1_000_000, values: { latitude: 32.8801, longitude: -117.234, horizontalAccuracy: 5 } };
  assert.deepEqual(motion.ingest(batch(0, [location]), now), { accepted: 0, ignored: 1 });
  assert.equal(JSON.stringify(motion.snapshot(now)).includes('32.8801'), false);
  assert.equal(motion.snapshot(now).source, null);
});

test('invalid pedometer counters and experimental thresholds are rejected', () => {
  for (const walkingThreshold of [4, 11, 5.5, NaN]) assert.throws(() => new MotionTracker({ walkingThreshold }), /5 to 10/);
  const motion = new MotionTracker({ walkingThreshold: 10 });
  assert.deepEqual(motion.ingest(batch(0, [-1, 2.5, NaN, Infinity, 1_000_000_001].map(count => steps(now, count))), now), { accepted: 0, ignored: 5 });
  motion.ingest(batch(1, [steps(now, 10)]), now);
  motion.ingest(batch(2, [steps(now + 4000, 19)]), now + 4000);
  assert.equal(motion.snapshot(now + 4000, now).walking.thresholdMet, false);
  motion.ingest(batch(3, [steps(now + 5000, 20)]), now + 5000);
  assert.equal(motion.snapshot(now + 5000, now).walking.thresholdMet, true);
});

test('reported Activity labels remain bounded and cannot independently satisfy the step gate', () => {
  const motion = new MotionTracker();
  motion.ingest(batch(0, [{ name: 'activity', time: now * 1_000_000, values: { activity: 'Walking', extra: 'never-store-this-value' } }]), now);
  const evidence = motion.snapshot(now, now);
  assert.equal(evidence.activity.reportedActivity, 'Walking');
  assert.equal(evidence.activity.status, 'available');
  assert.equal(evidence.activity.classification, 'walking');
  assert.equal(evidence.walking.activityCorroborated, false);
  assert.equal(evidence.walking.thresholdMet, false);
  assert.equal(JSON.stringify(evidence).includes('never-store-this-value'), false);
});

test('invalid Activity labels and wrong nesting are not retained', () => {
  const invalid: unknown[] = [undefined, null, 1, true, ['walking'], { state: 'walking' }, '', ' ', 'a'.repeat(41), '<walking>', 'walking\n', 'walking/secret'];
  for (const activity of invalid) {
    const motion = new MotionTracker();
    motion.ingest(batch(0, [{ name: 'activity', time: now * 1_000_000, values: { activity } }]), now);
    assert.equal(motion.snapshot(now).activity.reportedActivity, null);
  }
  const motion = new MotionTracker();
  motion.ingest(batch(0, [{ name: 'activity', time: now * 1_000_000, activity: 'Walking', values: [] }]), now);
  assert.equal(motion.snapshot(now).activity.reportedActivity, null);
});

test('observed sensor names distinguish absent pedometer from rejected payloads without keeping values', () => {
  const motion = new MotionTracker();
  const payload = [sample(now, 0), { name: 'pedometer', time: now * 1_000_000, values: { steps: 'wrong-shape' } },
    { name: 'other sensor', values: { secret: 'never-store-this-value' } }];
  motion.ingest(batch(0, payload), now);
  const evidence = motion.snapshot(now);
  assert.deepEqual(evidence.observedSensors, ['accelerometer', 'pedometer', 'other sensor']);
  assert.equal(evidence.sensors.pedometer.status, 'unavailable');
  assert.equal(JSON.stringify(evidence).includes('never-store-this-value'), false);
  evidence.observedSensors.push('cannot-mutate-tracker');
  assert.equal(motion.snapshot(now).observedSensors.includes('cannot-mutate-tracker'), false);
});

test('observed sensor names are sanitized, bounded, and isolated between recordings', () => {
  const motion = new MotionTracker();
  motion.ingest(batch(0, [{ name: 'sensor\n<script>' }, { name: 'x'.repeat(100) },
    ...Array.from({ length: 30 }, (_, index) => ({ name: 'sensor-' + index }))]), now);
  const names = motion.snapshot(now).observedSensors;
  assert.equal(names.length, 16);
  assert.equal(names.every(name => name.length <= 64 && /^[A-Za-z0-9 _.\-]+$/.test(name)), true);
  motion.ingest(batch(0, [sample(now + 1000)], 'phone', 'new-recording'), now + 1000);
  assert.deepEqual(motion.snapshot(now + 1000).observedSensors, ['accelerometer']);
});

test('Activity normalization accepts only exact allowlisted labels with case folding', () => {
  const labels = [['stationary', 'stationary'], ['WALKING', 'walking'], ['Running', 'running'],
    ['cycling', 'cycling'], ['automotive', 'driving'], ['driving', 'driving'], ['unknown', 'unknown']];
  for (const [label, expected] of labels) {
    const motion = new MotionTracker();
    motion.ingest(batch(0, [{ name: 'activity', time: now * 1_000_000, values: { activity: label } }]), now);
    const evidence = motion.snapshot(now, now);
    assert.equal(evidence.activity.classification, expected);
    assert.equal(evidence.activity.status, 'available');
    assert.equal(evidence.walking.activityCorroborated, false);
    assert.equal(evidence.walking.thresholdMet, false);
  }
  for (const label of ['not walking', 'walking running', 'walking_and_running', 'jogging', 'in_vehicle', 'stationary-ish']) {
    const motion = new MotionTracker();
    motion.ingest(batch(0, [{ name: 'activity', time: now * 1_000_000, values: { activity: label } }]), now);
    const evidence = motion.snapshot(now, now);
    assert.equal(evidence.activity.reportedActivity, label);
    assert.equal(evidence.activity.classification, 'unrecognized');
    assert.equal(evidence.activity.status, 'unknown-schema');
    assert.equal(evidence.walking.activityCorroborated, false);
  }
});

test('Activity corroboration requires counts and overlapping post-baseline evidence in the current stream', () => {
  const motion = new MotionTracker();
  const activity = (at: number) => ({ name: 'activity', time: at * 1_000_000, values: { activity: 'walking' } });
  motion.ingest(batch(0, [activity(now - 1000)]), now - 1000);
  motion.ingest(batch(1, [steps(now, 100)]), now);
  motion.ingest(batch(2, [steps(now + 3000, 107)]), now + 3000);
  assert.equal(motion.snapshot(now + 3000, now).walking.thresholdMet, true);
  assert.equal(motion.snapshot(now + 3000, now).walking.activityCorroborated, false);
  motion.ingest(batch(3, [activity(now + 3000)]), now + 3000);
  assert.equal(motion.snapshot(now + 3000, now).walking.activityCorroborated, true);
  assert.equal(motion.snapshot(now + 3000).walking.activityCorroborated, false);
  assert.equal(motion.snapshot(now + 3001 + PEDOMETER_FRESH_MS, now).walking.activityCorroborated, false);
  motion.ingest(batch(0, [steps(now + 20_000, 100)], 'phone', 'new-recording'), now + 20_000);
  assert.equal(motion.snapshot(now + 20_000, now).walking.activityCorroborated, false);
});

test('pre-reset walking labels and labels far from the counter update cannot corroborate shaking counts', () => {
  const motion = new MotionTracker();
  const activity = (at: number) => ({ name: 'activity', time: at * 1_000_000, values: { activity: 'walking' } });
  motion.ingest(batch(0, [steps(now, 100), activity(now)]), now);
  motion.ingest(batch(1, [steps(now + 3000, 0)]), now + 3000);
  motion.ingest(batch(2, [steps(now + 6000, 7)]), now + 6000);
  assert.equal(motion.snapshot(now + 6000, now).walking.thresholdMet, true);
  assert.equal(motion.snapshot(now + 6000, now).walking.activityCorroborated, false);
  motion.ingest(batch(3, [activity(now + 12_000)]), now + 12_000);
  assert.equal(motion.snapshot(now + 12_000, now).walking.activityCorroborated, false);
  motion.ingest(batch(4, [steps(now + 12_000, 8)]), now + 12_000);
  assert.equal(motion.snapshot(now + 12_000, now).walking.activityCorroborated, true);
});

test('counter increments alone remain diagnostic when Activity is missing, stationary, or unrecognized', () => {
  for (const label of [null, 'stationary', 'unrecognized-label']) {
    const motion = new MotionTracker();
    motion.ingest(batch(0, [steps(now, 100)]), now);
    const payload: unknown[] = [steps(now + 3000, 106)];
    if (label) payload.push({ name: 'activity', time: (now + 3000) * 1_000_000, values: { activity: label } });
    motion.ingest(batch(1, payload), now + 3000);
    const evidence = motion.snapshot(now + 3000, now);
    assert.equal(evidence.walking.thresholdMet, true);
    assert.equal(evidence.walking.activityCorroborated, false);
  }
});
