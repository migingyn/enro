import test from 'node:test';
import assert from 'node:assert/strict';
import twilio from 'twilio';
import { voiceResponse, validSignature, voiceReady } from '../src/providers/voice.js';
import type { CallSession, MotionSnapshot } from '../src/types.js';
import type { TripContext } from '../src/core/trip.js';
const now = Date.now();
const session = (): CallSession => ({ id: 'test', kind: 'test', status: 'in-progress', createdAt: new Date(now - 10_000).toISOString(), answeredAt: new Date(now - 8_000).toISOString(), verbalConfirmed: false, movementObserved: false, rounds: 0, greeting: 'Test wake-up.' });
const motion = (status: MotionSnapshot['status']): MotionSnapshot => ({ status, lastSampleAt: now, lastReceivedAt: now, sampleCount: 20, movementSamples: 10, movementSeconds: 3, latestMagnitude: 2, latencyMs: 50, deviceId: 'test-only', source: 'sensor-logger' });

const steps = () => ({ ...motion('stationary'), walking: { status: 'step-threshold-met' as const, thresholdMet: true, threshold: 6, stepsSinceBaseline: 6, scopedTo: now - 8_000, lastSampleAt: now, activityCorroborated: true }, activity: { status: 'available' as const, classification: 'walking' as const, lastSampleAt: now, lastReceivedAt: now } });
test('counts plus fresh walking activity and affirmative speech can end the experimental check', () => {
  const call = session();
  const xml = voiceResponse(call, steps(), { speech: "I'm up" }, undefined, now);
  assert.match(xml, /<Hangup/); assert.equal(call.result, 'steps-activity-and-voice-confirmed');
  assert.doesNotMatch(xml, /all set|confirmed awake/);
});
test('seven shake-generated pedometer increments plus voice cannot pass without walking activity', () => {
  for (const classification of ['stationary', 'unknown', 'unrecognized', 'driving', 'cycling'] as const) {
    const evidence = { ...steps(), activity: { ...steps().activity, classification } };
    evidence.walking.stepsSinceBaseline = 7;
    const call = session();
    const xml = voiceResponse(call, evidence, { speech: 'I am up' }, undefined, now);
    assert.doesNotMatch(xml, /<Hangup/);
    assert.match(xml, /Shaking can increase the counter/);
    assert.equal(call.result, undefined);
  }
  const { activity: _ignored, ...missingActivity } = steps();
  missingActivity.walking.stepsSinceBaseline = 7;
  assert.doesNotMatch(voiceResponse(session(), missingActivity, { speech: 'I am up' }, undefined, now), /<Hangup/);
});
test('unavailable, stale, pre-answer, and uncorroborated activity cannot pass', () => {
  for (const patch of [{ status: 'stale' as const }, { status: 'unknown-schema' as const }, { status: 'unavailable' as const },
    { lastSampleAt: now - 16_000 }, { lastReceivedAt: now - 9_000 }]) {
    const evidence = { ...steps(), activity: { ...steps().activity, ...patch } };
    assert.doesNotMatch(voiceResponse(session(), evidence, { speech: 'I am up' }, undefined, now), /<Hangup/);
  }
  const evidence = steps(); evidence.walking.activityCorroborated = false;
  assert.doesNotMatch(voiceResponse(session(), evidence, { speech: 'I am up' }, undefined, now), /<Hangup/);
});
test('shaking and a spoken check-in cannot end the wake call without steps', () => {
  const xml = voiceResponse(session(), motion('moving'), { speech: "I'm up" }, undefined, now);
  assert.doesNotMatch(xml, /<Hangup/);
});
test('old or differently scoped steps cannot end the wake call', () => {
  for (const patch of [{ scopedTo: now - 9_000 }, { lastSampleAt: now - 16_000 }, { thresholdMet: false }, { status: 'counter-reset' }]) {
    const evidence = steps(); Object.assign(evidence.walking, patch);
    assert.doesNotMatch(voiceResponse(session(), evidence, { speech: "I'm up" }, undefined, now), /<Hangup/);
  }
});
test('stale evidence cannot finish a call even with earlier movement', () => {
  const call = session(); call.movementObserved = true;
  const xml = voiceResponse(call, motion('stale'), { speech: 'I am up' }, undefined, now);
  assert.doesNotMatch(xml, /<Hangup/); assert.match(xml, /not live/);
});
test('motion alone and negative speech cannot count as check-in', () => {
  const call = session();
  const xml = voiceResponse(call, motion('moving'), { speech: "I'm not ready" }, undefined, now);
  assert.equal(call.verbalConfirmed, false); assert.doesNotMatch(xml, /<Hangup/);
});
test('a later negative response revokes an earlier check-in', () => {
  const call = session();
  voiceResponse(call, motion('stationary'), { speech: 'I am up' }, undefined, now);
  assert.equal(call.verbalConfirmed, true);
  const xml = voiceResponse(call, motion('moving'), { speech: 'I am not ready' }, undefined, now);
  assert.equal(call.verbalConfirmed, false);
  assert.doesNotMatch(xml, /<Hangup/);
});
test('stop always wins including simultaneous movement and readiness', () => {
  const call = session();
  voiceResponse(call, motion('moving'), { speech: 'I am up but stop' }, undefined, now);
  assert.equal(call.result, 'stopped-by-user');
  const dtmf = session(); voiceResponse(dtmf, motion('unavailable'), { digits: '0' }, undefined, now);
  assert.equal(dtmf.result, 'stopped-by-user');
});
test('call is bounded if the user is silent and no motion is available', () => {
  const call = session(); call.rounds = 7;
  assert.match(voiceResponse(call, motion('unavailable'), {}, undefined, now), /<Hangup/);
  assert.equal(call.result, 'time-limit');
});
test('snooze requests do not claim to schedule an unimplemented callback', () => {
  const call = session();
  const xml = voiceResponse(call, motion('stationary'), { speech: 'ten more minutes' }, undefined, now);
  assert.match(xml, /have not scheduled another call/);
});
test('departure uses a distinct verbal confirmation', () => {
  const call = session(); call.kind = 'leave';
  voiceResponse(call, motion('unavailable'), { speech: 'leaving' }, undefined, now);
  assert.equal(call.result, 'departure-reported-by-user');
});
test('wake check-ins and future departure statements do not count as leaving', () => {
  for (const speech of ['I am up', 'awake', 'ready', 'I am not leaving', 'I will be leaving later']) {
    const call = session(); call.kind = 'leave';
    assert.doesNotMatch(voiceResponse(call, steps(), { speech }, undefined, now), /<Hangup/);
    assert.equal(call.verbalConfirmed, false);
    assert.notEqual(call.result, 'departure-reported-by-user');
  }
});
test('signed webhook must match canonical URL and actual form body', () => {
  const config = { accountSid: `AC${'a'.repeat(32)}`, authToken: 'isolated-test-secret', from: '+15555550100', phone: '+15555550101', baseUrl: 'https://example.test' };
  const body = { CallSid: `CA${'b'.repeat(32)}`, SpeechResult: 'stop' };
  const signature = twilio.getExpectedTwilioSignature(config.authToken, 'https://example.test/voice/test', body);
  assert.equal(validSignature(config, signature, '/voice/test', body), true);
  assert.equal(validSignature(config, signature, '/voice/test', { ...body, SpeechResult: 'I am up' }), false);
  assert.equal(validSignature(config, '', '/voice/test', body), false);
  assert.equal(voiceReady(config), true);
  assert.equal(voiceReady({ ...config, phone: '+1571' }), false);
});
test('departure call uses fresh GPS and pose context without claiming clothing or travel from shakes',()=>{
  const call=session();call.kind='leave';call.rehearsal=true;
  const trip:TripContext={status:'still-at-origin',summary:'GPS still places the phone near Start. Departure for End has not been observed.',distanceToDestinationMeters:3300,gpsMovement:'stationary',destinationConfirmed:false,phonePosition:'horizontal',activity:'stationary',observedAt:new Date(now).toISOString()};
  const xml=voiceResponse(call,steps(),{speech:'check my progress'},undefined,now,trip);
  assert.match(xml,/GPS still places/);assert.match(xml,/horizontal/);assert.doesNotMatch(xml,/<Hangup/);
  const next=voiceResponse(call,steps(),{speech:'I am walking to the parking structure'},undefined,now,trip);
  assert.match(next,/Departure for End has not been observed/);assert.match(next,/<Hangup/);assert.equal(call.result,'departure-reported-by-user');
  const stale=session();stale.kind='leave';
  const absent=voiceResponse(stale,steps(),{speech:'check my progress'},undefined,now,{...trip,observedAt:new Date(now-6000).toISOString()});
  assert.match(absent,/unavailable/);assert.doesNotMatch(absent,/GPS still places/);
});
