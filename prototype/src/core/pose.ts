const FRESH_MS = 5000;
type Values = Record<string, unknown>;
interface Sample { at: number; receivedAt: number; values: number[] }
const object = (v: unknown): v is Values => !!v && typeof v === 'object' && !Array.isArray(v);
const number = (v: unknown): v is number => typeof v === 'number' && Number.isFinite(v);

/** Sensor Logger's documented quaternion/XYZ schema; phone pose never implies body pose. */
export class PoseTracker {
  private stream = '';
  private retired = new Set<string>();
  private message = -1;
  private latestAt = -Infinity;
  private samples: Partial<Record<'orientation' | 'gyroscope' | 'gravity', Sample>> = {};
  private baseline: Sample | null = null;

  ingest(body: unknown, now = Date.now()) {
    if (!object(body) || !Array.isArray(body.payload) || body.payload.length > 2000 ||
      typeof body.deviceId !== 'string' || !body.deviceId || body.deviceId.length > 200 ||
      typeof body.sessionId !== 'string' || !body.sessionId || body.sessionId.length > 200 ||
      !number(body.messageId) || !Number.isSafeInteger(body.messageId) || body.messageId < 0) return;
    const stream = JSON.stringify([body.deviceId, body.sessionId]);
    if (this.retired.has(stream) || stream === this.stream && body.messageId <= this.message) return;
    if (stream === this.stream) this.message = body.messageId;
    for (const raw of body.payload) {
      if (!object(raw) || !number(raw.time) || !object(raw.values) ||
        !['orientation', 'gyroscope', 'gravity'].includes(String(raw.name))) continue;
      const name = raw.name as 'orientation' | 'gyroscope' | 'gravity';
      if (raw.accuracy === 0) continue;
      const at = raw.time / 1e6;
      if (at < now - FRESH_MS || at > now + 2000 || stream !== this.stream && at <= this.latestAt) continue;
      const v = raw.values;
      const values = name === 'orientation' ? [v.qw, v.qx, v.qy, v.qz] : [v.x, v.y, v.z];
      if (!values.every(number)) continue;
      const norm = Math.hypot(...values as number[]);
      if (name === 'orientation' ? norm < 0.5 || norm > 1.5 : name === 'gravity' ? norm < 0.1 || norm > 20 : norm > 200) continue;
      if (stream !== this.stream) {
        if (this.stream) this.retired.add(this.stream);
        if (this.retired.size > 64) this.retired.delete(this.retired.values().next().value!);
        this.stream = stream; this.message = body.messageId; this.samples = {}; this.baseline = null;
      }
      if (at <= (this.samples[name]?.at ?? -Infinity)) continue;
      const sample = { at, receivedAt: now, values: name === 'orientation' ? (values as number[]).map(x => x / norm) : values as number[] };
      this.samples[name] = sample;
      this.latestAt = Math.max(this.latestAt, at);
      if (name === 'orientation' && (!this.baseline || at - this.baseline.at > 120000)) this.baseline = sample;
    }
  }

  snapshot(now = Date.now()) {
    const status = (s?: Sample) => !s ? 'unavailable' as const : now - s.at > FRESH_MS || now - s.receivedAt > FRESH_MS || s.at > now + 2000 ? 'stale' as const : 'live' as const;
    const orientation = this.samples.orientation;
    const gyro = this.samples.gyroscope;
    const gravity = this.samples.gravity;
    const angle = gravity ? Math.acos(Math.min(1, Math.abs(gravity.values[2]!) / Math.hypot(...gravity.values))) * 180 / Math.PI : null;
    const change = orientation && this.baseline ? 2 * Math.acos(Math.min(1, Math.abs(orientation.values.reduce((n, x, i) => n + x * this.baseline!.values[i]!, 0)))) * 180 / Math.PI : null;
    return {
      orientation: { status: status(orientation), quaternion: orientation?.values ?? null, rotationFromBaselineDegrees: change, lastSampleAt: orientation?.at ?? null },
      gyroscope: { status: status(gyro), xyz: gyro?.values ?? null, rotationRate: gyro ? Math.hypot(...gyro.values) : null, unit: 'rad/s', lastSampleAt: gyro?.at ?? null },
      phonePosition: { status: status(gravity), angleFromHorizontalDegrees: angle,
        position: status(gravity) !== 'live' || angle === null ? 'unknown' : angle <= 20 ? 'horizontal' : angle >= 70 ? 'upright' : 'tilted' },
    };
  }
}
