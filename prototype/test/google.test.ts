import test from 'node:test';
import assert from 'node:assert/strict';
import { GoogleProvider, type GoogleTokens } from '../src/providers/google.js';

const config = { clientId: 'test-client', clientSecret: 'test-secret', redirectUri: 'http://localhost:3000/auth/google/callback' };
const json = (body: unknown, status = 200) => new Response(JSON.stringify(body), { status, headers: { 'Content-Type': 'application/json' } });
function provider(handler: typeof fetch, initial: GoogleTokens | null = { accessToken: 'test-token', expiresAt: Date.now() + 3600_000 }) {
  let tokens = initial;
  return { client: new GoogleProvider(config, { read: () => tokens, write: next => { tokens = next; } }, handler), read: () => tokens };
}

test('OAuth requests only read scopes with offline access and supplied state', () => {
  const { client } = provider(async () => { throw new Error('Network must not run'); });
  const url = new URL(client.authorizationUrl('random-state'));
  assert.equal(url.searchParams.get('state'), 'random-state');
  assert.equal(url.searchParams.get('access_type'), 'offline');
  assert.equal(url.searchParams.get('redirect_uri'), config.redirectUri);
  assert.deepEqual(url.searchParams.get('scope')?.split(' '), [
    'https://www.googleapis.com/auth/calendar.events.readonly', 'https://www.googleapis.com/auth/gmail.readonly',
  ]);
  assert.throws(() => client.authorizationUrl(''), /state/);
});

test('new authorization never retains a previous account refresh token', async () => {
  const { client, read } = provider(async (_url, init) => {
    assert.equal(new URLSearchParams(String(init?.body)).get('grant_type'), 'authorization_code');
    return json({ access_token: 'new-access', expires_in: 3600 });
  }, { accessToken: 'old', refreshToken: 'old-account', expiresAt: 0 });
  await client.exchangeCode('test-code');
  assert.equal(read()?.accessToken, 'new-access');
  assert.equal(read()?.refreshToken, undefined);
});

test('expired token refreshes once and preserves its refresh token', async () => {
  let refreshes = 0;
  const { client, read } = provider(async (url, init) => {
    if (String(url).includes('oauth2.googleapis.com')) {
      refreshes++;
      assert.equal(new URLSearchParams(String(init?.body)).get('refresh_token'), 'refresh-test');
      return json({ access_token: 'renewed', expires_in: 3600 });
    }
    assert.equal(new Headers(init?.headers).get('Authorization'), 'Bearer renewed');
    return json({ items: [] });
  }, { accessToken: 'expired', refreshToken: 'refresh-test', expiresAt: 0 });
  await Promise.all([client.listEvents(), client.listEvents()]);
  assert.equal(refreshes, 1);
  assert.equal(read()?.refreshToken, 'refresh-test');
});

test('401 refreshes once and retries the data request', async () => {
  let gets = 0;
  const { client } = provider(async url => {
    if (String(url).includes('oauth2.googleapis.com')) return json({ access_token: 'refreshed', expires_in: 3600 });
    return ++gets === 1 ? json({}, 401) : json({ items: [] });
  }, { accessToken: 'revoked', refreshToken: 'refresh-test', expiresAt: Date.now() + 3600_000 });
  assert.deepEqual(await client.listEvents(), []);
  assert.equal(gets, 2);
});

test('calendar imports future located events, expands recurrences, preserves timezone and paginates', async () => {
  const now = Date.parse('2026-10-24T12:00:00Z');
  const future = { dateTime: '2026-10-25T08:30:00-07:00', timeZone: 'America/Los_Angeles' };
  let calls = 0;
  const { client } = provider(async input => {
    const url = new URL(String(input));
    assert.equal(url.searchParams.get('singleEvents'), 'true');
    assert.equal(url.searchParams.get('timeMax'), '2026-10-31T12:00:00.000Z');
    calls++;
    if (calls === 2) {
      assert.equal(url.searchParams.get('pageToken'), 'page-two');
      return json({ timeZone: 'America/New_York', items: [{ id: 'second', start: { dateTime: '2026-10-25T12:30:00-04:00' }, location: 'Office' }] });
    }
    return json({ nextPageToken: 'page-two', items: [
      { id: 'good', summary: 'UTC event', start: future, location: 'Westfield UTC', htmlLink: 'https://calendar.google.com/test' },
      { id: 'all-day', start: { date: '2026-10-25' }, location: 'Home' },
      { id: 'declined', start: future, location: 'Office', attendees: [{ self: true, responseStatus: 'declined' }] },
      { id: 'cancelled', status: 'cancelled', start: future, location: 'Office' },
      { id: 'virtual', start: future },
      { id: 'past', start: { dateTime: '2026-10-23T08:00:00Z' }, location: 'Office' },
    ] });
  });
  const events = await client.listEvents(now);
  assert.deepEqual(events.map(event => event.id), ['google:good', 'google:second']);
  assert.equal(events[0]?.startAt, future.dateTime);
  assert.equal(events[0]?.timeZone, 'America/Los_Angeles');
  assert.equal(events[1]?.timeZone, 'America/New_York');
});

test('email returns recursive plaintext and ICS source evidence without inventing event dates', async () => {
  const b64 = (text: string) => Buffer.from(text).toString('base64url');
  const now = Date.parse('2026-10-24T12:00:00Z');
  const { client } = provider(async input => {
    const url = new URL(String(input));
    if (url.pathname.endsWith('/messages')) {
      assert.equal(url.searchParams.get('maxResults'), '10');
      assert.match(url.searchParams.get('q')!, /after:\d+.*filename:ics/);
      return json({ messages: [{ id: 'mail-id' }] });
    }
    if (url.pathname.includes('/attachments/')) return json({ data: b64('BEGIN:VEVENT\nDTSTART:20261025T153000Z\nEND:VEVENT') });
    return json({ id: 'mail-id', internalDate: String(now), payload: {
      mimeType: 'multipart/mixed', headers: [{ name: 'Subject', value: 'Invitation: UTC' }], parts: [
        { mimeType: 'multipart/alternative', parts: [
          { mimeType: 'text/html', body: { data: b64('<p>HTML duplicate</p>') } },
          { mimeType: 'text/plain', body: { data: b64('Tomorrow at Westfield UTC; bring your ID.') } },
        ] },
        { mimeType: 'text/calendar', filename: 'invite.ics', body: { attachmentId: 'calendar-part', size: 100 } },
      ],
    } });
  });
  const [candidate] = await client.listEmailCandidates(now);
  assert.match(candidate!.body, /Tomorrow at Westfield UTC/);
  assert.match(candidate!.body, /DTSTART:20261025T153000Z/);
  assert.doesNotMatch(candidate!.body, /HTML duplicate/);
  assert.equal(candidate!.receivedAt, '2026-10-24T12:00:00.000Z');
  assert.equal('startAt' in candidate!, false);
});

test('calendar distinguishes user attendance from the organizer event status', async () => {
  const now = Date.parse('2026-10-24T12:00:00Z');
  const base = { start: { dateTime: '2026-10-25T15:30:00Z' }, location: 'Westfield UTC', status: 'confirmed' };
  const { client } = provider(async () => json({ items: [
    { ...base, id: 'unanswered', attendees: [{ self: true, responseStatus: 'needsAction' }] },
    { ...base, id: 'tentative', attendees: [{ self: true, responseStatus: 'tentative' }] },
    { ...base, id: 'accepted', attendees: [{ self: true, responseStatus: 'accepted' }, { responseStatus: 'declined' }] },
    { ...base, id: 'owned' },
    { ...base, id: 'declined', attendees: [{ self: true, responseStatus: 'declined' }] },
    { ...base, id: 'organizer-tentative', status: 'tentative', attendees: [{ self: true, responseStatus: 'accepted' }] },
  ] }));
  const events = await client.listEvents(now);
  assert.deepEqual(events.map(event => [event.id, event.status]), [
    ['google:unanswered', 'tentative'], ['google:tentative', 'tentative'],
    ['google:accepted', 'confirmed'], ['google:owned', 'confirmed'],
    ['google:organizer-tentative', 'tentative'],
  ]);
});

test('provider errors do not expose token response contents', async () => {
  const { client } = provider(async () => json({ error_description: 'test-secret-private' }, 400));
  await assert.rejects(client.exchangeCode('bad'), error => {
    assert.match(String(error), /HTTP 400/);
    assert.doesNotMatch(String(error), /test-secret-private/);
    return true;
  });
});
