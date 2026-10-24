// Mock fixtures from contracts/examples, typed with the frozen contract types.
// JSON imports widen string unions to string, so each one is cast once here.
import type {
  ConsequencesResponse,
  ContactConfirmResponse,
  DashboardEvent,
  DemoTriggerResponse,
  DemoTripResponse,
  HealthResponse,
  LocationResponse,
  OnboardingCompleteRequest,
  OnboardingCompleteResponse,
  OnboardingSessionResponse,
  PlanResponse,
  PunctualityResponse,
  SimCallServerMessage,
  SimResult,
} from './types';

import health from '@contracts/health.json';
import onboardingSession from '@contracts/onboarding_session.json';
import onboardingCompleteRequest from '@contracts/onboarding_complete_request.json';
import onboardingComplete from '@contracts/onboarding_complete.json';
import contactConfirm from '@contracts/contact_confirm.json';
import plan from '@contracts/plan.json';
import consequences from '@contracts/consequences.json';
import punctuality from '@contracts/punctuality.json';
import location from '@contracts/location.json';
import demoTrigger from '@contracts/demo_trigger.json';
import demoTrip from '@contracts/demo_trip.json';
import simResult from '@contracts/sim_result.json';
import wsPlan from '@contracts/ws_plan.json';
import wsDecision from '@contracts/ws_decision.json';
import wsCallState from '@contracts/ws_call_state.json';
import wsTurn from '@contracts/ws_turn.json';
import wsTripTick from '@contracts/ws_trip_tick.json';
import wsSimResult from '@contracts/ws_sim_result.json';
import simcallRing from '@contracts/simcall_ring.json';
import simcallSay from '@contracts/simcall_say.json';
import simcallHangup from '@contracts/simcall_hangup.json';

function typed<T>(json: unknown): T {
  return json as T;
}

export const fixtures = {
  health: typed<HealthResponse>(health),
  onboardingSession: typed<OnboardingSessionResponse>(onboardingSession),
  onboardingCompleteRequest: typed<OnboardingCompleteRequest>(onboardingCompleteRequest),
  onboardingComplete: typed<OnboardingCompleteResponse>(onboardingComplete),
  contactConfirm: typed<ContactConfirmResponse>(contactConfirm),
  plan: typed<PlanResponse>(plan),
  consequences: typed<ConsequencesResponse>(consequences),
  punctuality: typed<PunctualityResponse>(punctuality),
  location: typed<LocationResponse>(location),
  demoTrigger: typed<DemoTriggerResponse>(demoTrigger),
  demoTrip: typed<DemoTripResponse>(demoTrip),
  simResult: typed<SimResult>(simResult),
};

/** The six /ws/dashboard fixtures, in the order the mock socket replays them. */
export const dashboardEvents: DashboardEvent[] = [
  typed<DashboardEvent>(wsPlan),
  typed<DashboardEvent>(wsCallState),
  typed<DashboardEvent>(wsTurn),
  typed<DashboardEvent>(wsDecision),
  typed<DashboardEvent>(wsTripTick),
  typed<DashboardEvent>(wsSimResult),
];

export const simCallMessages = {
  ring: typed<Extract<SimCallServerMessage, { type: 'ring' }>>(simcallRing),
  say: typed<Extract<SimCallServerMessage, { type: 'say' }>>(simcallSay),
  hangup: typed<Extract<SimCallServerMessage, { type: 'hangup' }>>(simcallHangup),
};
