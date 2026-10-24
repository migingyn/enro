import { useEffect, useState } from 'react';
import type { DashboardEvent } from '../api/types';
import { dashboardEvents } from '../api/fixtures';
import { USE_MOCKS, wsUrl } from '../lib/env';
import { nowTs } from '../lib/time';

export type SocketStatus = 'connecting' | 'open' | 'closed' | 'mock';

const MAX_EVENTS = 200;
const MOCK_INTERVAL_MS = 2000;
const RECONNECT_MS = 2000;

/** Live feed from /ws/dashboard, newest first. Mock mode replays the six ws_*.json fixtures. */
export function useDashboardSocket(): { events: DashboardEvent[]; status: SocketStatus } {
  const [events, setEvents] = useState<DashboardEvent[]>([]);
  const [status, setStatus] = useState<SocketStatus>(USE_MOCKS ? 'mock' : 'connecting');

  useEffect(() => {
    const push = (e: DashboardEvent) => setEvents((prev) => [e, ...prev].slice(0, MAX_EVENTS));

    if (USE_MOCKS) {
      let i = 0;
      const tick = () => {
        // Re-stamp the envelope so the feed reads as live; data keeps its fixture timestamps.
        const e = structuredClone(dashboardEvents[i % dashboardEvents.length]);
        push({ ...e, ts: nowTs() } as DashboardEvent);
        i += 1;
      };
      tick();
      const id = window.setInterval(tick, MOCK_INTERVAL_MS);
      return () => window.clearInterval(id);
    }

    let ws: WebSocket | null = null;
    let retry: number | undefined;
    let stopped = false;

    const connect = () => {
      setStatus('connecting');
      ws = new WebSocket(wsUrl('/ws/dashboard'));
      ws.onopen = () => setStatus('open');
      ws.onmessage = (msg) => {
        try {
          push(JSON.parse(String(msg.data)) as DashboardEvent);
        } catch {
          console.warn('dashboard: bad message', msg.data);
        }
      };
      ws.onclose = () => {
        setStatus('closed');
        if (!stopped) retry = window.setTimeout(connect, RECONNECT_MS);
      };
    };
    connect();

    return () => {
      stopped = true;
      window.clearTimeout(retry);
      ws?.close();
    };
  }, []);

  return { events, status };
}
