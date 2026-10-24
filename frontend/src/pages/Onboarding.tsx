import { useEffect, useRef } from 'react';
import { useNavigate } from 'react-router-dom';
import { VoiceOrb } from '../components/VoiceOrb';
import { useVoiceSession } from '../hooks/useVoiceSession';

// TODO(T17): placeholder voice-only onboarding, wire the real Realtime session and mic

export default function Onboarding() {
  const { state, captions, start, stop } = useVoiceSession();
  const navigate = useNavigate();
  const bottom = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (state === 'done') navigate('/done');
  }, [state, navigate]);

  useEffect(() => {
    bottom.current?.scrollIntoView?.({ behavior: 'smooth' });
  }, [captions.length]);

  const running = state !== 'idle' && state !== 'done' && state !== 'error';

  return (
    <main className="mx-auto flex min-h-screen max-w-xl flex-col items-center gap-8 p-6">
      <div className="pt-12">
        <VoiceOrb state={state} />
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
