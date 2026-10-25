import test from 'node:test';
import assert from 'node:assert/strict';
import { evaluateScene, formatTime } from './stage-model.js';

test('Crossroads timings subtract travel, arrival buffer and preparation', () => {
  const result = evaluateScene();
  assert.equal(formatTime(result.eventAt), '6:30 PM');
  assert.equal(formatTime(result.leaveAt), '6:00 PM');
  assert.equal(formatTime(result.wakeAt), '5:15 PM');
  assert.equal(formatTime(result.arrivalAt), '6:36 PM');
  assert.equal(result.lateMinutes, 6);
  const changed = evaluateScene({ travel: 30, buffer: 5, prep: 15 });
  assert.equal(formatTime(changed.leaveAt), '5:55 PM');
  assert.equal(formatTime(changed.wakeAt), '5:40 PM');
});
test('seven staged shaking counts and a check-in cannot complete the preview', () => {
  const shaking = evaluateScene({ scene: 'shaking', checkedIn: true });
  assert.equal(shaking.countsMet, true);
  assert.equal(shaking.activityMet, false);
  assert.equal(shaking.confirmed, false);
  assert.match(shaking.decision, /walking unconfirmed/);
  assert.equal(evaluateScene({ scene: 'pickup', checkedIn: true }).confirmed, false);
});
test('the staged walking scene requires the spoken check-in too', () => {
  assert.equal(evaluateScene({ scene: 'walking' }).confirmed, false);
  assert.equal(evaluateScene({ scene: 'walking', checkedIn: true }).confirmed, true);
  assert.equal(evaluateScene({ scene: 'walking', checkedIn: true }).earlyMinutes, 10);
});
test('outside-origin GPS remains distinct from walking confirmation and arrival', () => {
  const result = evaluateScene({ scene: 'departed', checkedIn: true });
  assert.equal(result.signal.outside, true);
  assert.equal(result.confirmed, false);
  assert.match(result.decision, /route unconfirmed/);
  assert.equal(result.arrivalAt, 18 * 60 + 20);
});
test('invalid assumptions fail instead of producing fictional timings', () => {
  for (const input of [{ scene: 'asleep' }, { travel: 0 }, { clock: NaN }, { buffer: 31 }, { prep: -1 }]) {
    assert.throws(() => evaluateScene(input), RangeError);
  }
  assert.equal(formatTime(-1), '11:59 PM');
});
