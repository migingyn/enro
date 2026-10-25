import { useEffect, useRef, useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { VoiceOrb } from '../components/VoiceOrb';
import { useVoiceSession } from '../hooks/useVoiceSession';

// Voice-only onboarding (PRD 5). Realtime voice when the backend mints a key, scripted
// text interview otherwise. Either way the profile is saved and we move on to /done.

export default function Onboarding() {
  const { state, captions, level, mode, start, stop, answer } = useVoiceSession();
  const navigate = useNavigate();
  const bottom = useRef<HTMLDivElement>(null);
  const [draft, setDraft] = useState('');

  useEffect(() => {
    if (state === 'done') navigate('/done');
  }, [state, navigate]);

  useEffect(() => {
    bottom.current?.scrollIntoView?.({ behavior: 'smooth' });
  }, [captions.length]);

  const running = state !== 'idle' && state !== 'done' && state !== 'error';
  const canType = mode === 'scripted' && state === 'listening';

  return (
    <main className="mx-auto flex min-h-screen max-w-xl flex-col items-center gap-8 p-6">
      <div className="pt-12">
        <VoiceOrb state={state} level={level} />
      </div>
      <div className="flex gap-3">
        {running ? (
          <button onClick={stop} className="rounded-full border border-neutral-300 px-6 py-2 hover:bg-neutral-100">
            Stop
          </button>
        ) : (
          <button onClick={start} className="rounded-full bg-neutral-900 px-6 py-2 text-white hover:bg-neutral-700">
            {state === 'error' ? 'Try again' : 'Start talking'}
          </button>
        )}
      </div>
      {mode === 'scripted' && running && (
        <form
          className="flex w-full gap-2"
          onSubmit={(e) => {
            e.preventDefault();
            answer(draft);
            setDraft('');
          }}
        >
          <input
            value={draft}
            onChange={(e) => setDraft(e.target.value)}
            disabled={!canType}
            placeholder={canType ? 'Type your answer, or wait for the demo answer' : 'Enro is talking...'}
            className="flex-1 rounded-full border border-neutral-300 px-4 py-2 disabled:bg-neutral-50"
          />
          <button
            type="submit"
            disabled={!canType || !draft.trim()}
            className="rounded-full bg-neutral-900 px-4 py-2 text-white disabled:opacity-40"
          >
            Send
          </button>
        </form>
      )}
      <div className="w-full flex-1 space-y-2 overflow-y-auto" aria-live="polite">
        {captions.map((c, i) => (
          <p key={i} className={c.speaker === 'agent' ? 'text-lg' : 'text-right text-neutral-500'}>
            {c.text}
          </p>
        ))}
        <div ref={bottom} />
      </div>
    </main>
  );
}
