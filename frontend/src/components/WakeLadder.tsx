import type { Call, WakePlan } from '../api/types';
import { fmtTime } from '../lib/time';
import { Badge, Empty, Panel } from './Panel';

/** Wake escalation ladder; highlights the step the latest wake call is on. */

const STEP_ATTEMPT: Record<string, number> = { CALL_1: 1, CALL_2: 2, CALL_3: 3 };

export function WakeLadder({ wake, calls }: { wake: WakePlan | null; calls: Call[] }) {
  const call = calls[0] ?? null;
  if (!wake) {
    return (
      <Panel title="Wake ladder">
        <Empty>No wake plan yet.</Empty>
      </Panel>
    );
  }
  const wakeCall = calls.find((c) => c.purpose === 'wake') ?? null;
  const active = wakeCall ? wakeCall.attempt : 0;

  return (
    <Panel title="Wake ladder">
      <p className="mb-2 text-sm text-neutral-600">
        alarm {wake.alarm_time} ({fmtTime(wake.alarm_ts)}) + {wake.grace_minutes} min grace
      </p>
      <ol className="space-y-1 text-sm">
        {wake.steps.map((s) => {
          const n = STEP_ATTEMPT[s.step];
          const isActive = n !== undefined && n === active;
          const isDone = n !== undefined && n < active;
          return (
            <li
              key={s.step}
              className={`flex items-center gap-2 rounded px-2 py-1 transition-colors ${
                isActive ? 'bg-neutral-900 text-white' : isDone ? 'bg-neutral-100 text-neutral-400 line-through' : 'bg-neutral-50'
              }`}
            >
              {isActive && <span className="h-2 w-2 animate-pulse rounded-full bg-red-400" />}
              <span className="w-20 font-mono">{s.step}</span>
              <span className="font-mono">{fmtTime(s.at)}</span>
              <span>{s.tone}</span>
              {s.notify_contacts && <span className="text-xs">+ text contacts</span>}
              {isActive && wakeCall && <span className="ml-auto text-xs">{wakeCall.state}</span>}
            </li>
          );
        })}
      </ol>
      {calls.length > 0 && (
        <ul className="mt-3 space-y-1 text-xs text-neutral-600">
          {calls.map((c) => (
            <li key={c.id} className="flex flex-wrap items-center gap-2">
              <span className="font-mono">{c.id}</span>
              <span>{c.purpose}</span>
              <span>attempt {c.attempt}</span>
              <span>{c.turns} turns</span>
              <span className="ml-auto">
                <Badge label={c.state} />
              </span>
            </li>
          ))}
        </ul>
      )}
      {call && call.purpose !== 'wake' && <p className="mt-2 text-xs text-neutral-400">latest call: {call.purpose}</p>}
    </Panel>
  );
}
