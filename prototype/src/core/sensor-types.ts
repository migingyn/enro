import type { MotionSnapshot } from '../types.js';

export interface SensorSummary {
  status: 'unavailable' | 'stale' | 'live';
  lastSampleAt: number | null;
  lastReceivedAt: number | null;
  sampleCount: number;
}

/** Experimental cumulative-counter evidence, not proof of wakefulness or gait. */
export interface WalkingEvidence {
  status: 'unavailable' | 'baseline-needed' | 'stale' | 'insufficient' | 'counter-reset' | 'step-threshold-met';
  stepsSinceBaseline: number;
  threshold: number;
  thresholdMet: boolean;
  /** Current-sequence walking/running Activity corroboration required by the voice check. */
  activityCorroborated: boolean;
  baselineSteps: number | null;
  baselineAt: number | null;
  latestSteps: number | null;
  lastSampleAt: number | null;
  lastReceivedAt: number | null;
  resetCount: number;
  scopedTo: number | null;
}

export interface GravityEvidence {
  status: 'unavailable' | 'stale' | 'available';
  lastSampleAt: number | null;
  lastReceivedAt: number | null;
  vector: { x: number; y: number; z: number } | null;
  orientationChangeDegrees: number | null;
  /** Phone orientation changed; never implies walking or the body's posture. */
  pickupPossible: boolean;
}

export interface ActivityDiagnostic {
  status: 'unavailable' | 'stale' | 'unknown-schema' | 'available';
  lastSampleAt: number | null;
  lastReceivedAt: number | null;
  sensorName: string | null;
  /** Bounded values.activity label for inspection only; never a trusted classification. */
  reportedActivity: string | null;
  /** Experimental exact-label normalization; sensor labels still require physical testing. */
  classification?: 'walking' | 'running' | 'cycling' | 'driving' | 'stationary' | 'unknown' | 'unrecognized';
  /** Bounded field names and types only. No raw values or inferred classification. */
  fieldTypes: Record<string, string>;
}

export interface MotionEvidenceSnapshot extends MotionSnapshot {
  sessionId: string | null;
  /** Bounded names observed in this stream, including unsupported/invalid sensor readings. */
  observedSensors: string[];
  walking: WalkingEvidence;
  gravity: GravityEvidence;
  activity: ActivityDiagnostic;
  sensors: {
    accelerometer: SensorSummary;
    pedometer: SensorSummary;
    gravity: SensorSummary;
    activity: SensorSummary;
  };
}
