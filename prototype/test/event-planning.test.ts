import test from 'node:test';
import assert from 'node:assert/strict';
// @ts-expect-error Browser module is intentionally plain JavaScript.
import { upcomingEvents, matchingPlan, calculateSelected } from '../public/event-planning.js';

const now = Date.parse('2026-10-24T12:00:00Z');
const event = { id: 'calendar:a', title: 'Class', location: 'Price Center', startAt: '2026-10-25T17:00:00Z', status: 'confirmed' };
const settings = { origin: 'Crossroads', travelMode: 'DRIVE', preparationMinutes: 45, bufferMinutes: 10 };
const plan = { id: 'plan:a', event, ...settings, route: { provider: 'google-routes', fetchedAt: new Date(now).toISOString() }, leaveAt: '2026-10-25T16:30:00Z', armed: false };

test('event review excludes cancelled, past, invalid, and unlocated events', () => {
  const events = [event, { ...event, id: 'cancelled', status: 'cancelled' }, { ...event, id: 'past', startAt: '2026-10-23T12:00:00Z' }, { ...event, id: 'invalid', startAt: 'bad' }, { ...event, id: 'empty', location: ' ' }];
  assert.deepEqual(upcomingEvents(events, now).map((item: typeof event) => item.id), ['calendar:a']);
});

test('changed event details or travel settings cannot reuse an old plan', () => {
  assert.equal(matchingPlan([plan], event, settings, now), plan);
  assert.equal(matchingPlan([plan], { ...event, location: 'Airport' }, settings, now), undefined);
  assert.equal(matchingPlan([plan], { ...event, startAt: '2026-10-25T19:00:00Z' }, settings, now), undefined);
  assert.equal(matchingPlan([plan], event, { ...settings, travelMode: 'WALK' }, now), undefined);
  assert.equal(matchingPlan([plan], event, { ...settings, preparationMinutes: 30 }, now), undefined);
  assert.equal(matchingPlan([{ ...plan, accessMinutes: 10 }], event, settings, now), undefined);
  assert.equal(matchingPlan([plan], event, settings, now + 15 * 60_000 + 1), undefined);
  assert.equal(matchingPlan([{ ...plan, route: { ...plan.route, fetchedAt: 'invalid' } }], event, settings, now), undefined);
  assert.equal(matchingPlan([plan], event, settings, now - 2 * 60_000 - 1), undefined);
  assert.equal(matchingPlan([{ ...plan, route: { provider: 'osrm' } }], event, settings, now), undefined);
  assert.equal(matchingPlan([plan], event, settings, Date.parse('2026-10-26T00:00:00Z')), undefined);
});

test('partial route failures retain successes and retries reuse completed plans without arming', async () => {
  const second = { ...event, id: 'email-ics:b', title: 'Interview', status: 'tentative' };
  const calls: string[] = [];
  const state = { events: [event, second], plans: [] as typeof plan[] };
  const api = async (path: string, options: { body: string }) => {
    calls.push(path);
    if (path.endsWith('/confirm')) return { ...second, status: 'confirmed' };
    const body = JSON.parse(options.body);
    if (body.eventId === second.id) throw new Error('Route unavailable');
    state.plans.push(plan);
    return plan;
  };
  const first = await calculateSelected({ ids: [event.id, second.id, event.id], state, settings, api, now });
  assert.equal(first.results.length, 1);
  assert.equal(first.errors.length, 1);
  assert.deepEqual(calls, ['/api/plans', '/api/events/email-ics%3Ab/confirm', '/api/plans']);
  calls.length = 0;
  const retry = await calculateSelected({ ids: [event.id], state, settings, api, now });
  assert.equal(retry.results[0], plan);
  assert.deepEqual(calls, []);
});

test('an event removed during review cannot create a plan', async () => {
  let called = false;
  const result = await calculateSelected({ ids: [event.id], state: { events: [], plans: [] }, settings, api: async () => { called = true; }, now });
  assert.equal(called, false);
  assert.equal(result.results.length, 0);
  assert.equal(result.errors.length, 1);
});

test('failed attendance confirmation prevents route creation', async () => {
  const invitation = { ...event, id: 'email-ics:x', status: 'tentative' };
  const calls: string[] = [];
  const result = await calculateSelected({ ids: [invitation.id], state: { events: [invitation], plans: [] }, settings, api: async (path: string) => { calls.push(path); throw new Error('Confirmation failed'); }, now });
  assert.deepEqual(calls, ['/api/events/email-ics%3Ax/confirm']);
  assert.equal(result.results.length, 0);
  assert.match(result.errors[0].message, /Confirmation failed/);
});

test('online events stay visible but never request a travel plan', async () => {
  const online = { ...event, travel: { kind: 'online' }, location: 'Online' };
  let called = false;
  assert.equal(upcomingEvents([online], now).length, 1);
  const result = await calculateSelected({ ids: [online.id], state: { events: [online], plans: [] }, settings, api: async () => { called = true; }, now });
  assert.equal(called, false);
  assert.match(result.errors[0].message, /online event/);
});
