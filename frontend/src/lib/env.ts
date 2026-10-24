// Mock mode is the default so the frontend never waits on the backend (PRD 15.1).
export const USE_MOCKS = (import.meta.env.VITE_USE_MOCKS ?? 'true') !== 'false';

// The single seeded demo user (backend/seed/demo_user.json).
export const DEMO_USER_ID = 'u_demo';

export const TIMEZONE = 'America/Los_Angeles';

/** ws:// or wss:// URL on the current host. The Vite dev server proxies /ws to the backend. */
export function wsUrl(path: string): string {
  const proto = window.location.protocol === 'https:' ? 'wss' : 'ws';
  return `${proto}://${window.location.host}${path}`;
}
