import type { Decision } from '../api/types';
import { fmtTimeSec } from '../lib/time';
import { Badge, Empty, Panel } from './Panel';

/** Live feed of agent decisions, newest first. Every agent action lands here. */
export function ReflexFeed({ decisions }: { decisions: Decision[] }) {
  return (
    <Panel title="Reflex decisions" actions={<span className="text-xs text-neutral-400">{decisions.length}</span>}>
      {decisions.length === 0 ? (
        <Empty>No decisions yet.</Empty>
      ) : (
        <ul className="max-h-80 space-y-2 overflow-y-auto text-sm">
          {decisions.map((d, i) => (
            <li key={i} className={`rounded-lg border p-2 ${i === 0 ? 'border-neutral-400' : 'border-neutral-100'}`}>
              <div className="flex flex-wrap items-center gap-2">
                <span className="font-mono text-xs text-neutral-500">{fmtTimeSec(d.ts)}</span>
                <span className="font-medium">{d.kind}</span>
                <span className="rounded bg-neutral-900 px-1.5 py-0.5 text-xs text-white">{d.verdict}</span>
                <span className="ml-auto flex items-center gap-1 text-xs text-neutral-500">
                  <Badge label={d.source} />
                  {Math.round(d.confidence * 100)}% · {d.latency_ms} ms
                </span>
              </div>
              <div className="mt-1 flex flex-wrap gap-1">
                {Object.entries(d.inputs ?? {}).map(([k, v]) => (
                  <span key={k} className="rounded bg-neutral-50 px-1.5 py-0.5 font-mono text-xs text-neutral-600">
                    {k}={typeof v === 'object' ? JSON.stringify(v) : String(v)}
                  </span>
                ))}
              </div>
            </li>
          ))}
        </ul>
      )}
    </Panel>
  );
}
