import type { Decision } from '../api/types';
import { fmtTimeSec } from '../lib/time';
import { Empty, Panel } from './Panel';

// TODO(T18): placeholder feed of agent decisions

export function ReflexFeed({ decisions }: { decisions: Decision[] }) {
  return (
    <Panel title="Reflex decisions">
      {decisions.length === 0 ? (
        <Empty>No decisions yet.</Empty>
      ) : (
        <ul className="max-h-72 space-y-2 overflow-y-auto text-sm">
          {decisions.map((d, i) => (
            <li key={i} className="rounded border border-neutral-100 p-2">
              <div className="flex flex-wrap items-center gap-2">
                <span className="font-mono text-xs text-neutral-500">{fmtTimeSec(d.ts)}</span>
                <span className="font-medium">{d.kind}</span>
                <span>→ {d.verdict}</span>
                <span className="ml-auto text-xs text-neutral-500">
                  {d.source} · {Math.round(d.confidence * 100)}% · {d.latency_ms} ms
                </span>
              </div>
              <pre className="mt-1 overflow-x-auto text-xs text-neutral-500">{JSON.stringify(d.inputs)}</pre>
            </li>
          ))}
        </ul>
      )}
    </Panel>
  );
}
