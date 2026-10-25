import test from 'node:test';
import assert from 'node:assert/strict';
import { GoogleAccounts, type GoogleAccountTokens } from '../src/providers/google-accounts.js';

const config = () => ({ clientId: 'test-client', clientSecret: 'test-secret', redirectUri: 'http://localhost:3333/oauth/google/callback' });
const json = (body: unknown, status = 200) => new Response(JSON.stringify(body), { status });

test('connecting and reconnecting accounts preserves the other account; wrong-account consent is rejected', async () => {
  let accounts: GoogleAccountTokens = {};
  let selected = 'personal@example.com';
  let token = 'personal-token';
  const client = new GoogleAccounts(config, { read: () => accounts, write: next => { accounts = next; } }, async input => {
    if (String(input).includes('/token')) return json({ access_token: token, refresh_token: `refresh-${token}`, expires_in: 3600 });
    assert.ok(String(input).endsWith('/profile'));
    return json({ emailAddress: selected });
  });
  assert.equal(new URL(client.authorizationUrl('state', selected)).searchParams.get('login_hint'), selected);
  await client.connect('code', selected);
  selected = 'school@example.edu'; token = 'school-token';
  await client.connect('code', selected);
  assert.equal(accounts['personal@example.com']?.accessToken, 'personal-token');
  assert.equal(accounts['school@example.edu']?.accessToken, 'school-token');
  token = 'school-new-token';
  await client.connect('code', selected);
  assert.equal(accounts['personal@example.com']?.accessToken, 'personal-token');
  assert.equal(accounts['school@example.edu']?.accessToken, 'school-new-token');
  const before = structuredClone(accounts);
  await assert.rejects(client.connect('code', 'wrong@example.com'), /different Google account/);
  assert.deepEqual(accounts, before);
  assert.equal(client.list().length, 2);
});

test('same calendar IDs and invitation UIDs stay separate across accounts; email links select their owner', async () => {
  const now = Date.now();
  const future = new Date(now + 86_400_000);
  const stamp = future.toISOString().replace(/[-:]/g, '').replace(/\.\d{3}/, '');
  let accounts: GoogleAccountTokens = {
    'personal@example.com': { accessToken: 'personal', expiresAt: now + 3600_000 },
    'school@example.edu': { accessToken: 'school', expiresAt: now + 3600_000 },
  };
  const client = new GoogleAccounts(config, { read: () => accounts, write: next => { accounts = next; } }, async (input, init) => {
    const url = new URL(String(input));
    const owner = new Headers(init?.headers).get('Authorization')?.replace('Bearer ', '');
    if (url.pathname.endsWith('/events')) return json({ items: [{ id: 'shared-id', summary: owner, location: 'Library', start: { dateTime: future.toISOString() } }] });
    if (url.pathname.endsWith('/messages')) return json({ messages: [{ id: 'shared-message' }] });
    return json({ internalDate: String(now), payload: { mimeType: 'text/calendar', headers: [{ name: 'Subject', value: owner }], body: { data: Buffer.from(`BEGIN:VCALENDAR\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nUID:shared-invitation\r\nDTSTART:${stamp}\r\nSUMMARY:Meeting\r\nLOCATION:Library\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n`).toString('base64url') } } });
  });
  const imported = await client.import();
  assert.equal(imported.calendar.length, 2);
  assert.equal(new Set(imported.calendar.map(event => event.id)).size, 2);
  assert.equal(imported.invitations.length, 2);
  assert.equal(new Set(imported.invitations.map(event => event.id)).size, 2);
  for (const event of imported.invitations) {
    assert.equal(new URL(event.source.url!).searchParams.get('authuser'), event.source.account);
    assert.equal(event.status, 'tentative');
  }
  const candidates = await client.candidates();
  assert.deepEqual(new Set(candidates.map(candidate => candidate.account)), new Set(Object.keys(accounts)));
});

test('failure from one account prevents a partial import from replacing stored source events', async () => {
  const client = new GoogleAccounts(config, { read: () => ({
    'personal@example.com': { accessToken: 'good', expiresAt: Date.now() + 3600_000 },
    'school@example.edu': { accessToken: 'bad', expiresAt: Date.now() + 3600_000 },
  }), write: () => {} }, async (_url, init) => new Headers(init?.headers).get('Authorization') === 'Bearer bad' ? json({}, 403) : json({ items: [], messages: [] }));
  await assert.rejects(client.import(), /HTTP 403/);
});
