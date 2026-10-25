import { EXPERIMENTAL_DEPARTURE_THRESHOLDS, type DepartureLocationFix, type SensorStream } from './departure.js';

const MAX_BATCH = 2_000;
const MAX_FIXES = 120;
const MAX_STREAMS = 64;
interface StreamState { messageId: number; lastSampleAt: number }
interface StoredFix { fix: DepartureLocationFix; receivedAt: number }
type RecordValue = Record<string, unknown>;
const record = (value: unknown): value is RecordValue => value !== null && typeof value === 'object' && !Array.isArray(value);
const identifier = (value: unknown): value is string => typeof value === 'string' && value.trim().length > 0 && value.length <= 200;
const finite = (value: unknown): value is number => typeof value === 'number' && Number.isFinite(value);

/**
 * Opt-in, in-memory GPS collector. It is disabled by default and discards all
 * evidence when disabled. The caller must tie enabling to explicit user consent.
 * Sensor Logger schema: https://github.com/tszheichoi/awesome-sensor-logger/blob/main/PUSHING.md
 * Only `location` records are read. `time` is UTC epoch nanoseconds and
 * `horizontalAccuracy` is the reported horizontal accuracy radius in meters.
 */
export class LocationTracker {
  private enabled = false;
  private retained: StoredFix[] = [];
  private streams = new Map<string, StreamState>();
  private activeStream: string | null = null;
  private retired = new Set<string>();
  private latestAt = -Infinity;
  private diagnosticState: { reason:string; fields:string[]; sampleAgeMs:number|null; accuracyMeters:number|null } = {reason:'No Location packet received.',fields:[],sampleAgeMs:null,accuracyMeters:null};

  diagnostic() { return {...this.diagnosticState,fields:[...this.diagnosticState.fields]}; }

  setEnabled(enabled: boolean): void {
    if (typeof enabled !== 'boolean') throw new Error('Location preference must be a boolean.');
    this.enabled = enabled;
    if (!enabled) this.clear();
  }

  clear(): void {
    this.retained = [];
    this.streams.clear();
    this.activeStream = null;
    this.retired.clear(); this.latestAt = -Infinity;
    this.diagnosticState = {reason:'No Location packet received.',fields:[],sampleAgeMs:null,accuracyMeters:null};
  }

  ingest(body: unknown, receivedAt = Date.now()): { accepted: number; ignored: number } {
    const count = record(body) && Array.isArray(body.payload) ? body.payload.length : 1;
    if (!this.enabled || !finite(receivedAt) || !record(body) || !Array.isArray(body.payload)
      || body.payload.length > MAX_BATCH || !identifier(body.deviceId) || !identifier(body.sessionId)
      || !Number.isSafeInteger(body.messageId) || Number(body.messageId) < 0) return { accepted: 0, ignored: count };
    const key = JSON.stringify([body.deviceId, body.sessionId]);
    if (this.retired.has(key)) return { accepted: 0, ignored: count };
    const prior = this.streams.get(key);
    if (prior && Number(body.messageId) <= prior.messageId) return { accepted: 0, ignored: count };
    const stream: StreamState = { messageId: Number(body.messageId), lastSampleAt: prior?.lastSampleAt ?? -Infinity };
    this.streams.delete(key);
    this.streams.set(key, stream);
    if (this.streams.size > MAX_STREAMS) this.streams.delete(this.streams.keys().next().value!);
    const t = EXPERIMENTAL_DEPARTURE_THRESHOLDS;
    let accepted = 0;
    for (const reading of body.payload) {
      if (!record(reading) || reading.name !== 'location') continue;
      this.diagnosticState = {reason:'Invalid Location timestamp or values.', fields:record(reading.values) ? Object.keys(reading.values).slice(0,20).map(k=>k.replace(/[^a-zA-Z0-9_]/g,'').slice(0,48)) : [],
        sampleAgeMs:finite(reading.time) ? receivedAt-reading.time/1e6 : null,
        accuracyMeters:record(reading.values) && finite(reading.values.horizontalAccuracy) ? reading.values.horizontalAccuracy : null};
      if (!finite(reading.time) || !record(reading.values)) continue;
      const { latitude, longitude, horizontalAccuracy } = reading.values;
      if (!finite(latitude) || Math.abs(latitude) > 90 || !finite(longitude) || Math.abs(longitude) > 180
        || !finite(horizontalAccuracy) || horizontalAccuracy <= 0 || horizontalAccuracy > 100_000) {this.diagnosticState.reason='Location coordinates or horizontalAccuracy are invalid.';continue;}
      const timestamp = reading.time / 1_000_000;
      if (timestamp < receivedAt - t.gpsFreshMs || timestamp > receivedAt + t.futureToleranceMs || timestamp <= stream.lastSampleAt) {this.diagnosticState.reason='Location timestamp is stale, in the future, or already seen.';continue;}
      if (key !== this.activeStream) {
        if (timestamp <= this.latestAt) continue;
        if (this.activeStream) this.retired.add(this.activeStream);
        if (this.retired.size > MAX_STREAMS) this.retired.delete(this.retired.values().next().value!);
        this.retained = [];
        this.activeStream = key;
      }
      stream.lastSampleAt = timestamp;
      this.latestAt = Math.max(this.latestAt, timestamp);
      const optional = (value: unknown, maximum: number) => finite(value) && value >= 0 && value <= maximum ? value : undefined;
      const speed = optional(reading.values.speed, 100);
      const speedAccuracy = optional(reading.values.speedAccuracy, 100);
      const bearing = optional(reading.values.bearing, 360);
      this.retained.push({ fix: {
        deviceId: body.deviceId, sessionId: body.sessionId,
        latitude, longitude, accuracyMeters: horizontalAccuracy, timestamp,
        ...(speed === undefined ? {} : {speedMetersPerSecond:speed}),
        ...(speedAccuracy === undefined ? {} : {speedAccuracyMetersPerSecond:speedAccuracy}),
        ...(bearing === undefined ? {} : {bearingDegrees:bearing}),
      }, receivedAt });
      accepted++;
      this.diagnosticState.reason='Valid Location fix received.';
    }
    this.prune(receivedAt);
    return { accepted, ignored: count - accepted };
  }

  fixes(now = Date.now(), expectedStream?: SensorStream): DepartureLocationFix[] {
    if (!finite(now)) throw new Error('Invalid location snapshot time.');
    if (!this.enabled) return [];
    this.prune(now);
    if (expectedStream && this.activeStream !== JSON.stringify([expectedStream.deviceId, expectedStream.sessionId])) return [];
    const t = EXPERIMENTAL_DEPARTURE_THRESHOLDS;
    return this.retained.filter(({ fix, receivedAt }) => fix.timestamp <= now + t.futureToleranceMs
      && receivedAt >= now - t.gpsFreshMs && receivedAt <= now + t.futureToleranceMs)
      .map(({ fix }) => ({ ...fix }));
  }

  private prune(now: number): void {
    const cutoff = now - EXPERIMENTAL_DEPARTURE_THRESHOLDS.gpsFreshMs;
    this.retained = this.retained.filter(({ fix, receivedAt }) => fix.timestamp >= cutoff && receivedAt >= cutoff).slice(-MAX_FIXES);
  }
}
