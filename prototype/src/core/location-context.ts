import { assessDeparture, type DepartureLocationFix, type Coordinates } from './departure.js';

export interface PlaceAnchor extends Coordinates { label: string; radiusMeters: number; sourceUrl?: string }
function distance(a: Coordinates, b: Coordinates) {
  const r = Math.PI / 180;
  const x = (b.longitude - a.longitude) * r * Math.cos((a.latitude + b.latitude) * r / 2);
  const y = (b.latitude - a.latitude) * r;
  return Math.hypot(x, y) * 6371000;
}

/** Conservative GPS context, independent of phone shaking, gravity, and step counts. */
export function locationContext(enabled: boolean, input: readonly DepartureLocationFix[], anchor: PlaceAnchor | undefined, now = Date.now()) {
  const fixes = enabled ? input.filter(f => f.timestamp <= now + 2000 && f.timestamp >= now - 45000).sort((a,b) => a.timestamp-b.timestamp) : [];
  const latest = fixes.at(-1);
  const empty = { enabled, status: enabled ? 'waiting' : 'off', place: anchor ?? null, current: null,
    distanceFromPlaceMeters: null, location: 'unknown', movement: 'unknown', trend: 'unknown',
    fixCount: fixes.length, spanSeconds: 0, trail: [] as {east:number;north:number}[],
    summary: enabled ? 'Waiting for live phone GPS.' : 'GPS collection is off.' };
  if (!latest) return empty;
  const streamFixes = fixes.filter(f => f.deviceId === latest.deviceId && f.sessionId === latest.sessionId);
  const usable = streamFixes.filter(f => f.accuracyMeters > 0 && f.accuracyMeters <= 25);
  const first = usable[0];
  const span = first ? (latest.timestamp - first.timestamp) / 1000 : 0;
  let movement = 'unknown';
  if (latest.accuracyMeters <= 25 && latest.speedMetersPerSecond !== undefined && latest.speedAccuracyMetersPerSecond !== undefined) {
    if (latest.speedMetersPerSecond - latest.speedAccuracyMetersPerSecond >= 1) movement = 'moving';
    else if (latest.speedMetersPerSecond + latest.speedAccuracyMetersPerSecond <= 0.5) movement = 'stationary';
  }
  if (first && span >= 10 && latest.accuracyMeters <= 25 && distance(first, latest) - first.accuracyMeters - latest.accuracyMeters >= 30) movement = 'moving';
  const base = { ...empty, status: latest.accuracyMeters > 25 ? 'imprecise' : 'live', current: latest, movement, spanSeconds: Math.max(0,span), fixCount: streamFixes.length };
  if (!anchor) return { ...base, summary: 'GPS received. Set a place reference to determine whether you are still there.' };
  const assessment = assessDeparture({plan:{leaveAt:new Date(now+60000).toISOString()}, now, origin:anchor,
    originRadiusMeters:anchor.radiusMeters, fixes:streamFixes, expectedStream:latest});
  const d = assessment.evidence.gps.distanceMeters;
  const trendDelta = first && d !== null ? d - distance(anchor, first) : 0;
  const error = first ? first.accuracyMeters + latest.accuracyMeters : Infinity;
  const trend = span >= 10 && Math.abs(trendDelta) - error >= 25 ? trendDelta > 0 ? 'away' : 'toward' : 'unknown';
  const inside = assessment.location === 'inside-origin';
  const outside = assessment.location === 'outside-origin-confirmed';
  const status = latest.accuracyMeters > 25 ? 'imprecise' : inside ? movement === 'moving' ? 'moving-near-place' : usable.length >= 3 && span >= 15 ? 'staying-near-place' : 'near-place'
    : outside ? trend === 'away' ? 'moving-away' : 'outside-place' : 'uncertain';
  const summary = status === 'imprecise' ? `GPS is too imprecise to determine whether you are at ${anchor.label}.`
    : status === 'moving-near-place' ? `GPS movement detected within the ${anchor.label} area.`
    : status === 'staying-near-place' ? `Repeated GPS fixes remain near ${anchor.label}.`
    : status === 'near-place' ? `GPS places the phone near ${anchor.label}.`
    : status === 'moving-away' ? `GPS distance from ${anchor.label} is increasing; the phone is outside its reference area.`
    : status === 'outside-place' ? `Repeated GPS fixes place the phone outside the ${anchor.label} area.`
    : `GPS uncertainty overlaps the ${anchor.label} boundary. Location is uncertain.`;
  const trail = streamFixes.slice(-60).map(f => ({east:(f.longitude-anchor.longitude)*Math.PI/180*6371000*Math.cos(anchor.latitude*Math.PI/180),north:(f.latitude-anchor.latitude)*Math.PI/180*6371000}));
  return {...base,status,summary,location:assessment.location,distanceFromPlaceMeters:d,trend,trail};
}
