import { createHash } from 'node:crypto';
import ICAL from 'ical.js';
import type { EnroEvent } from '../types.js';
import { preparationRequirements } from '../core/requirements.js';
import type { EmailCandidate } from './google.js';

interface InvitationVersion {
  component: InstanceType<typeof ICAL.Component>;
  calendar: InstanceType<typeof ICAL.Component>;
  source: EmailCandidate;
  method: string;
  sequence: number;
  updatedAt: number;
}

/**
 * Convert complete, one-off ICS invitations into events for user review.
 * RFC 5545: https://www.rfc-editor.org/rfc/rfc5545.html
 * Parser: https://kewisch.github.io/ical.js/api/ICAL.Component.html
 * Only UTC or IANA TZID dates are supported. All-day/floating dates, DST folds
 * and gaps, custom timezones, and recurrence stay in the source-review workflow.
 * receivedAt orders otherwise equal invitation versions; it NEVER sets startAt.
 */
export function parseInvitationCandidates(candidates: EmailCandidate[], now = Date.now()): EnroEvent[] {
  if (!Number.isFinite(now)) throw new Error('A valid current timestamp is required.');
  const versions = new Map<string, InvitationVersion>();
  for (const source of candidates.slice(0, 10)) {
    const blocks = source.body.slice(0, 80_000).match(/^BEGIN:VCALENDAR\r?\n[\s\S]*?^END:VCALENDAR[ \t]*\r?$/gmi) ?? [];
    for (const block of blocks.slice(0, 10)) {
      try {
        const calendar = new ICAL.Component(ICAL.parse(block));
        if (calendar.name !== 'vcalendar') continue;
        const method = text(calendar, 'method').toUpperCase();
        if (!['', 'REQUEST', 'PUBLISH', 'CANCEL'].includes(method)) continue;
        for (const component of calendar.getAllSubcomponents('vevent').slice(0, 50)) {
          const uid = text(component, 'uid');
          if (!uid || component.getAllProperties('uid').length !== 1) continue;
          const sequence = Number(component.getFirstPropertyValue('sequence') ?? 0);
          if (!Number.isSafeInteger(sequence) || sequence < 0) continue;
          const revision = String(component.getFirstProperty('last-modified')?.toJSON()[3]
            ?? component.getFirstProperty('dtstamp')?.toJSON()[3] ?? '');
          const revisionMs = /Z$/.test(revision) ? Date.parse(revision) : NaN;
          const received = Date.parse(source.receivedAt);
          const updatedAt = Number.isFinite(revisionMs) ? revisionMs : Number.isFinite(received) ? received : 0;
          const version = { component, calendar, source, method, sequence, updatedAt };
          const prior = versions.get(uid);
          if (!prior || sequence > prior.sequence || (sequence === prior.sequence && updatedAt > prior.updatedAt)
            || (sequence === prior.sequence && updatedAt === prior.updatedAt && method === 'CANCEL')) {
            versions.set(uid, version);
          }
        }
      } catch {
        // Malformed or unsupported attachments remain visible as source candidates.
      }
    }
  }

  const events: EnroEvent[] = [];
  for (const [uid, version] of versions) {
    try {
      const { component, calendar, source, method } = version;
      if (['rrule', 'rdate', 'exdate', 'exrule', 'recurrence-id'].some(name => component.hasProperty(name))) continue;
      const title = text(component, 'summary');
      const location = text(component, 'location');
      if (!title || !location || component.getAllProperties('dtstart').length !== 1) continue;
      const start = component.getFirstProperty('dtstart');
      if (!start || start.type !== 'date-time') continue;
      const raw = start.toJSON()[3];
      const tzid = start.getFirstParameter('tzid');
      const normalized = normalizeDate(raw, typeof tzid === 'string' ? tzid : undefined);
      if (!normalized || normalized.timestamp < now || normalized.timestamp >= now + 7 * 86_400_000) continue;

      // ICAL.js has no bundled IANA timezone data. Intl supplies the IANA fallback.
      // If the file also supplies VTIMEZONE, reject conflicting definitions.
      if (tzid && calendar.getTimeZoneByID(tzid)) {
        const time = start.getFirstValue();
        if (!(time instanceof ICAL.Time) || time.toUnixTime() * 1000 !== normalized.timestamp) continue;
      }
      const statedStatus = text(component, 'status').toUpperCase();
      if (statedStatus && !['CONFIRMED', 'TENTATIVE', 'CANCELLED'].includes(statedStatus)) continue;
      // An organizer's CONFIRMED status is not evidence that this recipient
      // accepted. The review UI must explicitly confirm attendance before arming.
      const status = method === 'CANCEL' || statedStatus === 'CANCELLED' ? 'cancelled' : 'tentative';
      events.push({
        id: `email-ics:${createHash('sha256').update(uid).digest('hex').slice(0, 24)}`,
        title, startAt: new Date(normalized.timestamp).toISOString(), timeZone: normalized.timeZone,
        location, requirements: preparationRequirements(text(component, 'description')), status,
        source: { type: 'email', id: source.id, url: source.sourceUrl },
      });
    } catch {
      // Do not select a guessed timezone, recurrence instance, or malformed date.
    }
  }
  return events.sort((left, right) => Date.parse(left.startAt) - Date.parse(right.startAt));
}

function text(component: InstanceType<typeof ICAL.Component>, property: string): string {
  const value = component.getFirstPropertyValue(property);
  return typeof value === 'string' ? value.trim() : '';
}

function normalizeDate(raw: unknown, tzid?: string): { timestamp: number; timeZone: string } | null {
  if (typeof raw !== 'string') return null;
  const match = /^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(Z)?$/.exec(raw);
  if (!match) return null;
  const [year, month, day, hour, minute, second] = match.slice(1, 7).map(Number) as [number, number, number, number, number, number];
  if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 59) return null;
  const civil = Date.UTC(year, month - 1, day, hour, minute, second);
  if (new Date(civil).toISOString().slice(0, 19) !== raw.slice(0, 19)) return null;
  if (match[7]) return tzid ? null : { timestamp: civil, timeZone: 'UTC' };
  if (!tzid) return null; // Floating local times have no safe implicit timezone.

  let formatter: Intl.DateTimeFormat;
  try {
    formatter = new Intl.DateTimeFormat('en-GB', {
      timeZone: tzid, calendar: 'gregory', numberingSystem: 'latn',
      year: 'numeric', month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit', second: '2-digit', hourCycle: 'h23',
    });
  } catch { return null; }
  const localEpoch = (timestamp: number): number => {
    const fields = Object.fromEntries(formatter.formatToParts(timestamp).map(part => [part.type, part.value]));
    return Date.UTC(Number(fields.year), Number(fields.month) - 1, Number(fields.day), Number(fields.hour), Number(fields.minute), Number(fields.second));
  };
  // Collect offsets on both sides of a nearby transition, then round-trip every
  // possible instant. Two matches = repeated DST hour; zero = nonexistent hour.
  const offsets = new Set<number>();
  for (const hours of [-48, -24, -12, 0, 12, 24, 48]) {
    const sample = civil + hours * 3_600_000;
    offsets.add(localEpoch(sample) - sample);
  }
  const matches = [...offsets].map(offset => civil - offset).filter(timestamp => localEpoch(timestamp) === civil);
  return matches.length === 1 ? { timestamp: matches[0]!, timeZone: tzid } : null;
}
