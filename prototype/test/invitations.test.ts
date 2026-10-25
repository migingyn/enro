import test from 'node:test';
import assert from 'node:assert/strict';
import { parseInvitationCandidates } from '../src/providers/invitations.js';
import type { EmailCandidate } from '../src/providers/google.js';

const now = Date.parse('2026-10-24T12:00:00Z');
function candidate(lines: string[], options: { method?: string; id?: string; receivedAt?: string; uid?: string } = {}): EmailCandidate {
  return {
    id: options.id ?? 'mail-1', subject: 'Invitation', receivedAt: options.receivedAt ?? '2026-10-24T11:00:00Z',
    sourceUrl: `https://mail.google.com/mail/u/0/#all/${options.id ?? 'mail-1'}`,
    body: `Email introduction\r\nBEGIN:VCALENDAR\r\nVERSION:2.0\r\nMETHOD:${options.method ?? 'REQUEST'}\r\nBEGIN:VEVENT\r\nUID:${options.uid ?? 'event-1@example.test'}\r\nSUMMARY:Morning event\r\nLOCATION:Westfield UTC\\, San Diego\r\n${lines.join('\r\n')}\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n`,
  };
}

test('UTC invitation normalizes exact DTSTART and retains Gmail source, never received time', () => {
  const [event] = parseInvitationCandidates([candidate(['DTSTART:20261025T153000Z'])], now);
  assert.equal(event?.startAt, '2026-10-25T15:30:00.000Z');
  assert.equal(event?.timeZone, 'UTC');
  assert.equal(event?.location, 'Westfield UTC, San Diego');
  assert.equal(event?.source.type, 'email');
  assert.equal(event?.source.id, 'mail-1');
  assert.equal(event?.status, 'tentative');
  assert.deepEqual(event?.requirements, []);
});

test('IANA TZID works without a VTIMEZONE and preserves timezone for display', () => {
  const [event] = parseInvitationCandidates([candidate(['DTSTART;TZID=America/Los_Angeles:20261025T083000'])], now);
  assert.equal(event?.startAt, '2026-10-25T15:30:00.000Z');
  assert.equal(event?.timeZone, 'America/Los_Angeles');
});

test('embedded VTIMEZONE must agree with the IANA conversion for the invitation date', () => {
  const source = candidate(['DTSTART;TZID=America/Los_Angeles:20261025T083000']);
  const zone = 'BEGIN:VTIMEZONE\r\nTZID:America/Los_Angeles\r\nBEGIN:STANDARD\r\nDTSTART:19700101T000000\r\nTZOFFSETFROM:-0700\r\nTZOFFSETTO:-0700\r\nEND:STANDARD\r\nEND:VTIMEZONE\r\n';
  source.body = source.body.replace('BEGIN:VEVENT', `${zone}BEGIN:VEVENT`);
  assert.equal(parseInvitationCandidates([source], now)[0]?.startAt, '2026-10-25T15:30:00.000Z');
  source.body = source.body.replaceAll('-0700', '-0500');
  assert.deepEqual(parseInvitationCandidates([source], now), []);
});

test('quoted half-hour timezone and line unfolding are handled', () => {
  const source = candidate(['DTSTART;TZID="Asia/Kolkata":20261025T083000']);
  source.body = source.body.replace('SUMMARY:Morning event', 'SUMMARY:Morning \\nworkshop\r\n  at UTC');
  const [event] = parseInvitationCandidates([source], now);
  assert.equal(event?.startAt, '2026-10-25T03:00:00.000Z');
  assert.equal(event?.title, 'Morning \nworkshop at UTC');
});

test('all-day, floating, unknown timezone, malformed civil date, and conflicting UTC TZID are rejected', () => {
  for (const line of [
    'DTSTART;VALUE=DATE:20261025', 'DTSTART:20261025T083000',
    'DTSTART;TZID=Custom/Unresolvable:20261025T083000', 'DTSTART:20261032T083000Z',
    'DTSTART;TZID=America/Los_Angeles:20261025T083000Z',
  ]) assert.deepEqual(parseInvitationCandidates([candidate([line])], now), [], line);
});

test('ambiguous fall-back hour and nonexistent spring-forward hour require source review', () => {
  assert.deepEqual(parseInvitationCandidates([candidate(['DTSTART;TZID=America/Los_Angeles:20261101T013000'])], Date.parse('2026-10-30T00:00:00Z')), []);
  assert.deepEqual(parseInvitationCandidates([candidate(['DTSTART;TZID=America/Los_Angeles:20260308T023000'])], Date.parse('2026-03-06T00:00:00Z')), []);
  const [valid] = parseInvitationCandidates([candidate(['DTSTART;TZID=America/Los_Angeles:20261101T033000'])], Date.parse('2026-10-30T00:00:00Z'));
  assert.equal(valid?.startAt, '2026-11-01T11:30:00.000Z');
});

test('recurrence rules, dates, exclusions and detached instances are rejected', () => {
  for (const recurrence of ['RRULE:FREQ=DAILY', 'RDATE:20261026T153000Z', 'EXDATE:20261026T153000Z', 'RECURRENCE-ID:20261025T153000Z']) {
    assert.deepEqual(parseInvitationCandidates([candidate(['DTSTART:20261025T153000Z', recurrence])], now), []);
  }
});

test('tentative and cancelled status survive normalization; attendee reply is not an invitation', () => {
  assert.equal(parseInvitationCandidates([candidate(['DTSTART:20261025T153000Z', 'STATUS:TENTATIVE'])], now)[0]?.status, 'tentative');
  assert.equal(parseInvitationCandidates([candidate(['DTSTART:20261025T153000Z', 'STATUS:CONFIRMED'])], now)[0]?.status, 'tentative');
  assert.equal(parseInvitationCandidates([candidate(['DTSTART:20261025T153000Z', 'STATUS:CANCELLED'])], now)[0]?.status, 'cancelled');
  assert.equal(parseInvitationCandidates([candidate(['DTSTART:20261025T153000Z'], { method: 'CANCEL' })], now)[0]?.status, 'cancelled');
  assert.deepEqual(parseInvitationCandidates([candidate(['DTSTART:20261025T153000Z'], { method: 'REPLY' })], now), []);
});

test('latest sequence replaces old invitation regardless of mail order and keeps stable event id', () => {
  const older = candidate(['DTSTART:20261025T153000Z', 'SEQUENCE:1'], { id: 'old' });
  const newer = candidate(['DTSTART:20261025T160000Z', 'SEQUENCE:2'], { id: 'new' });
  const [event] = parseInvitationCandidates([newer, older], now);
  assert.equal(event?.startAt, '2026-10-25T16:00:00.000Z');
  assert.equal(event?.source.id, 'new');
  assert.equal(event?.id, parseInvitationCandidates([older], now)[0]?.id);
  const cancellation = candidate(['SEQUENCE:3'], { method: 'CANCEL', id: 'cancelled' });
  assert.deepEqual(parseInvitationCandidates([older, cancellation], now), []);
});

test('missing location, incomplete ICS and dates outside next seven days remain candidates only', () => {
  const missing = candidate(['DTSTART:20261025T153000Z']);
  missing.body = missing.body.replace('LOCATION:Westfield UTC\\, San Diego\r\n', '');
  const broken = candidate(['DTSTART:20261025T153000Z']);
  broken.body = broken.body.replace('END:VCALENDAR', '');
  assert.deepEqual(parseInvitationCandidates([missing, broken], now), []);
  assert.deepEqual(parseInvitationCandidates([candidate(['DTSTART:20261023T153000Z'])], now), []);
  assert.deepEqual(parseInvitationCandidates([candidate(['DTSTART:20261103T153000Z'])], now), []);
});
