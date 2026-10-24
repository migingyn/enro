import type { PunctualityResponse, PunctualityStats } from '../api/types';
import { Empty, Panel } from './Panel';

// TODO(T18): placeholder tables, chart the learned biases

function StatsTable({ title, rows }: { title: string; rows: Record<string, PunctualityStats> }) {
  return (
    <table className="w-full text-left text-sm">
      <thead className="text-xs text-neutral-500">
        <tr>
          <th className="py-1 font-medium">{title}</th>
          <th className="font-medium">prep bias</th>
          <th className="font-medium">travel x</th>
          <th className="font-medium">arrival Δ</th>
          <th className="font-medium">n</th>
        </tr>
      </thead>
      <tbody>
        {Object.entries(rows).map(([key, s]) => (
          <tr key={key} className="border-t border-neutral-100">
            <td className="py-1">{key}</td>
            <td>{s.prep_bias >= 0 ? '+' : ''}{s.prep_bias.toFixed(1)} min</td>
            <td>{s.travel_factor.toFixed(2)}</td>
            <td>{s.arrival_delta >= 0 ? '+' : ''}{s.arrival_delta.toFixed(1)} min</td>
            <td>{s.samples}</td>
          </tr>
        ))}
      </tbody>
    </table>
  );
}

export function PunctualityCard({ stats }: { stats: PunctualityResponse | null }) {
  if (!stats) {
    return (
      <Panel title="Punctuality">
        <Empty>Loading...</Empty>
      </Panel>
    );
  }
  return (
    <Panel title="Punctuality">
      <ul className="mb-4 space-y-1">
        {stats.headlines.map((h, i) => (
          <li key={i} className="font-medium">{h}</li>
        ))}
      </ul>
      <div className="space-y-4">
        <StatsTable title="event type" rows={stats.by_event_type} />
        <StatsTable title="mode" rows={stats.by_mode} />
        <div className="text-sm">
          <div className="mb-1 text-xs text-neutral-500">answer rate by channel</div>
          <ul className="flex flex-wrap gap-2">
            {stats.channel_response.map((c, i) => (
              <li key={i} className="rounded bg-neutral-100 px-2 py-0.5">
                {c.channel} / {c.time_of_day}: {Math.round(c.answer_rate * 100)}%
              </li>
            ))}
          </ul>
        </div>
        <div className="text-sm">
          <div className="mb-1 text-xs text-neutral-500">transport learned</div>
          <ul className="space-y-0.5">
            {stats.transport.map((t, i) => (
              <li key={i}>
                {t.destination_type} {t.distance_bucket}: <span className="font-medium">{t.predicted}</span>{' '}
                <span className="text-neutral-500">
                  ({Object.entries(t.counts).map(([m, n]) => `${m} ${n}`).join(', ')})
                </span>
              </li>
            ))}
          </ul>
        </div>
      </div>
    </Panel>
  );
}
