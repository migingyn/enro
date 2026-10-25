import type { ActivityDiagnostic, GravityEvidence, MotionEvidenceSnapshot, SensorSummary, WalkingEvidence } from './sensor-types.js';
export type { MotionEvidenceSnapshot, WalkingEvidence, GravityEvidence, ActivityDiagnostic, SensorSummary } from './sensor-types.js';

export const MOTION_FRESH_MS = 5_000;
/** Native pedometer updates can be event-driven; separate from acceleration freshness. */
export const PEDOMETER_FRESH_MS = 15_000;
/** Experimental overlap window; event-driven native Activity can fail this check. */
export const ACTIVITY_PEDOMETER_OVERLAP_MS = 5_000;
export const DEFAULT_WALKING_THRESHOLD = 6;
const FUTURE_TOLERANCE_MS = 2_000;
const RETAIN_MS = 120_000;
const MAX_BATCH = 2_000;
const MAX_STREAMS = 64;
const MOVEMENT_THRESHOLD = 1.2;

interface Timed { at: number; receivedAt: number }
interface Acceleration extends Timed { magnitude: number; moving: boolean }
interface Gravity extends Timed { x: number; y: number; z: number }
interface Pedometer extends Timed { steps: number; generation: number; reset: boolean }
interface Activity extends Timed { sensorName: string; fieldTypes: Record<string, string>; reportedActivity: string | null }
type Sensor = 'accelerometer' | 'gravity' | 'pedometer' | 'activity';
interface Stream { messageId: number; lastAt: Partial<Record<Sensor, number>>; activated: boolean; observedSensors: string[] }
type RecordValue = Record<string, unknown>;

function record(value: unknown): value is RecordValue { return value !== null && typeof value === 'object' && !Array.isArray(value); }
function finite(value: unknown): value is number { return typeof value === 'number' && Number.isFinite(value); }
function identifier(value: unknown): value is string { return typeof value === 'string' && value.trim().length > 0 && value.length <= 200; }
function reportedActivity(value: unknown): string | null {
  return typeof value === 'string' && value.length <= 40 && /^[A-Za-z0-9 _-]+$/.test(value) && value.trim()
    ? value.trim() : null;
}
function activityClassification(label: string | null): NonNullable<ActivityDiagnostic['classification']> {
  // Experimental mapping of exact labels only. Apple exposes these activity
  // categories, but Sensor Logger's string schema is not officially documented.
  switch (label?.toLowerCase()) {
    case 'walking': return 'walking';
    case 'running': return 'running';
    case 'cycling': return 'cycling';
    case 'automotive':
    case 'driving': return 'driving';
    case 'stationary': return 'stationary';
    case 'unknown': return 'unknown';
    default: return 'unrecognized';
  }
}
function fresh(sample: Timed | null, now: number, age: number): boolean {
  return !!sample && now - sample.at <= age && sample.at - now <= FUTURE_TOLERANCE_MS &&
    now - sample.receivedAt <= age && sample.receivedAt <= now + FUTURE_TOLERANCE_MS;
}
function inScope(sample: Timed | null, since: number): boolean {
  // Future-tolerated device timestamps received before answer cannot count.
  return !!sample && sample.at >= since && sample.receivedAt >= since;
}
function retain<T extends Timed>(samples: T[], now: number, max: number): T[] {
  const first = samples.findIndex(sample => sample.at >= now - RETAIN_MS);
  return samples.slice(Math.max(first === -1 ? samples.length : first, samples.length - max));
}
function summary(latest: Timed | null, count: number, now: number, age: number): SensorSummary {
  return { status: !latest ? 'unavailable' : fresh(latest, now, age) ? 'live' : 'stale',
    lastSampleAt: latest?.at ?? null, lastReceivedAt: latest?.receivedAt ?? null, sampleCount: count };
}
function shape(reading: RecordValue): Record<string, string> {
  const fields: Array<[string, string]> = [];
  const visit = (value: RecordValue, prefix: string, depth: number) => {
    for (const [rawKey, item] of Object.entries(value)) {
      if (fields.length >= 32) break;
      if (!prefix && ['name', 'time'].includes(rawKey)) continue;
      const key = rawKey.replace(/[^\w -]/g, '').slice(0, 48);
      if (!key) continue;
      const field = prefix ? prefix + '.' + key : key;
      if (record(item) && depth < 2) visit(item, field, depth + 1);
      else fields.push([field, item === null ? 'null' : Array.isArray(item) ? 'array' : typeof item]);
    }
  };
  visit(reading, '', 0);
  return Object.fromEntries(fields);
}

/**
 * Documented Sensor Logger push schema: UTC epoch nanoseconds, gravity/linear
 * acceleration XYZ, and pedometer values.steps. Activity is absent from the
 * published schema, so only bounded shape metadata and an untrusted, short
 * values.activity label are retained. Exact allowlisted labels have an experimental
 * normalization; unknown strings are never interpreted by substring matching.
 * Source: https://github.com/tszheichoi/awesome-sensor-logger/blob/main/PUSHING.md
 * Location is deliberately ignored; an explicitly enabled location tracker is separate.
 *
 * Existing status/movement fields remain acceleration diagnostics. Only the
 * walking extension can meet the experimental per-call step threshold, and the
 * caller must still require spoken confirmation. No method infers wakefulness.
 */
export class MotionTracker {
  private samples: Acceleration[] = [];
  private gravitySamples: Gravity[] = [];
  private stepSamples: Pedometer[] = [];
  private streams = new Map<string, Stream>();
  private activeStream: string | null = null;
  private lastObservedStream: string | null = null;
  private deviceId: string | null = null;
  private sessionId: string | null = null;
  private latest: Acceleration | null = null;
  private latestGravity: Gravity | null = null;
  private latestStep: Pedometer | null = null;
  private latestActivity: Activity | null = null;
  private latestAnyAt = -Infinity;
  private readonly walkingThreshold: number;

  constructor(options: { walkingThreshold?: number } = {}) {
    const threshold = options.walkingThreshold ?? DEFAULT_WALKING_THRESHOLD;
    if (!Number.isInteger(threshold) || threshold < 5 || threshold > 10) throw new Error('Walking threshold must be an integer from 5 to 10 steps.');
    this.walkingThreshold = threshold;
  }

  ingest(body: unknown, receivedAt = Date.now()): { accepted: number; ignored: number } {
    const count = record(body) && Array.isArray(body.payload) ? body.payload.length : 1;
    if (!finite(receivedAt) || !record(body) || !Array.isArray(body.payload) || body.payload.length > MAX_BATCH ||
        !identifier(body.deviceId) || !identifier(body.sessionId) || !finite(body.messageId) ||
        !Number.isSafeInteger(body.messageId) || body.messageId < 0) return { accepted: 0, ignored: count };
    const key = JSON.stringify([body.deviceId, body.sessionId]);
    let stream = this.streams.get(key);
    if (stream && (body.messageId <= stream.messageId || (stream.activated && key !== this.activeStream))) {
      return { accepted: 0, ignored: count };
    }
    stream ||= { messageId: body.messageId, lastAt: {}, activated: false, observedSensors: [] };
    stream.messageId = body.messageId;
    this.streams.delete(key);
    this.streams.set(key, stream);
    this.lastObservedStream = key;
    if (this.streams.size > MAX_STREAMS) this.streams.delete(this.streams.keys().next().value!);
    let accepted = 0;
    for (const reading of body.payload) {
      if (record(reading) && typeof reading.name === 'string') {
        const name = reading.name.replace(/[^A-Za-z0-9 _.\-]/g, '').trim().slice(0, 64);
        if (name && stream.observedSensors.length < 16 && !stream.observedSensors.includes(name)) stream.observedSensors.push(name);
      }
      if (!record(reading) || typeof reading.name !== 'string' || reading.name.length > 64 || !finite(reading.time)) continue;
      const sensor: Sensor | null = ['accelerometer', 'pedometer', 'gravity'].includes(reading.name)
        ? reading.name as Sensor : /activity/i.test(reading.name) ? 'activity' : null;
      if (!sensor) continue;
      const at = reading.time / 1_000_000;
      const maxAge = sensor === 'pedometer' || sensor === 'activity' ? PEDOMETER_FRESH_MS : MOTION_FRESH_MS;
      if (at < receivedAt - maxAge || at > receivedAt + FUTURE_TOLERANCE_MS || at <= (stream.lastAt[sensor] ?? -Infinity)) continue;
      if (this.activeStream !== key && at <= this.latestAnyAt) continue;
      const values = record(reading.values) ? reading.values : {};
      if (sensor === 'pedometer' && (!finite(values.steps) || !Number.isSafeInteger(values.steps) || values.steps < 0 || values.steps > 1_000_000_000)) continue;
      if (sensor === 'accelerometer' || sensor === 'gravity') {
        if (![values.x, values.y, values.z].every(value => finite(value) && Math.abs(value) <= 200) || reading.accuracy === 0) continue;
        const magnitude = Math.hypot(values.x as number, values.y as number, values.z as number);
        if (sensor === 'gravity' && (magnitude < 0.1 || magnitude > 20)) continue;
      }
      if (this.activeStream !== key) {
        this.samples = []; this.gravitySamples = []; this.stepSamples = [];
        this.latest = null; this.latestGravity = null; this.latestStep = null; this.latestActivity = null;
        this.activeStream = key; this.deviceId = body.deviceId; this.sessionId = body.sessionId;
        stream.activated = true;
      }
      stream.lastAt[sensor] = at;
      this.latestAnyAt = Math.max(this.latestAnyAt, at);
      if (sensor === 'accelerometer') {
        const magnitude = Math.hypot(values.x as number, values.y as number, values.z as number);
        this.latest = { at, receivedAt, magnitude, moving: magnitude >= MOVEMENT_THRESHOLD };
        this.samples.push(this.latest);
      } else if (sensor === 'gravity') {
        this.latestGravity = { at, receivedAt, x: values.x as number, y: values.y as number, z: values.z as number };
        this.gravitySamples.push(this.latestGravity);
      } else if (sensor === 'pedometer') {
        const steps = values.steps as number;
        const previous = this.latestStep;
        const gap = previous ? at - previous.at : 0;
        // Reset on counter decreases, feed gaps, or implausible jumps rather than
        // crediting an unexplained counter discontinuity as new walking.
        const reset = !!previous && (steps < previous.steps || gap > PEDOMETER_FRESH_MS ||
          steps - previous.steps > Math.max(10, gap / 1000 * 5 + 2));
        this.latestStep = { at, receivedAt, steps, generation: (previous?.generation ?? 0) + Number(reset), reset };
        this.stepSamples.push(this.latestStep);
      } else {
        this.latestActivity = { at, receivedAt, sensorName: reading.name, fieldTypes: shape(reading), reportedActivity: reportedActivity(values.activity) };
      }
      accepted++;
    }
    this.prune(receivedAt);
    return { accepted, ignored: count - accepted };
  }

  /**
   * Pass the exact answeredAt epoch milliseconds as since for a call. The
   * FIRST post-answer pedometer reading is its baseline; pre-answer steps and
   * the first reading's historical count are never credited. Without that
   * explicit scope, walking.thresholdMet is always false. Buffers retain at
   * most 120 seconds; if a baseline expires, the replacement is conservative.
   */
  snapshot(now = Date.now(), since = -Infinity): MotionEvidenceSnapshot {
    if (!finite(now) || (since !== -Infinity && !finite(since))) throw new Error('Invalid motion snapshot time.');
    this.prune(now);
    const latest = inScope(this.latest, since) ? this.latest : null;
    const samples = this.samples.filter(sample => inScope(sample, since));
    const moving = samples.filter(sample => sample.moving);
    const recentMoving = moving.filter(sample => fresh(sample, now, MOTION_FRESH_MS));
    const recentSeconds = new Set(recentMoving.map(sample => Math.floor(sample.at / 1000)));
    const spansSecond = recentMoving.length >= 2 && recentMoving.at(-1)!.at - recentMoving[0]!.at >= 1000;
    const step = inScope(this.latestStep, since) ? this.latestStep : null;
    const gravity = inScope(this.latestGravity, since) ? this.latestGravity : null;
    const activity = inScope(this.latestActivity, since) ? this.latestActivity : null;
    const classification = activity ? activityClassification(activity.reportedActivity) : 'unknown';
    const activityLive = fresh(activity, now, PEDOMETER_FRESH_MS);
    const scopedSteps = this.stepSamples.filter(sample => inScope(sample, since));
    const walking = this.walking(now, since, scopedSteps, this.latestStep);
    const activityCorroborated = finite(since) && walking.thresholdMet && activityLive && !!activity &&
      (classification === 'walking' || classification === 'running') && walking.baselineAt !== null &&
      walking.lastSampleAt !== null && activity.at >= walking.baselineAt && activity.receivedAt >= walking.baselineAt &&
      Math.abs(activity.at - walking.lastSampleAt) <= ACTIVITY_PEDOMETER_OVERLAP_MS;
    const scopedGravity = this.gravitySamples.filter(sample => inScope(sample, since));
    const any = latest || step || gravity || activity;
    return {
      status: !latest ? 'unavailable' : !fresh(latest, now, MOTION_FRESH_MS) ? 'stale' : recentSeconds.size >= 2 && spansSecond ? 'moving' : 'stationary',
      lastSampleAt: latest?.at ?? null, lastReceivedAt: latest?.receivedAt ?? null,
      sampleCount: samples.length, movementSamples: moving.length,
      movementSeconds: new Set(moving.map(sample => Math.floor(sample.at / 1000))).size,
      latestMagnitude: latest?.magnitude ?? null,
      latencyMs: latest ? Math.max(0, latest.receivedAt - latest.at) : null,
      deviceId: any ? this.deviceId : null, sessionId: any ? this.sessionId : null, source: any ? 'sensor-logger' : null,
      observedSensors: [...(this.streams.get(this.activeStream ?? this.lastObservedStream ?? '')?.observedSensors ?? [])],
      walking: { ...walking, activityCorroborated },
      gravity: this.gravity(now, scopedGravity, gravity),
      activity: { status: !activity ? 'unavailable' : !activityLive ? 'stale' : classification === 'unrecognized' ? 'unknown-schema' : 'available',
        lastSampleAt: activity?.at ?? null, lastReceivedAt: activity?.receivedAt ?? null,
        sensorName: activity?.sensorName ?? null, reportedActivity: activity?.reportedActivity ?? null, classification, fieldTypes: { ...activity?.fieldTypes } },
      sensors: {
        accelerometer: summary(latest, samples.length, now, MOTION_FRESH_MS),
        pedometer: summary(step, scopedSteps.length, now, PEDOMETER_FRESH_MS),
        gravity: summary(gravity, scopedGravity.length, now, MOTION_FRESH_MS),
        activity: summary(activity, Number(!!activity), now, PEDOMETER_FRESH_MS),
      },
    };
  }

  private walking(now: number, since: number, samples: Pedometer[], latest: Pedometer | null): WalkingEvidence {
    const scopedTo = finite(since) ? since : null;
    const baseline = scopedTo === null || !latest ? null : samples.find(sample => sample.generation === latest.generation) ?? null;
    const delta = baseline && latest ? latest.steps - baseline.steps : 0;
    const live = fresh(latest, now, PEDOMETER_FRESH_MS);
    const thresholdMet = !!(scopedTo !== null && baseline && latest && live && delta >= this.walkingThreshold && latest.at - baseline.at >= 1000);
    const resets = samples.filter(sample => sample.reset).length;
    return {
      status: !latest ? 'unavailable' : !live ? 'stale' : scopedTo === null || !baseline ? 'baseline-needed' : thresholdMet ? 'step-threshold-met' : resets > 0 ? 'counter-reset' : 'insufficient',
      stepsSinceBaseline: Math.max(0, delta), threshold: this.walkingThreshold, thresholdMet, activityCorroborated: false,
      baselineSteps: baseline?.steps ?? null, baselineAt: baseline?.at ?? null,
      latestSteps: latest?.steps ?? null, lastSampleAt: latest?.at ?? null, lastReceivedAt: latest?.receivedAt ?? null,
      resetCount: resets, scopedTo,
    };
  }

  private gravity(now: number, samples: Gravity[], latest: Gravity | null): GravityEvidence {
    const baseline = samples[0];
    let angle: number | null = null;
    if (baseline && latest) {
      const baselineMagnitude = Math.hypot(baseline.x, baseline.y, baseline.z);
      angle = samples.reduce((maximum, sample) => {
        const dot = (baseline.x * sample.x + baseline.y * sample.y + baseline.z * sample.z) /
          (baselineMagnitude * Math.hypot(sample.x, sample.y, sample.z));
        return Math.max(maximum, Math.acos(Math.max(-1, Math.min(1, dot))) * 180 / Math.PI);
      }, 0);
    }
    const live = fresh(latest, now, MOTION_FRESH_MS);
    return { status: !latest ? 'unavailable' : live ? 'available' : 'stale',
      lastSampleAt: latest?.at ?? null, lastReceivedAt: latest?.receivedAt ?? null,
      vector: latest ? { x: latest.x, y: latest.y, z: latest.z } : null,
      orientationChangeDegrees: angle, pickupPossible: live && angle !== null && angle >= 30 };
  }

  private prune(now: number): void {
    this.samples = retain(this.samples, now, 12_000);
    this.gravitySamples = retain(this.gravitySamples, now, 6_000);
    this.stepSamples = retain(this.stepSamples, now, 512);
  }
}
