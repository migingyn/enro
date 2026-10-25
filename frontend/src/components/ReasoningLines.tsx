import type { PlanResponse } from '../api/types';
import { Empty, Panel } from './Panel';

/** Planner reasoning: day-level lines, then per-event leave-by math. */
export function ReasoningLines({ plan }: { plan: PlanResponse | null }) {
  if (!plan) {
    return (
      <Panel title="Reasoning">
        <Empty>No plan yet.</Empty>
      </Panel>
    );
  }
  const titles = new Map(plan.events.map((e) => [e.id, e.title]));

  return (
    <Panel title="Reasoning">
      <ul className="mb-3 list-disc space-y-1 pl-5 text-sm">
        {plan.reasoning.map((line, i) => (
          <li key={i}>{line}</li>
        ))}
      </ul>
      <div className="space-y-2">
        {plan.plans.map((lp) => (
          <details key={lp.event_id} className="text-sm">
            <summary className="cursor-pointer font-medium">{titles.get(lp.event_id) ?? lp.event_id}</summary>
            <ul className="mt-1 space-y-0.5 pl-4 font-mono text-xs text-neutral-600">
              {lp.reasoning.map((line, i) => (
                <li key={i}>{line}</li>
              ))}
            </ul>
          </details>
        ))}
      </div>
    </Panel>
  );
}
