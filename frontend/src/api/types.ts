// Shared API types. Mirrors contracts/schemas/*.schema.json and backend/src/models/Models.hpp.
// CONTRACT FILE: change all three in the same PR and tell the team (contracts/README.md).

/** Unix epoch seconds (UTC). */
export type Timestamp = number;

export type Urgency = 'CHILL' | 'FIRM' | 'URGENT';
export type ConsentStatus = 'PENDING' | 'CONFIRMED' | 'REVOKED';
export type TaskKind =
  | 'PLAN_TOMORROW' | 'WAKE_CHECK' | 'WAKE_CALL' | 'LEAVE_REMINDER' | 'DEPARTURE_CHECK'
  | 'TRANSIT_TICK' | 'POLL_CALENDAR' | 'POLL_GMAIL' | 'POST_EVENT_LOG';
export type ConsequenceStatus = 'OK' | 'AT_RISK' | 'MISSED';
export type DecisionSource = 'jev' | 'rules';
export type DecisionKind =
  | 'email_relevance' | 'transit_speak' | 'interrupt_channel' | 'readiness' | 'snooze' | 'stop_condition';
export type TransitVerdict = 'IGNORE' | 'COMMENT' | 'ALERT' | 'REROUTE';

// ---------- section 10 ----------

export interface User {
  id: string;
  name: string;
  phone: string;
  timezone: string;
  home_area: string;
  archetype_id: string;
  alarm_time: string; // "HH:MM"
  grace_minutes: number;
  preferred_mode: Record<string, string>; // trip type -> mode
  prep_minutes: Record<string, number>; // casual | formal -> minutes
  warmth: number;
  harshness: number;
  humor: number;
  verbosity: number;
}

export interface EmergencyContact {
  id: string;
  user_id: string;
  spoken_name: string;
  relationship: string;
  phone: string;
  consent: ConsentStatus;
  added_ts: Timestamp;
}

export interface Event {
  id: string;
  user_id: string;
  title: string;
  location: string;
  event_type: string;
  source: 'calendar' | 'email';
  start_ts: Timestamp;
  end_ts: Timestamp;
  canceled: boolean;
}

export interface LeavePlan {
  event_id: string;
  mode: string;
  prep_minutes: number;
  travel_minutes: number;
  buffer_minutes: number;
  reminder_at: Timestamp;
  leave_by: Timestamp;
  urgency: Urgency;
  urgency_score: number;
  reasoning: string[];
}

export interface AgentTask {
  due_at: Timestamp;
  user_id: string;
  kind: TaskKind;
  payload: Record<string, unknown>;
}

export interface Decision {
  user_id: string;
  kind: DecisionKind;
  verdict: string;
  source: DecisionSource;
  confidence: number;
  ts: Timestamp;
  latency_ms: number;
  inputs: Record<string, unknown>;
}

export interface Outcome {
  user_id: string;
  event_id: string;
  event_type: string;
  mode: string;
  planned_leave: Timestamp;
  actual_leave: Timestamp;
  arrival: Timestamp;
}

export interface PunctualityStats {
  prep_bias: number;
  travel_factor: number;
  arrival_delta: number;
  snooze_inflation: number;
  samples: number;
}

export interface Call {
  id: string;
  user_id: string;
  provider_sid: string;
  purpose: 'wake' | 'awake_verify' | 'leave_reminder' | 'departure_check';
  state: 'queued' | 'ringing' | 'in_progress' | 'completed' | 'no_answer' | 'failed';
  attempt: number;
  turns: number;
}

export interface Turn {
  call_id: string;
  speaker: 'agent' | 'user';
  text: string;
  intent: string;
  ts: Timestamp;
}

// ---------- section 11 responses ----------

export interface HealthResponse {
  status: 'ok';
  version: string;
  uptime_s: number;
  providers: { voice: string; llm: string; reflex: string; calendar: string; email: string; router: string; transit: string };
}

export interface ErrorResponse {
  error: string;
  message: string;
}

export interface OnboardingQuestion {
  slot: string;
  prompt: string;
}

export interface OnboardingSessionResponse {
  provider: 'fake' | 'openai';
  client_secret: string;
  expires_at: Timestamp;
  model: string;
  voice: string;
  instructions: string;
  questions: OnboardingQuestion[];
}

export interface NewContact {
  spoken_name: string;
  relationship: string;
  phone: string;
}

export interface OnboardingCompleteRequest {
  profile: {
    name: string;
    phone: string;
    timezone: string;
    home_area: string;
    alarm_time: string;
    sets_own_alarm: boolean;
    preferred_mode: Record<string, string>;
    prep_minutes: Record<string, number>;
    harshness_pref: number;
  };
  contacts: NewContact[];
}

export interface OnboardingCompleteResponse {
  user: User;
  contacts: EmergencyContact[];
  oauth_url: string;
}

export interface ContactConfirmResponse {
  contact: EmergencyContact;
}

export interface LadderStep {
  step: 'CALL_1' | 'CALL_2' | 'CALL_3' | 'AWAKE_VERIFY';
  at: Timestamp;
  tone: string;
  notify_contacts: boolean;
}

export interface WakePlan {
  alarm_time: string;
  alarm_ts: Timestamp;
  grace_minutes: number;
  steps: LadderStep[];
}

export interface PlanResponse {
  user_id: string;
  date: string;
  timezone: string;
  generated_at: Timestamp;
  events: Event[];
  plans: LeavePlan[];
  wake: WakePlan;
  reasoning: string[];
}

export interface ConsequenceItem {
  event_id: string;
  title: string;
  start_ts: Timestamp;
  status: ConsequenceStatus;
  minutes_late: number;
}

export interface ConsequencesResponse {
  user_id: string;
  delay_minutes: number;
  worst: ConsequenceStatus;
  items: ConsequenceItem[];
  summary: string;
}

export interface ChannelResponse {
  channel: 'call' | 'earbud' | 'text';
  time_of_day: 'morning' | 'day' | 'evening';
  answer_rate: number;
  samples: number;
}

export interface TransportCount {
  destination_type: string;
  distance_bucket: string;
  counts: Record<string, number>;
  predicted: string;
}

export interface PunctualityResponse {
  user_id: string;
  by_event_type: Record<string, PunctualityStats>;
  by_mode: Record<string, PunctualityStats>;
  channel_response: ChannelResponse[];
  transport: TransportCount[];
  headlines: string[];
}

export interface LocationRequest {
  user_id: string;
  lat: number;
  lng: number;
  accuracy_m: number;
  ts: Timestamp;
}

export interface LocationResponse {
  ok: boolean;
  departed: boolean;
  distance_from_home_m: number;
}

export type DemoScenario = 'wake_ladder' | 'leave_reminder' | 'email_drop';

export interface DemoTriggerRequest {
  user_id: string;
  scenario: DemoScenario;
}

export interface DemoTriggerResponse {
  ok: boolean;
  scenario: string;
  call_id: string;
  message: string;
}

export interface DemoTripRequest {
  trip: string;
  speed: number;
}

export interface DemoTripResponse {
  ok: boolean;
  trip: string;
  speed: number;
  ticks: number;
  duration_s: number;
}

export interface SimLoadRequest {
  users: number;
  threads: number;
}

export interface CallsPerMinuteBucket {
  minute: string;
  calls: number;
}

export interface SimResult {
  run_id: string;
  users: number;
  threads: number;
  runtime_ms: number;
  tasks_scheduled: number;
  llm_calls: number;
  calls_per_minute: CallsPerMinuteBucket[];
  cost_per_user_month_usd: number;
  cost_breakdown: Record<string, number>;
}

export interface Reroute {
  reason: string;
  suggestion: string;
  new_eta_ts: Timestamp;
}

export interface TripTick {
  trip_id: string;
  seq: number;
  ts: Timestamp;
  lat: number;
  lng: number;
  route: string;
  next_stop: string;
  eta_ts: Timestamp;
  scheduled_arrival_ts: Timestamp;
  delay_minutes: number;
  on_route: boolean;
  verdict: TransitVerdict;
  line: string;
  reroute: Reroute | null;
}

// ---------- websockets ----------

interface Envelope<T extends string, D> {
  type: T;
  ts: Timestamp;
  data: D;
}

/** Server -> browser on /ws/dashboard. */
export type DashboardEvent =
  | Envelope<'plan', PlanResponse>
  | Envelope<'decision', Decision>
  | Envelope<'call_state', Call>
  | Envelope<'turn', Turn>
  | Envelope<'trip_tick', TripTick>
  | Envelope<'sim_result', SimResult>;

export type DashboardEventType = DashboardEvent['type'];

/** Server -> browser on /ws/sim-call/:call_id. */
export type SimCallServerMessage =
  | Envelope<'ring', { call_id: string; purpose: string; attempt: number; caller: string }>
  | Envelope<'say', { call_id: string; text: string; audio_url: string | null; persona: string; urgency: Urgency }>
  | Envelope<'hangup', { call_id: string; reason: string }>;

/** Browser -> server on /ws/sim-call/:call_id. */
export type SimCallClientMessage =
  | Envelope<'answer', { call_id: string }>
  | Envelope<'decline', { call_id: string }>
  | Envelope<'speech', { call_id: string; text: string }>;
