import type { Turn } from '../api/types';
import { fmtTimeSec } from '../lib/time';
import { Empty, Panel } from './Panel';

/** Call transcript, oldest first, grouped by call. */
export function Transcript({ turns }: { turns: Turn[] }) {
  const groups: { callId: string; turns: Turn[] }[] = [];
  for (const t of turns) {
    const last = groups[groups.length - 1];
    if (last && last.callId === t.call_id) last.turns.push(t);
    else groups.push({ callId: t.call_id, turns: [t] });
  }

  return (
    <Panel title="Transcript">
      {groups.length === 0 ? (
        <Empty>No call turns yet.</Empty>
      ) : (
        <div className="max-h-80 space-y-3 overflow-y-auto text-sm">
          {groups.map((g, gi) => (
            <div key={gi}>
              <div className="mb-1 text-xs font-medium text-neutral-400">call {g.callId}</div>
              <ul className="space-y-2">
                {g.turns.map((t, i) => (
                  <li key={i} className={t.speaker === 'agent' ? '' : 'text-right'}>
                    <div className="text-xs text-neutral-500">
                      {t.speaker} · {fmtTimeSec(t.ts)}
                      {t.intent && <> · {t.intent}</>}
                    </div>
                    <div
                      className={`inline-block max-w-[85%] rounded-2xl px-3 py-1.5 text-left ${
                        t.speaker === 'agent' ? 'bg-neutral-100' : 'bg-neutral-800 text-white'
                      }`}
                    >
                      {t.text}
                    </div>
                  </li>
                ))}
              </ul>
            </div>
          ))}
        </div>
      )}
    </Panel>
  );
}
