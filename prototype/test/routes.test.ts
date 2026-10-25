import test from 'node:test';
import assert from 'node:assert/strict';
import { estimateRoute } from '../src/providers/routes.js';

const input = {
  apiKey: 'test-key', origin: 'UC San Diego', destination: 'Westfield UTC',
  travelMode: 'DRIVE' as const, arrivalAt: new Date(Date.now() + 86_400_000).toISOString(),
};
const json = (body: unknown, status = 200) => new Response(JSON.stringify(body), { status });

test('driving refines traffic at estimated departure and never sends ignored arrivalTime', async () => {
  const requests: Record<string, unknown>[] = [];
  const durations = ['1200s', '1500s', '1450s'];
  const result = await estimateRoute(input, async (url, init) => {
    assert.equal(url, 'https://routes.googleapis.com/directions/v2:computeRoutes');
    const headers = new Headers(init?.headers);
    assert.equal(headers.get('X-Goog-Api-Key'), 'test-key');
    assert.equal(headers.get('X-Goog-FieldMask'), 'routes.duration,routes.distanceMeters');
    const body = JSON.parse(String(init?.body));
    requests.push(body);
    assert.equal(body.routingPreference, 'TRAFFIC_AWARE_OPTIMAL');
    assert.equal(body.arrivalTime, undefined);
    return json({ routes: [{ duration: durations.shift(), distanceMeters: 8500 }] });
  });
  assert.equal(requests.length, 3);
  assert.equal(Date.parse(String(requests[1]?.departureTime)), Date.parse(input.arrivalAt) - 1200_000);
  assert.equal(Date.parse(String(requests[2]?.departureTime)), Date.parse(input.arrivalAt) - 1500_000);
  assert.equal(result.durationSeconds, 1500);
  assert.equal(result.provider, 'google-routes');
});

test('transit accounts for an earlier scheduled departure plus the initial walk', async () => {
  const departure = Date.parse(input.arrivalAt) - 3600_000;
  const result = await estimateRoute({ ...input, travelMode: 'TRANSIT' }, async (_url, init) => {
    const body = JSON.parse(String(init?.body));
    assert.equal(body.arrivalTime, input.arrivalAt);
    assert.equal(body.departureTime, undefined);
    assert.equal(body.routingPreference, undefined);
    return json({ routes: [{ duration: '1800s', legs: [{ steps: [
      { travelMode: 'WALK', staticDuration: '300s' },
      { travelMode: 'TRANSIT', staticDuration: '1200s', transitDetails: { stopDetails: { departureTime: new Date(departure).toISOString() } } },
      { travelMode: 'WALK', staticDuration: '300s' },
    ] }] }] });
  });
  assert.equal(result.durationSeconds, 3900);
});

test('walking requests no traffic preferences or time parameters', async () => {
  const result = await estimateRoute({ ...input, travelMode: 'WALK' }, async (_url, init) => {
    const body = JSON.parse(String(init?.body));
    assert.equal(body.travelMode, 'WALK');
    assert.equal(body.routingPreference, undefined);
    assert.equal(body.arrivalTime, undefined);
    assert.equal(body.departureTime, undefined);
    return json({ routes: [{ duration: '900.5s' }] });
  });
  assert.equal(result.durationSeconds, 901);
});

test('missing credentials, empty routes and malformed times never fall back to fake durations', async () => {
  const neverFetch = async () => { throw new Error('Network must not run'); };
  await assert.rejects(estimateRoute({ ...input, apiKey: '' }, neverFetch), /not configured/);
  await assert.rejects(estimateRoute({ ...input, arrivalAt: '2026-10-25T08:30:00' }, neverFetch), /timezone/);
  await assert.rejects(estimateRoute(input, async () => json({ routes: [] })), /no route/);
  await assert.rejects(estimateRoute(input, async () => json({ routes: [{ duration: 'NaNs' }] })), /invalid duration/);
  await assert.rejects(estimateRoute(input, async () => json({ error: 'secret' }, 403)), /HTTP 403/);
});

test('transit without a usable itinerary fails instead of risking a missed departure', async () => {
  await assert.rejects(estimateRoute({ ...input, travelMode: 'TRANSIT' }, async () => json({ routes: [{ duration: '1800s' }] })), /scheduled transit leg/);
});
