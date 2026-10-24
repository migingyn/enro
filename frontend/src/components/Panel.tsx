import type { ReactNode } from 'react';

// TODO(T18): placeholder card chrome, restyle with the dashboard
export function Panel({ title, actions, children }: { title: string; actions?: ReactNode; children: ReactNode }) {
  return (
    <section className="rounded-lg border border-neutral-200 bg-white p-4 shadow-sm">
      <div className="mb-3 flex items-center justify-between gap-2">
        <h2 className="text-sm font-semibold uppercase tracking-wide text-neutral-500">{title}</h2>
        {actions}
      </div>
      {children}
    </section>
  );
}

export function Empty({ children }: { children: ReactNode }) {
  return <p className="text-sm text-neutral-400">{children}</p>;
}

const URGENCY_CLASS: Record<string, string> = {
  CHILL: 'bg-neutral-100 text-neutral-700',
  FIRM: 'bg-amber-100 text-amber-800',
  URGENT: 'bg-red-100 text-red-800',
  OK: 'bg-emerald-100 text-emerald-800',
  AT_RISK: 'bg-amber-100 text-amber-800',
  MISSED: 'bg-red-100 text-red-800',
};

/** Small pill for an urgency or consequence status. */
export function Badge({ label }: { label: string }) {
  return (
    <span className={`rounded px-1.5 py-0.5 text-xs font-medium ${URGENCY_CLASS[label] ?? 'bg-neutral-100 text-neutral-700'}`}>
      {label}
    </span>
  );
}
