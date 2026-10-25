import type { SimResult } from '../api/types';
import { Empty, Panel } from './Panel';

const COST_COLORS = ['#404040', '#10b981', '#f59e0b', '#6366f1', '#ef4444', '#06b6d4'];

const CW = 300;
const CH = 120;
const AXIS = 14;

export function LoadSimPanel({
  result,
  onRun,
  busy,
}: {
  result: SimResult | null;
  onRun?: () => void;
  busy?: boolean;
}) {
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
          <CallsChart buckets={result.calls_per_minute} />
          <CostBreakdown breakdown={result.cost_breakdown} total={result.cost_per_user_month_usd} />
          <div className="text-xs text-neutral-500">
            {result.run_id} · {result.threads} threads · {result.llm_calls.toLocaleString()} LLM calls
          </div>
        </div>
      )}
    </Panel>
  );
}

function CallsChart({ buckets }: { buckets: SimResult['calls_per_minute'] }) {
  if (buckets.length === 0) return <Empty>No calls recorded.</Empty>;
  const peak = Math.max(1, ...buckets.map((b) => b.calls));
  const peakBucket = buckets.find((b) => b.calls === peak);
  const slot = CW / buckets.length;
  const barW = Math.max(1, slot * 0.75);
  const plotH = CH - AXIS;
  // Label at most ~6 buckets so labels don't overlap.
  const every = Math.max(1, Math.ceil(buckets.length / 6));

  return (
    <div>
      <div className="mb-1 flex justify-between text-xs text-neutral-500">
        <span>calls per minute</span>
        {peakBucket && (
          <span>
            peak {peak.toLocaleString()} at {peakBucket.minute}
          </span>
        )}
      </div>
      <svg viewBox={`0 0 ${CW} ${CH}`} className="w-full" role="img" aria-label="Calls per minute chart">
        <line x1={0} y1={plotH} x2={CW} y2={plotH} stroke="#d4d4d4" />
        {buckets.map((b, i) => {
          const h = (b.calls / peak) * (plotH - 4);
          const x = i * slot + (slot - barW) / 2;
          return (
            <g key={b.minute}>
              <rect x={x} y={plotH - h} width={barW} height={h} rx={1.5} fill={b.calls === peak ? '#059669' : '#525252'}>
                <title>{`${b.minute}: ${b.calls.toLocaleString()} calls`}</title>
              </rect>
              {i % every === 0 && (
                <text x={i * slot + slot / 2} y={CH - 2} fontSize={9} textAnchor="middle" fill="#737373">
                  {b.minute}
                </text>
              )}
            </g>
          );
        })}
      </svg>
    </div>
  );
}

function CostBreakdown({ breakdown, total }: { breakdown: Record<string, number>; total: number }) {
  const entries = Object.entries(breakdown).sort((a, b) => b[1] - a[1]);
  const sum = entries.reduce((s, [, v]) => s + v, 0) || 1;
  if (entries.length === 0) return null;

  return (
    <div>
      <div className="mb-1 flex justify-between text-xs text-neutral-500">
        <span>cost per user / month</span>
        <span>${total.toFixed(2)} total</span>
      </div>
      <div className="flex h-3 w-full overflow-hidden rounded bg-neutral-100">
        {entries.map(([k, v], i) => (
          <div key={k} style={{ width: `${(v / sum) * 100}%`, background: COST_COLORS[i % COST_COLORS.length] }} title={`${k}: $${v.toFixed(2)}`} />
        ))}
      </div>
      <ul className="mt-2 grid grid-cols-2 gap-x-3 gap-y-1 text-xs">
        {entries.map(([k, v], i) => (
          <li key={k} className="flex items-center gap-1.5">
            <span className="inline-block h-2 w-2 rounded-sm" style={{ background: COST_COLORS[i % COST_COLORS.length] }} />
            <span className="text-neutral-600">{k}</span>
            <span className="ml-auto font-mono">
              ${v.toFixed(2)} <span className="text-neutral-400">({Math.round((v / sum) * 100)}%)</span>
            </span>
          </li>
        ))}
      </ul>
    </div>
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
