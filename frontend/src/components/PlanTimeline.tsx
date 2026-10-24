import type { ConsequencesResponse, PlanResponse } from '../api/types';
import { fmtTime } from '../lib/time';
import { Badge, Empty, Panel } from './Panel';

// TODO(T18): placeholder list, turn into a real timeline

export function PlanTimeline({
  plan,
  consequences,
}: {
  plan: PlanResponse | null;
  consequences?: ConsequencesResponse | null;
}) {
  if (!plan) {
    return (
      <Panel title="Plan">
        <Empty>Loading plan...</Empty>
      </Panel>
    );
  }

  const leaveFor = new Map(plan.plans.map((p) => [p.event_id, p]));
  const risk = new Map((consequences?.items ?? []).map((c) => [c.event_id, c]));

  return (
    <Panel title={`Plan for ${plan.date}`}>
      <ol className="space-y-3">
        {plan.events.map((ev) => {
          const lp = leaveFor.get(ev.id);
          const c = risk.get(ev.id);
          return (
            <li key={ev.id} className={`border-l-2 pl-3 ${ev.canceled ? 'border-neutral-200 opacity-50' : 'border-neutral-400'}`}>
              <div className="flex flex-wrap items-center gap-2">
                <span className="font-mono text-sm">{fmtTime(ev.start_ts)}</span>
                <span className={`font-medium ${ev.canceled ? 'line-through' : ''}`}>{ev.title}</span>
                {lp && <Badge label={lp.urgency} />}
                {c && c.status !== 'OK' && <Badge label={c.status} />}
                {ev.source === 'email' && <span className="text-xs text-neutral-500">from email</span>}
              </div>
              <div className="text-xs text-neutral-500">{ev.location}</div>
              {lp && (
                <div className="mt-1 text-sm text-neutral-700">
                  reminder {fmtTime(lp.reminder_at)} · leave by {fmtTime(lp.leave_by)} · {lp.mode}{' '}
                  {lp.travel_minutes} min · score {lp.urgency_score}
                </div>
              )}
            </li>
          );
        })}
      </ol>
      {consequences && (
        <p className="mt-4 rounded bg-neutral-100 p-2 text-sm">
          <span className="font-medium">+{consequences.delay_minutes} min: </span>
          {consequences.summary}
        </p>
      )}
    </Panel>
  );
}
