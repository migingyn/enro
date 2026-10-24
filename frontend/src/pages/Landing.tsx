import { Link } from 'react-router-dom';

// TODO(T20): placeholder landing, restyle with the visual identity

export default function Landing() {
  return (
    <main className="mx-auto flex min-h-screen max-w-xl flex-col items-center justify-center gap-6 p-6 text-center">
      <h1 className="text-5xl font-bold tracking-tight">Enro</h1>
      <p className="text-2xl font-medium">Ontime everytime.</p>
      <p className="text-neutral-600">It knows you'll be late before you do</p>
      <Link to="/onboarding" className="rounded-full bg-neutral-900 px-8 py-3 font-medium text-white hover:bg-neutral-700">
        Start
      </Link>
    </main>
  );
}
