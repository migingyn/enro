import type { TripTick } from '../api/types';
import { fmtTime } from '../lib/time';
import { Badge, Empty, Panel } from './Panel';

const W = 300;
const H = 160;
const PAD = 12;

/** Project lat/lng of every tick into the SVG box, keeping the aspect ratio. */
function project(points: TripTick[]) {
  const lats = points.map((p) => p.lat);
  const lngs = points.map((p) => p.lng);
  const minLat = Math.min(...lats);
  const maxLat = Math.max(...lats);
  const minLng = Math.min(...lngs);
  const maxLng = Math.max(...lngs);
  const span = Math.max(maxLat - minLat, maxLng - minLng, 1e-4);
  const scale = Math.min(W - 2 * PAD, H - 2 * PAD) / span;
  const offX = (W - (maxLng - minLng) * scale) / 2;
  const offY = (H - (maxLat - minLat) * scale) / 2;
  return (p: TripTick) => ({
    x: offX + (p.lng - minLng) * scale,
    y: H - (offY + (p.lat - minLat) * scale), // north is up
  });
}

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
  // Ticks arrive newest first; replay the current trip in order.
  const trip = latest ? ticks.filter((t) => t.trip_id === latest.trip_id).sort((a, b) => a.seq - b.seq) : [];
  const toXY = trip.length ? project(trip) : null;
  const commentary = trip.filter((t) => t.line).reverse();

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
      {!latest || !toXY ? (
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

          <svg viewBox={`0 0 ${W} ${H}`} className="w-full rounded bg-neutral-50" role="img" aria-label="Bus position map">
            {trip.slice(1).map((t, i) => {
              const a = toXY(trip[i]);
              const b = toXY(t);
              return (
                <line
                  key={t.seq}
                  x1={a.x}
                  y1={a.y}
                  x2={b.x}
                  y2={b.y}
                  stroke={t.on_route ? '#525252' : '#d97706'}
                  strokeWidth={3}
                  strokeLinecap="round"
                  strokeDasharray={t.on_route ? undefined : '4 3'}
                />
              );
            })}
            {trip.map((t) => {
              const p = toXY(t);
              return <circle key={t.seq} cx={p.x} cy={p.y} r={2} fill={t.on_route ? '#a3a3a3' : '#f59e0b'} />;
            })}
            {(() => {
              const p = toXY(latest);
              return (
                <g>
                  <circle cx={p.x} cy={p.y} r={9} fill={latest.on_route ? '#10b981' : '#ef4444'} opacity={0.25} />
                  <circle cx={p.x} cy={p.y} r={5} fill={latest.on_route ? '#059669' : '#dc2626'} stroke="white" strokeWidth={1.5} />
                </g>
              );
            })()}
          </svg>

          <div className="text-neutral-600">
            next stop {latest.next_stop} · ETA {fmtTime(latest.eta_ts)} (scheduled {fmtTime(latest.scheduled_arrival_ts)}) ·{' '}
            {latest.delay_minutes} min late
          </div>
          {latest.reroute && (
            <div className="rounded bg-amber-50 p-2">
              <div className="font-medium">{latest.reroute.suggestion}</div>
              <div className="text-xs text-neutral-600">
                {latest.reroute.reason} · new ETA {fmtTime(latest.reroute.new_eta_ts)}
              </div>
            </div>
          )}

          <div>
            <div className="mb-1 text-xs text-neutral-500">commentary</div>
            {commentary.length === 0 ? (
              <Empty>No commentary yet.</Empty>
            ) : (
              <ul className="max-h-40 space-y-1 overflow-y-auto">
                {commentary.map((t) => (
                  <li key={t.seq} className="border-l-2 border-neutral-300 pl-2">
                    <span className="mr-1 font-mono text-xs text-neutral-400">{fmtTime(t.ts)}</span>
                    <span className="italic">{t.line}</span>
                  </li>
                ))}
              </ul>
            )}
          </div>
          <div className="text-xs text-neutral-400">
            {trip.length} ticks · {latest.lat.toFixed(4)}, {latest.lng.toFixed(4)}
          </div>
        </div>
      )}
    </Panel>
  );
}
