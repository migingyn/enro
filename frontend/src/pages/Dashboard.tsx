import { useEffect, useMemo, useState } from 'react';
import { Link } from 'react-router-dom';
import type {
  Call,
  ConsequencesResponse,
  DashboardEvent,
  DashboardEventType,
  Decision,
  DemoScenario,
  DemoTriggerResponse,
  PlanResponse,
  PunctualityResponse,
  SimResult,
  TripTick,
  Turn,
} from '../api/types';
import { demoTrigger, demoTrip, getConsequences, getPlan, getPunctuality, simLoad } from '../api/client';
import { useDashboardSocket } from '../hooks/useDashboardSocket';
import { LoadSimPanel } from '../components/LoadSimPanel';
import { PlanTimeline } from '../components/PlanTimeline';
import { PunctualityCard } from '../components/PunctualityCard';
import { ReasoningLines } from '../components/ReasoningLines';
import { ReflexFeed } from '../components/ReflexFeed';
import { Transcript } from '../components/Transcript';
import { TripReplay } from '../components/TripReplay';
import { WakeLadder } from '../components/WakeLadder';
import { USE_MOCKS } from '../lib/env';
import { fmtTimeSec } from '../lib/time';

type Data<K extends DashboardEvent['type']> = Extract<DashboardEvent, { type: K }>['data'];

function ofType<K extends DashboardEvent['type']>(events: DashboardEvent[], type: K): Data<K>[] {
  return events.filter((e): e is Extract<DashboardEvent, { type: K }> => e.type === type).map((e) => e.data as Data<K>);
}

const SCENARIOS: { id: DemoScenario; label: string }[] = [
  { id: 'wake_ladder', label: 'Wake ladder' },
  { id: 'leave_reminder', label: 'Leave reminder' },
  { id: 'email_drop', label: 'Email drop' },
];

const EVENT_TYPES: DashboardEventType[] = ['plan', 'decision', 'call_state', 'turn', 'trip_tick', 'sim_result'];

export default function Dashboard() {
  const { events, status } = useDashboardSocket();
  const [fetchedPlan, setFetchedPlan] = useState<PlanResponse | null>(null);
  const [consequences, setConsequences] = useState<ConsequencesResponse | null>(null);
  const [punctuality, setPunctuality] = useState<PunctualityResponse | null>(null);
  const [simResult, setSimResult] = useState<SimResult | null>(null);
  const [trigger, setTrigger] = useState<DemoTriggerResponse | null>(null);
  const [busy, setBusy] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    Promise.all([getPlan(), getConsequences(), getPunctuality()])
      .then(([p, c, s]) => {
        setFetchedPlan(p);
        setConsequences(c);
        setPunctuality(s);
      })
      .catch((err: unknown) => setError(String(err)));
  }, []);

  // Events are newest first; a pushed plan or sim result replaces the fetched one.
  const plans = useMemo(() => ofType(events, 'plan'), [events]);
  const decisions: Decision[] = useMemo(() => ofType(events, 'decision'), [events]);
  // Latest state per call, most recently updated first.
  const calls: Call[] = useMemo(() => {
    const seen = new Map<string, Call>();
    for (const c of ofType(events, 'call_state')) if (!seen.has(c.id)) seen.set(c.id, c);
    return [...seen.values()];
  }, [events]);
  const counts = useMemo(() => {
    const m = new Map<DashboardEventType, number>(EVENT_TYPES.map((t) => [t, 0]));
    for (const e of events) m.set(e.type, (m.get(e.type) ?? 0) + 1);
    return m;
  }, [events]);
  const turns: Turn[] = useMemo(() => ofType(events, 'turn').reverse(), [events]);
  const ticks: TripTick[] = useMemo(() => ofType(events, 'trip_tick'), [events]);
  const sims = useMemo(() => ofType(events, 'sim_result'), [events]);

  const plan = plans[0] ?? fetchedPlan;
  const sim = sims[0] ?? simResult;

  const run = async (key: string, fn: () => Promise<void>) => {
    setBusy(key);
    setError(null);
    try {
      await fn();
    } catch (err) {
      setError(String(err));
    } finally {
      setBusy(null);
    }
  };

  return (
    <main className="mx-auto min-h-screen max-w-7xl space-y-4 bg-neutral-50 p-4">
      <header className="flex flex-wrap items-center gap-3">
        <h1 className="text-xl font-bold">Enro · judge dashboard</h1>
        <span className="rounded bg-neutral-200 px-2 py-0.5 text-xs">
          {USE_MOCKS ? 'mock data' : 'live'} · ws {status} · {events.length} events
        </span>
        <div className="ml-auto flex flex-wrap gap-2">
          {SCENARIOS.map((s) => (
            <button
              key={s.id}
              disabled={busy !== null}
              onClick={() => run(s.id, async () => setTrigger(await demoTrigger(s.id)))}
              className="rounded bg-neutral-900 px-3 py-1.5 text-sm text-white hover:bg-neutral-700 disabled:opacity-50"
            >
              {s.label}
            </button>
          ))}
        </div>
      </header>

      <ul className="flex flex-wrap gap-2 text-xs">
        {EVENT_TYPES.map((t) => (
          <li key={t} className="rounded-full border border-neutral-200 bg-white px-2 py-0.5 font-mono">
            {t} <span className="font-semibold">{counts.get(t)}</span>
          </li>
        ))}
      </ul>

      {trigger && (
        <p className="rounded bg-neutral-100 p-2 text-sm">
          {trigger.scenario}: {trigger.message}
          {trigger.call_id && (
            <>
              {' '}
              <Link className="underline" to={`/call/${trigger.call_id}`} target="_blank">
                open call simulator ({trigger.call_id})
              </Link>
            </>
          )}
        </p>
      )}
      {error && <p className="rounded bg-red-50 p-2 text-sm text-red-700">{error}</p>}

      <div className="grid gap-4 lg:grid-cols-3">
        <div className="space-y-4 lg:col-span-2">
          <PlanTimeline plan={plan} consequences={consequences} />
          <ReasoningLines plan={plan} />
          <div className="grid gap-4 md:grid-cols-2">
            <TripReplay
              ticks={ticks}
              busy={busy === 'trip'}
              onStart={() => run('trip', async () => void (await demoTrip()))}
            />
            <LoadSimPanel
              result={sim}
              busy={busy === 'sim'}
              onRun={() => run('sim', async () => setSimResult(await simLoad()))}
            />
          </div>
        </div>
        <div className="space-y-4">
          <WakeLadder wake={plan?.wake ?? null} calls={calls} />
          <Transcript turns={turns} />
          <ReflexFeed decisions={decisions} />
          <PunctualityCard stats={punctuality} />
        </div>
      </div>

      <details className="text-xs text-neutral-500">
        <summary className="cursor-pointer">raw event log</summary>
        <ul className="mt-2 space-y-0.5 font-mono">
          {events.slice(0, 50).map((e, i) => (
            <li key={i}>
              {fmtTimeSec(e.ts)} <span className="font-semibold">{e.type}</span> {JSON.stringify(e.data).slice(0, 120)}
            </li>
          ))}
        </ul>
      </details>
    </main>
  );
}
