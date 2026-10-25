import test from 'node:test';
import assert from 'node:assert/strict';
import { LocationTracker } from '../src/core/location.js';

const now = Date.parse('2026-10-24T18:00:00-07:00');
const stream = { deviceId: 'phone-a', sessionId: 'recording-a' };
const location = (at = now, values: Record<string, unknown> = {}) => ({
  name: 'location', time: at * 1_000_000,
  values: { latitude: 32.88, longitude: -117.23, horizontalAccuracy: 10, ...values },
});
const envelope = (payload = [location()], messageId = 1, overrides = {}) => ({ ...stream, messageId, payload, ...overrides });

test('GPS is disabled by default and disabling drops retained coordinates', () => {
  const tracker = new LocationTracker();
  assert.equal(tracker.ingest(envelope(), now).accepted, 0);
  assert.deepEqual(tracker.fixes(now), []);
  tracker.setEnabled(true);
  assert.equal(tracker.ingest(envelope(), now).accepted, 1);
  tracker.setEnabled(false);
  assert.deepEqual(tracker.fixes(now), []);
  tracker.setEnabled(true);
  assert.deepEqual(tracker.fixes(now), []);
});

test('official GPS fields normalize nanoseconds to milliseconds and return defensive copies', () => {
  const tracker = new LocationTracker();
  tracker.setEnabled(true);
  tracker.ingest(envelope(), now);
  const [fix] = tracker.fixes(now, stream);
  assert.deepEqual(fix, { ...stream, latitude: 32.88, longitude: -117.23, accuracyMeters: 10, timestamp: now });
  fix!.latitude = 0;
  assert.equal(tracker.fixes(now)[0]?.latitude, 32.88);
});

test('non-GPS sensors, invalid coordinates, absent accuracy, invalid envelopes and stale/future fixes are ignored', () => {
  const tracker = new LocationTracker();
  tracker.setEnabled(true);
  const samples = [
    { ...location(), name: 'accelerometer' }, location(now, { latitude: 91 }), location(now, { longitude: -181 }),
    location(now, { horizontalAccuracy: -1 }), location(now, { horizontalAccuracy: undefined }),
    location(now - 46000), location(now + 3000),
  ];
  assert.equal(tracker.ingest(envelope(samples), now).accepted, 0);
  assert.equal(tracker.ingest(envelope([location()], 2, { deviceId: '' }), now).accepted, 0);
  assert.deepEqual(tracker.fixes(now), []);
});

test('duplicate messages and sample timestamps cannot manufacture repeated GPS evidence', () => {
  const tracker = new LocationTracker();
  tracker.setEnabled(true);
  assert.equal(tracker.ingest(envelope([location(now - 1000), location(now - 1000), location(now)]), now).accepted, 2);
  assert.equal(tracker.ingest(envelope(), now).accepted, 0);
  assert.equal(tracker.ingest(envelope([location(now)], 2), now).accepted, 0);
  assert.equal(tracker.fixes(now).length, 2);
});

test('recordings and devices do not combine GPS histories', () => {
  const tracker = new LocationTracker();
  tracker.setEnabled(true);
  tracker.ingest(envelope([location(now - 12000), location(now - 6000)]), now);
  tracker.ingest(envelope([location(now)], 1, { sessionId: 'recording-b' }), now);
  assert.deepEqual(tracker.fixes(now, stream), []);
  assert.equal(tracker.fixes(now, { ...stream, sessionId: 'recording-b' }).length, 1);
  tracker.ingest(envelope([location(now + 1000)], 1, { deviceId: 'phone-b' }), now + 1000);
  assert.equal(tracker.fixes(now + 1000).length, 1);
  assert.equal(tracker.fixes(now + 1000)[0]?.deviceId, 'phone-b');
});

test('GPS expires and clear removes history without requiring persistent storage', () => {
  const tracker = new LocationTracker();
  tracker.setEnabled(true);
  tracker.ingest(envelope(), now);
  assert.deepEqual(tracker.fixes(now + 46000), []);
  tracker.ingest(envelope([location(now + 50000)], 2), now + 50000);
  tracker.clear();
  assert.deepEqual(tracker.fixes(now + 50000), []);
});


test('location speed and heading accept only valid documented measurements', () => {
  const tracker = new LocationTracker(); tracker.setEnabled(true);
  tracker.ingest(envelope([location(now, {speed: 3, speedAccuracy: 0.2, bearing: 90})]), now);
  assert.equal(tracker.fixes(now)[0]?.speedMetersPerSecond, 3);
  assert.equal(tracker.fixes(now)[0]?.speedAccuracyMetersPerSecond, 0.2);
  assert.equal(tracker.fixes(now)[0]?.bearingDegrees, 90);
  tracker.ingest(envelope([location(now + 1000, {speed: -1, speedAccuracy: -1, bearing: -1})], 2), now + 1000);
  assert.equal(tracker.fixes(now + 1000).at(-1)?.speedMetersPerSecond, undefined);
});

test('an old recording cannot take over GPS evidence after a newer recording starts', () => {
  const tracker = new LocationTracker(); tracker.setEnabled(true);
  tracker.ingest(envelope([location(now)]), now);
  tracker.ingest(envelope([location(now+1000)], 1, {sessionId:'new'}), now+1000);
  tracker.ingest(envelope([location(now+2000)], 2), now+2000);
  assert.equal(tracker.fixes(now+2000)[0]?.sessionId,'new');
});
