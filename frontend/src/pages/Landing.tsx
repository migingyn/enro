import { Link } from 'react-router-dom';

const STEPS = [
  { title: 'Wakes you up', body: 'A real call that escalates until you are actually out of bed.' },
  { title: 'Gets you out the door', body: 'It plans backwards from your first event and nudges you at each step.' },
  { title: 'Learns how late you run', body: 'Every "five more minutes" feeds a model of your real lateness.' },
];

export default function Landing() {
  return (
    <main className="mx-auto flex min-h-screen max-w-3xl flex-col justify-center gap-10 px-6 py-12">
      <section className="space-y-4 text-center">
        <div className="text-sm font-semibold uppercase tracking-widest text-emerald-600">Enro</div>
        <h1 className="text-5xl font-bold tracking-tight sm:text-6xl">Ontime everytime.</h1>
        <p className="mx-auto max-w-xl text-lg text-neutral-600">
          A background AI agent that knows you'll be late before you do, and calls you about it.
        </p>
        <div className="flex flex-wrap justify-center gap-3 pt-2">
          <Link to="/onboarding" className="rounded-full bg-neutral-900 px-8 py-3 font-medium text-white hover:bg-neutral-700">
            Get started
          </Link>
          <Link to="/dashboard" className="rounded-full border border-neutral-300 px-6 py-3 font-medium hover:bg-neutral-100">
            Dashboard
          </Link>
          <Link to="/call/demo" className="rounded-full border border-neutral-300 px-6 py-3 font-medium hover:bg-neutral-100">
            Try a call
          </Link>
        </div>
      </section>

      <section className="grid gap-4 sm:grid-cols-3">
        {STEPS.map((s, i) => (
          <div key={s.title} className="rounded-2xl border border-neutral-200 p-5">
            <div className="text-xs font-semibold text-neutral-400">0{i + 1}</div>
            <div className="mt-1 font-semibold">{s.title}</div>
            <p className="mt-1 text-sm text-neutral-600">{s.body}</p>
          </div>
        ))}
      </section>

      <p className="text-center text-xs text-neutral-500">
        Leaving the house? Open the <Link to="/beacon" className="underline">beacon</Link> on your phone.
      </p>
    </main>
  );
}
