import { useEffect, useRef, useState } from 'react';
import type { LocationResponse } from '../api/types';
import { postLocation } from '../api/client';
import { DEMO_USER_ID } from '../lib/env';
import { fmtTimeSec, nowTs } from '../lib/time';

// TODO(T20): placeholder beacon page for departure verification

const SEND_EVERY_MS = 15_000;

export default function Beacon() {
  const [active, setActive] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [last, setLast] = useState<{ ts: number; res: LocationResponse; accuracy: number } | null>(null);
  const position = useRef<GeolocationPosition | null>(null);

  useEffect(() => {
    if (!active) return;
    if (!('geolocation' in navigator)) {
      setError('Geolocation is not available in this browser.');
      return;
    }
    const watch = navigator.geolocation.watchPosition(
      (p) => {
        position.current = p;
        setError(null);
      },
      (err) => setError(err.message),
      { enableHighAccuracy: true },
    );

    const send = async () => {
      const p = position.current;
      if (!p) return;
      try {
        const res = await postLocation({
          user_id: DEMO_USER_ID,
          lat: p.coords.latitude,
          lng: p.coords.longitude,
          accuracy_m: p.coords.accuracy,
          ts: nowTs(),
        });
        setLast({ ts: nowTs(), res, accuracy: p.coords.accuracy });
      } catch (err) {
        setError(String(err));
      }
    };
    // First fix may take a moment; try shortly after starting, then every 15 s.
    const first = window.setTimeout(send, 2000);
    const id = window.setInterval(send, SEND_EVERY_MS);
    return () => {
      navigator.geolocation.clearWatch(watch);
      window.clearTimeout(first);
      window.clearInterval(id);
    };
  }, [active]);

  return (
    <main className="mx-auto flex min-h-screen max-w-sm flex-col items-center justify-center gap-4 p-6 text-center">
      <h1 className="text-2xl font-bold">Beacon</h1>
      <p className="text-sm text-neutral-600">Keep this page open so Enro can tell when you've left.</p>
      <button
        onClick={() => setActive((a) => !a)}
        className={`rounded-full px-6 py-2 text-white ${active ? 'bg-red-600' : 'bg-neutral-900'}`}
      >
        {active ? 'Stop sharing' : 'Share location'}
      </button>
      {active && <p className="text-xs text-neutral-500">Sending every 15 seconds.</p>}
      {error && <p className="text-sm text-red-700">{error}</p>}
      {last && (
        <div className="rounded bg-neutral-100 p-3 text-sm">
          <div>last sent {fmtTimeSec(last.ts)} (±{Math.round(last.accuracy)} m)</div>
          <div>{Math.round(last.res.distance_from_home_m)} m from home</div>
          <div className="font-medium">{last.res.departed ? 'Departed' : 'Still home'}</div>
        </div>
      )}
    </main>
  );
}
