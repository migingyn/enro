import { Link } from 'react-router-dom';

export default function Done() {
  return (
    <main className="mx-auto flex min-h-screen max-w-md flex-col items-center justify-center gap-4 p-6 text-center">
      <div className="flex h-20 w-20 items-center justify-center rounded-full bg-emerald-500 text-4xl text-white" aria-hidden>
        ✓
      </div>
      <h1 className="text-4xl font-bold">You're set</h1>
      <p className="text-neutral-600">
        I've got it from here. Expect a call before your first event. Go to sleep.
      </p>
      <div className="flex flex-wrap justify-center gap-3 pt-2">
        <Link to="/dashboard" className="rounded-full bg-neutral-900 px-6 py-3 font-medium text-white hover:bg-neutral-700">
          Open dashboard
        </Link>
        <Link to="/beacon" className="rounded-full border border-neutral-300 px-6 py-3 font-medium hover:bg-neutral-100">
          Beacon
        </Link>
      </div>
    </main>
  );
}
