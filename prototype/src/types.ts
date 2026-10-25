export interface EnroEvent {
  id: string;
  title: string;
  startAt: string;
  timeZone: string;
  location: string;
  requirements: string[];
  demoInvitation?: { subject: string; body: string };
  status: 'confirmed' | 'tentative' | 'cancelled';
  source: { type: 'calendar' | 'email' | 'manual'; id: string; url?: string; account?: string };
}
export type TravelMode = 'DRIVE' | 'WALK' | 'TRANSIT';
export interface RouteEstimate { durationSeconds: number; distanceMeters?: number; fetchedAt: string; provider: 'google-routes' | 'osrm'; }
export interface MorningPlan {
  id: string; event: EnroEvent; origin: string; travelMode: TravelMode;
  originCoordinates?: { latitude: number; longitude: number };
  destinationCoordinates?: { latitude: number; longitude: number };
  originRadiusMeters?: number;
  destinationRadiusMeters?: number;
  accessMinutes?: number;
  reminderKinds?: Array<'wake' | 'leave'>;
  flatInactivity?: { durationSeconds: 60 | 300; armedAt: string };
  preparationMinutes: number; bufferMinutes: number; route: RouteEstimate;
  wakeAt: string; leaveAt: string; createdAt: string; armed: boolean;
}
export interface MotionSnapshot {
  status: 'unavailable' | 'stale' | 'stationary' | 'moving';
  lastSampleAt: number | null; lastReceivedAt: number | null;
  sampleCount: number; movementSamples: number; movementSeconds: number;
  latestMagnitude: number | null; latencyMs: number | null;
  deviceId: string | null; source: 'sensor-logger' | null;
}
export type CallStatus = 'starting' | 'queued' | 'ringing' | 'in-progress' | 'completed' | 'busy' | 'failed' | 'no-answer' | 'canceled' | 'unknown';
export interface CallSession {
  id: string; kind: 'test' | 'wake' | 'leave'; planId?: string;
  callSid?: string; status: CallStatus; createdAt: string;
  answeredAt?: string; finishedAt?: string; verbalConfirmed: boolean;
  movementObserved: boolean; rounds: number; result?: string;
  greeting: string; lastSpeech?: string; callbackSequence?: number;
  motionEvidence?: MotionSnapshot;
  rehearsal?: boolean;
  inactivityEvidence?: { observedSeconds: number; requiredSeconds: number; lastSampleAt: number | null; observedAt: string };
  tripEvidence?: { status: string; summary: string; phonePosition: string; activity: string; observedAt: string };
  validation?: { outcome: 'false-positive'; source: 'user-report'; note: string; recordedAt: string };
}
export interface AuditEntry { at: string; type: string; message: string; }
export interface AppState {
  events: EnroEvent[]; plans: MorningPlan[]; calls: CallSession[];
  acceptedEmailIds?: string[];
  jobs: Record<string, { state: 'dispatching' | 'sent' | 'failed' | 'unknown' | 'missed' | 'satisfied'; at: string; callId?: string; error?: string }>;
  audit: AuditEntry[];
}
