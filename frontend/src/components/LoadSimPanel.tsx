import type { SimResult } from '../api/types';
import { Empty, Panel } from './Panel';

// TODO(T19): placeholder bars, chart calls per minute and cost breakdown properly

export function LoadSimPanel({
  result,
  onRun,
  busy,
}: {
  result: SimResult | null;
  onRun?: () => void;
  busy?: boolean;
}) {
  const peak = result ? Math.max(1, ...result.calls_per_minute.map((b) => b.calls)) : 1;

  return (
    <Panel
      title="Load simulation"
      actions={
        onRun && (
          <button onClick={onRun} disabled={busy} className="rounded border border-neutral-300 px-2 py-1 text-xs hover:bg-neutral-100 disabled:opacity-50">
            {busy ? 'Running...' : 'Run 100k users'}
          </button>
        )
      }
    >
      {!result ? (
        <Empty>No run yet.</Empty>
      ) : (
        <div className="space-y-3 text-sm">
          <div className="grid grid-cols-2 gap-2 sm:grid-cols-4">
            <Stat label="users" value={result.users.toLocaleString()} />
            <Stat label="runtime" value={`${(result.runtime_ms / 1000).toFixed(2)} s`} />
            <Stat label="tasks" value={result.tasks_scheduled.toLocaleString()} />
            <Stat label="$/user/month" value={`$${result.cost_per_user_month_usd.toFixed(2)}`} />
          </div>
          <div>
            <div className="mb-1 text-xs text-neutral-500">calls per minute</div>
            <div className="flex h-24 items-end gap-1">
              {result.calls_per_minute.map((b) => (
                <div key={b.minute} className="flex h-full flex-1 items-end">
                  <div className="w-full bg-neutral-700" style={{ height: `${(b.calls / peak) * 100}%` }} title={`${b.minute}: ${b.calls}`} />
                </div>
              ))}
            </div>
            <div className="mt-1 flex justify-between text-xs text-neutral-500">
              <span>{result.calls_per_minute[0]?.minute}</span>
              <span>{result.calls_per_minute[result.calls_per_minute.length - 1]?.minute}</span>
            </div>
          </div>
          <div className="text-xs text-neutral-600">
            {result.threads} threads · {result.llm_calls.toLocaleString()} LLM calls · cost{' '}
            {Object.entries(result.cost_breakdown).map(([k, v]) => `${k} $${v.toFixed(2)}`).join(', ')}
          </div>
        </div>
      )}
    </Panel>
  );
}

function Stat({ label, value }: { label: string; value: string }) {
  return (
    <div className="rounded bg-neutral-100 p-2">
      <div className="text-xs text-neutral-500">{label}</div>
      <div className="font-medium">{value}</div>
    </div>
  );
}
