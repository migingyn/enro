import type { ConsequencesResponse, PlanResponse } from '../api/types';
import { fmtTime } from '../lib/time';
import { Badge, Empty, Panel } from './Panel';

/** Day timeline: each event with its leave-by plan and consequence status. */
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
      <ol className="relative ml-2 space-y-4 border-l-2 border-neutral-200">
        {plan.events.map((ev) => {
          const lp = leaveFor.get(ev.id);
          const c = risk.get(ev.id);
          return (
            <li key={ev.id} className={`relative pl-5 ${ev.canceled ? 'opacity-50' : ''}`}>
              <span
                className={`absolute -left-[7px] top-1.5 h-3 w-3 rounded-full border-2 border-white ${
                  ev.canceled ? 'bg-neutral-300' : c && c.status !== 'OK' ? 'bg-red-500' : 'bg-neutral-800'
                }`}
              />
              <div className="flex flex-wrap items-center gap-2">
                <span className="font-mono text-sm">{fmtTime(ev.start_ts)}</span>
                <span className={`font-medium ${ev.canceled ? 'line-through' : ''}`}>{ev.title}</span>
                {lp && <Badge label={lp.urgency} />}
                {c && c.status !== 'OK' && <Badge label={c.status} />}
                {ev.source === 'email' && <span className="text-xs text-neutral-500">from email</span>}
              </div>
              <div className="text-xs text-neutral-500">{ev.location}</div>
              {lp && (
                <div className="mt-1 rounded bg-neutral-50 px-2 py-1 text-sm text-neutral-700">
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
