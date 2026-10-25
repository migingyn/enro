// Fixed, explicitly staged scenario. This module never reads or mutates live data.
export const scenes = Object.freeze({
  stationary: { counts: 0, activity: 'stationary', position: 'horizontal', wait: 8, prepare: 8, outside: false, label: 'Still phone' },
  pickup: { counts: 0, activity: 'stationary', position: 'upright', wait: 3, prepare: 8, outside: false, label: 'Picked up' },
  shaking: { counts: 7, activity: 'stationary', position: 'tilted', wait: 3, prepare: 8, outside: false, label: 'Shaking' },
  walking: { counts: 8, activity: 'walking', position: 'upright', wait: 0, prepare: 0, outside: false, label: 'Walking' },
  departed: { counts: 8, activity: 'driving', position: 'horizontal', wait: 0, prepare: 0, outside: true, label: 'Outside origin' },
});

export function evaluateScene({ scene = 'stationary', clock = 0, travel = 20, prep = 45, buffer = 10, checkedIn = false } = {}) {
  if (!Object.hasOwn(scenes, scene)) throw new RangeError('Unknown scene.');
  for (const [name, value, maximum] of [['clock', clock, 25], ['travel', travel, 90], ['prep', prep, 120], ['buffer', buffer, 30]]) {
    if (!Number.isFinite(value) || value < (name === 'travel' ? 1 : 0) || value > maximum) throw new RangeError(`Invalid ${name}.`);
  }
  const signal = scenes[scene];
  const eventAt = 18 * 60 + 30;
  const now = 18 * 60 + clock;
  const leaveAt = eventAt - travel - buffer;
  const wakeAt = leaveAt - prep;
  const remainingPrep = Math.min(prep, signal.prepare);
  const arrivalAt = now + signal.wait + remainingPrep + travel;
  const countsMet = signal.counts >= 6;
  const activityMet = signal.activity === 'walking' || signal.activity === 'running';
  const confirmed = countsMet && activityMet && checkedIn;
  const decision = signal.outside ? 'Phone outside origin · route unconfirmed'
    : confirmed ? 'Counts + walking Activity + check in'
      : scene === 'shaking' ? 'Counter rose · walking unconfirmed'
        : scene === 'walking' ? 'Walking Activity · waiting for check in'
          : now < leaveAt ? 'Leave time is approaching' : 'Time to leave · no recent activity';
  return { scene, signal, eventAt, now, leaveAt, wakeAt, arrivalAt, remainingPrep,
    lateMinutes: Math.max(0, arrivalAt - eventAt), earlyMinutes: Math.max(0, eventAt - arrivalAt),
    bufferShortfall: Math.max(0, arrivalAt - (eventAt - buffer)), countsMet, activityMet, confirmed, decision };
}

export function formatTime(minutes) {
  const normalized = ((Math.round(minutes) % 1440) + 1440) % 1440;
  const hour = Math.floor(normalized / 60);
  return `${hour % 12 || 12}:${String(normalized % 60).padStart(2, '0')} ${hour >= 12 ? 'PM' : 'AM'}`;
}
