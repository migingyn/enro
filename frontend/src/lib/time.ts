import type { Timestamp } from '../api/types';
import { TIMEZONE } from './env';

const timeFmt = new Intl.DateTimeFormat('en-US', {
  timeZone: TIMEZONE,
  hour: 'numeric',
  minute: '2-digit',
});

const timeSecFmt = new Intl.DateTimeFormat('en-US', {
  timeZone: TIMEZONE,
  hour: 'numeric',
  minute: '2-digit',
  second: '2-digit',
});

/** Epoch seconds -> "7:39 AM" in America/Los_Angeles. */
export function fmtTime(ts: Timestamp): string {
  return timeFmt.format(new Date(ts * 1000));
}

/** Epoch seconds -> "7:39:12 AM" in America/Los_Angeles. */
export function fmtTimeSec(ts: Timestamp): string {
  return timeSecFmt.format(new Date(ts * 1000));
}

export function nowTs(): Timestamp {
  return Math.floor(Date.now() / 1000);
}
