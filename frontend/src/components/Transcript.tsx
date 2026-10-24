import type { Turn } from '../api/types';
import { fmtTimeSec } from '../lib/time';
import { Empty, Panel } from './Panel';

// TODO(T18): placeholder transcript, group by call

export function Transcript({ turns }: { turns: Turn[] }) {
  return (
    <Panel title="Transcript">
      {turns.length === 0 ? (
        <Empty>No call turns yet.</Empty>
      ) : (
        <ul className="max-h-72 space-y-2 overflow-y-auto text-sm">
          {turns.map((t, i) => (
            <li key={i} className={t.speaker === 'agent' ? '' : 'text-right'}>
              <div className="text-xs text-neutral-500">
                {t.speaker} · {t.call_id} · {fmtTimeSec(t.ts)}
                {t.intent && <> · {t.intent}</>}
              </div>
              <div
                className={`inline-block rounded px-2 py-1 ${t.speaker === 'agent' ? 'bg-neutral-100' : 'bg-neutral-800 text-white'}`}
              >
                {t.text}
              </div>
            </li>
          ))}
        </ul>
      )}
    </Panel>
  );
}
