import test from 'node:test';
import assert from 'node:assert/strict';
import { destinationForTravel, travelContext } from '../src/providers/travel-destination.js';

const event = (location: string, account = 'student@ucsd.edu') => ({ location, source: { type: 'calendar' as const, id: 'source', account } });

test('school building codes resolve to verified UCSD building names', () => {
  assert.equal(destinationForTravel(event('WLH 2001')), 'Warren Lecture Hall, UC San Diego, La Jolla, CA');
  assert.equal(destinationForTravel(event('COA B27')), 'Coalition, UC San Diego, La Jolla, CA');
  assert.equal(destinationForTravel(event('PCYNH 122')), 'Pepper Canyon Hall, UC San Diego, La Jolla, CA');
});

test('campus resolution preserves the source room and requires a UCSD account', () => {
  const source = event('WLH 2001'); destinationForTravel(source);
  assert.equal(source.location, 'WLH 2001');
  assert.equal(destinationForTravel(event('WLH 2001', 'person@gmail.com')), 'WLH 2001');
  assert.equal(destinationForTravel(event('UNKNOWN 101')), 'UNKNOWN 101');
  assert.equal(destinationForTravel(event('123 Main St')), '123 Main St');
});

test('online events and meeting links are not physical travel destinations', () => {
  for (const location of ['Online', 'Virtual meeting', 'Zoom', 'https://ucsd.zoom.us/j/123', 'https://meet.google.com/abc-defg-hij']) {
    assert.deepEqual(travelContext(event(location)), { kind: 'online' });
    assert.throws(() => destinationForTravel(event(location)), /online/);
  }
  assert.equal(travelContext(event('Online Learning Center, 123 Main St')).kind, 'in-person');
  assert.equal(travelContext(event('https://notzoom.us')).kind, 'in-person');
});
