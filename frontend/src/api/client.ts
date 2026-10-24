// Typed client, one function per PRD section 11 route.
// Mock mode resolves each call to its contracts/examples fixture; live mode uses fetch()
// on relative paths, which the Vite dev server proxies to the backend.
import type {
  ConsequencesResponse,
  ContactConfirmResponse,
  DemoScenario,
  DemoTriggerResponse,
  DemoTripRequest,
  DemoTripResponse,
  ErrorResponse,
  HealthResponse,
  LocationRequest,
  LocationResponse,
  OnboardingCompleteRequest,
  OnboardingCompleteResponse,
  OnboardingSessionResponse,
  PlanResponse,
  PunctualityResponse,
  SimLoadRequest,
  SimResult,
} from './types';
import { fixtures } from './fixtures';
import { DEMO_USER_ID, USE_MOCKS } from '../lib/env';

export class ApiError extends Error {
  constructor(
    readonly status: number,
    readonly body: ErrorResponse | null,
  ) {
    super(body ? `${body.error}: ${body.message}` : `HTTP ${status}`);
  }
}

const MOCK_LATENCY_MS = 150;

function mock<T>(value: T): Promise<T> {
  // Clone so callers can't mutate the shared fixture.
  return new Promise((resolve) =>
    setTimeout(() => resolve(structuredClone(value)), MOCK_LATENCY_MS),
  );
}

async function request<T>(method: 'GET' | 'POST', path: string, body?: unknown): Promise<T> {
  const res = await fetch(path, {
    method,
    headers: body === undefined ? undefined : { 'Content-Type': 'application/json' },
    body: body === undefined ? undefined : JSON.stringify(body),
  });
  if (!res.ok) {
    const err = (await res.json().catch(() => null)) as ErrorResponse | null;
    throw new ApiError(res.status, err);
  }
  return (await res.json()) as T;
}

const userPath = (userId: string) => `/api/users/${encodeURIComponent(userId)}`;

export function health(): Promise<HealthResponse> {
  return USE_MOCKS ? mock(fixtures.health) : request('GET', '/health');
}

export function onboardingSession(): Promise<OnboardingSessionResponse> {
  return USE_MOCKS
    ? mock(fixtures.onboardingSession)
    : request('POST', '/api/onboarding/session', {});
}

export function onboardingComplete(
  body: OnboardingCompleteRequest,
): Promise<OnboardingCompleteResponse> {
  return USE_MOCKS
    ? mock(fixtures.onboardingComplete)
    : request('POST', '/api/onboarding/complete', body);
}

export function confirmContact(contactId: string): Promise<ContactConfirmResponse> {
  return USE_MOCKS
    ? mock(fixtures.contactConfirm)
    : request('POST', `/api/contacts/${encodeURIComponent(contactId)}/confirm`, {});
}

/** date is "YYYY-MM-DD"; omit for the backend's default (tomorrow). */
export function getPlan(userId: string = DEMO_USER_ID, date?: string): Promise<PlanResponse> {
  if (USE_MOCKS) return mock(fixtures.plan);
  const q = date ? `?date=${encodeURIComponent(date)}` : '';
  return request('GET', `${userPath(userId)}/plan${q}`);
}

export function getConsequences(
  userId: string = DEMO_USER_ID,
  delayMinutes = 11,
): Promise<ConsequencesResponse> {
  return USE_MOCKS
    ? mock(fixtures.consequences)
    : request('GET', `${userPath(userId)}/consequences?delay=${delayMinutes}`);
}

export function getPunctuality(userId: string = DEMO_USER_ID): Promise<PunctualityResponse> {
  return USE_MOCKS ? mock(fixtures.punctuality) : request('GET', `${userPath(userId)}/punctuality`);
}

export function postLocation(body: LocationRequest): Promise<LocationResponse> {
  return USE_MOCKS ? mock(fixtures.location) : request('POST', '/api/location', body);
}

export function demoTrigger(
  scenario: DemoScenario,
  userId: string = DEMO_USER_ID,
): Promise<DemoTriggerResponse> {
  if (USE_MOCKS) {
    return mock({ ...fixtures.demoTrigger, scenario, message: `${scenario} started (mock).` });
  }
  return request('POST', '/api/demo/trigger', { user_id: userId, scenario });
}

export function demoTrip(body: DemoTripRequest = { trip: 'bus_trip_01', speed: 10 }): Promise<DemoTripResponse> {
  return USE_MOCKS ? mock(fixtures.demoTrip) : request('POST', '/api/demo/trip', body);
}

export function simLoad(body: SimLoadRequest = { users: 100000, threads: 8 }): Promise<SimResult> {
  return USE_MOCKS ? mock(fixtures.simResult) : request('POST', '/api/sim/load', body);
}
