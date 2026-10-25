import assert from 'node:assert/strict';
import { test } from 'node:test';
import { createPlan, dueJobs, missedJobs } from '../src/core/planning.js';
import type { AppState, EnroEvent, RouteEstimate } from '../src/types.js';

const now = Date.parse('2026-10-24T20:00:00Z');
const event: EnroEvent = {
  id: 'calendar-event', title: 'UTC meetup', startAt: '2026-10-25T08:30:00-07:00',
  timeZone: 'America/Los_Angeles', location: 'Westfield UTC, San Diego',
  requirements: ['Bring ID'], status: 'confirmed', source: { type: 'calendar', id: 'calendar-event' },
};
const route: RouteEstimate = { durationSeconds: 25 * 60, distanceMeters: 8500, fetchedAt: new Date(now).toISOString(), provider: 'google-routes' };
const input = { event, origin: 'UC San Diego', travelMode: 'DRIVE' as const, preparationMinutes: 45, bufferMinutes: 10, route };

test('works backward using route and buffers, emits UTC instants, starts disarmed', () => {
  const plan = createPlan(input, now);
  assert.equal(plan.leaveAt, '2026-10-25T14:55:00.000Z');
  assert.equal(plan.wakeAt, '2026-10-25T14:10:00.000Z');
  assert.equal(plan.armed, false);
  assert.match(plan.id, /^[a-f\d-]{36}$/);
  assert.equal(plan.createdAt, '2026-10-24T20:00:00.000Z');
});

test('subtracts across local midnight correctly', () => {
  const plan = createPlan({ ...input, event: { ...event, startAt: '2026-10-25T00:30:00-07:00' } }, now);
  assert.equal(plan.leaveAt, '2026-10-25T06:55:00.000Z');
  assert.equal(plan.wakeAt, '2026-10-25T06:10:00.000Z');
  assert.equal(new Intl.DateTimeFormat('en-US', { timeZone: event.timeZone, hour: '2-digit', minute: '2-digit' }).format(new Date(plan.wakeAt)), '11:10 PM');
});

test('uses explicit offset at daylight saving fallback instead of guessing a repeated hour', () => {
  const early = createPlan({ ...input, event: { ...event, startAt: '2026-11-01T01:30:00-07:00' } }, now);
  const late = createPlan({ ...input, event: { ...event, startAt: '2026-11-01T01:30:00-08:00' } }, now);
  assert.equal(Date.parse(late.wakeAt) - Date.parse(early.wakeAt), 3_600_000);
});

test('rejects offsetless, normalized invalid, all-day, past, and malformed event dates', () => {
  for (const startAt of ['2026-10-25T08:30:00', '2027-02-30T08:30:00Z', '2026-10-25', '2026-10-24T10:00:00Z', '2026-10-25T24:00:00Z', '2026-10-25T08:30:00+14:01']) {
    assert.throws(() => createPlan({ ...input, event: { ...event, startAt } }, now), { name: 'Error' }, startAt);
  }
  assert.throws(() => createPlan({ ...input, event: { ...event, timeZone: 'Mars/Olympus' } }, now), /time zone/);
});

test('refuses tentative/cancelled events and empty route endpoints', () => {
  for (const status of ['tentative', 'cancelled'] as const) {
    assert.throws(() => createPlan({ ...input, event: { ...event, status } }, now), /confirmed/);
  }
  assert.throws(() => createPlan({ ...input, origin: ' ' }, now), /Origin/);
  assert.throws(() => createPlan({ ...input, event: { ...event, location: '' } }, now), /location/);
});

test('rejects invalid durations and stale/missing route estimates, accepts explicit zero', () => {
  for (const preparationMinutes of [-1, Infinity, NaN, 721]) {
    assert.throws(() => createPlan({ ...input, preparationMinutes }, now), /Preparation/);
  }
  for (const bufferMinutes of [-1, Infinity, 361]) {
    assert.throws(() => createPlan({ ...input, bufferMinutes }, now), /Buffer/);
  }
  for (const durationSeconds of [-1, NaN, Infinity, 172_801]) {
    assert.throws(() => createPlan({ ...input, route: { ...route, durationSeconds } }, now), /duration/);
  }
  assert.throws(() => createPlan({ ...input, route: undefined as unknown as RouteEstimate }, now), /estimate/);
  assert.throws(() => createPlan({ ...input, route: { ...route, fetchedAt: '2026-10-24T19:44:59Z' } }, now), /15 minutes/);
  assert.throws(() => createPlan({ ...input, route: { ...route, fetchedAt: '2026-10-24T20:02:01Z' } }, now), /15 minutes/);
  assert.doesNotThrow(() => createPlan({ ...input, preparationMinutes: 0, bufferMinutes: 0, route: { ...route, durationSeconds: 0 } }, now));
});

test('does not share mutable event or route objects with the caller', () => {
  const plan = createPlan(input, now);
  plan.event.requirements.push('Changed');
  plan.route.durationSeconds = 5;
  assert.deepEqual(event.requirements, ['Bring ID']);
  assert.equal(route.durationSeconds, 1500);
});

test('only returns armed confirmed due jobs with stable keys and no duplicates', () => {
  const plan = { ...createPlan(input, now), armed: true };
  const at = Date.parse(plan.wakeAt);
  const jobs = dueJobs([plan, plan], {}, at);
  assert.deepEqual(jobs, [{ key: `${plan.id}:wake:${plan.wakeAt}`, kind: 'wake', planId: plan.id, dueAt: plan.wakeAt }]);
  assert.equal(dueJobs([{ ...plan, armed: false }], {}, at).length, 0);
  assert.equal(dueJobs([{ ...plan, event: { ...plan.event, status: 'cancelled' } }], {}, at).length, 0);
  assert.equal(dueJobs([plan], {}, at - 1).length, 0);
});

test('suppresses every already recorded state, including unknown/failed, without mutating state', () => {
  const plan = { ...createPlan(input, now), armed: true };
  const at = Date.parse(plan.wakeAt);
  const key = dueJobs([plan], {}, at)[0]!.key;
  for (const state of ['dispatching', 'sent', 'failed', 'unknown', 'missed'] as const) {
    const jobs: AppState['jobs'] = { [key]: { state, at: new Date(at).toISOString() } };
    assert.deepEqual(dueJobs([plan], jobs, at), []);
    assert.equal(jobs[key]!.state, state);
  }
});

test('does not catch up more than two minutes late, exposes missed jobs separately', () => {
  const plan = { ...createPlan(input, now), armed: true };
  const at = Date.parse(plan.wakeAt);
  assert.equal(dueJobs([plan], {}, at + 120_000).length, 1);
  assert.equal(dueJobs([plan], {}, at + 120_001).length, 0);
  assert.equal(missedJobs([plan], {}, at + 120_001)[0]!.kind, 'wake');
  assert.equal(dueJobs([plan], {}, Date.parse(plan.event.startAt) + 1).length, 0);
  assert.equal(missedJobs([plan], {}, Date.parse(plan.event.startAt) + 1).length, 2);
});
