import type { TripTick } from '../api/types';
import { fmtTime } from '../lib/time';
import { Badge, Empty, Panel } from './Panel';

// TODO(T19): placeholder trip view, add a map and the commentary feed

export function TripReplay({
  ticks,
  onStart,
  busy,
}: {
  ticks: TripTick[];
  onStart?: () => void;
  busy?: boolean;
}) {
  const latest = ticks[0] ?? null;

  return (
    <Panel
      title="Trip replay"
      actions={
        onStart && (
          <button onClick={onStart} disabled={busy} className="rounded border border-neutral-300 px-2 py-1 text-xs hover:bg-neutral-100 disabled:opacity-50">
            Start trip
          </button>
        )
      }
    >
      {!latest ? (
        <Empty>No trip running.</Empty>
      ) : (
        <div className="space-y-2 text-sm">
          <div className="flex flex-wrap items-center gap-2">
            <span className="font-medium">
              {latest.trip_id} · route {latest.route} · tick {latest.seq}
            </span>
            <Badge label={latest.verdict} />
            {!latest.on_route && <Badge label="OFF ROUTE" />}
          </div>
          <div className="text-neutral-600">
            next stop {latest.next_stop} · ETA {fmtTime(latest.eta_ts)} (scheduled {fmtTime(latest.scheduled_arrival_ts)}) ·{' '}
            {latest.delay_minutes} min late
          </div>
          <div className="font-mono text-xs text-neutral-500">
            {latest.lat.toFixed(4)}, {latest.lng.toFixed(4)}
          </div>
          {latest.line && <blockquote className="border-l-2 border-neutral-300 pl-2 italic">{latest.line}</blockquote>}
          {latest.reroute && (
            <div className="rounded bg-amber-50 p-2">
              <div className="font-medium">{latest.reroute.suggestion}</div>
              <div className="text-xs text-neutral-600">
                {latest.reroute.reason} · new ETA {fmtTime(latest.reroute.new_eta_ts)}
              </div>
            </div>
          )}
          <div className="text-xs text-neutral-400">{ticks.length} ticks received</div>
        </div>
      )}
    </Panel>
  );
}
