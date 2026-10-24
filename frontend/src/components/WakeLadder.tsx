import type { Call, WakePlan } from '../api/types';
import { fmtTime } from '../lib/time';
import { Empty, Panel } from './Panel';

// TODO(T18): placeholder ladder, animate escalation

const STEP_ATTEMPT: Record<string, number> = { CALL_1: 1, CALL_2: 2, CALL_3: 3 };

export function WakeLadder({ wake, call }: { wake: WakePlan | null; call: Call | null }) {
  if (!wake) {
    return (
      <Panel title="Wake ladder">
        <Empty>No wake plan yet.</Empty>
      </Panel>
    );
  }
  const active = call && call.purpose === 'wake' ? call.attempt : 0;

  return (
    <Panel title="Wake ladder">
      <p className="mb-2 text-sm text-neutral-600">
        alarm {wake.alarm_time} ({fmtTime(wake.alarm_ts)}) + {wake.grace_minutes} min grace
      </p>
      <ol className="space-y-1 text-sm">
        {wake.steps.map((s) => {
          const isActive = STEP_ATTEMPT[s.step] === active;
          return (
            <li
              key={s.step}
              className={`flex items-center gap-2 rounded px-2 py-1 ${isActive ? 'bg-neutral-900 text-white' : 'bg-neutral-50'}`}
            >
              <span className="w-24 font-mono">{s.step}</span>
              <span className="font-mono">{fmtTime(s.at)}</span>
              <span>{s.tone}</span>
              {s.notify_contacts && <span className="text-xs">+ text contacts</span>}
              {isActive && call && <span className="ml-auto text-xs">{call.state}</span>}
            </li>
          );
        })}
      </ol>
      {call && (
        <p className="mt-2 text-xs text-neutral-500">
          last call {call.id} · {call.purpose} · attempt {call.attempt} · {call.state} · {call.turns} turns
        </p>
      )}
    </Panel>
  );
}
