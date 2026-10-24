import type { VoiceState } from '../hooks/useVoiceSession';

// TODO(T21): placeholder orb, replace with the animated voice orb from the visual identity

const STATE_CLASS: Record<VoiceState, string> = {
  idle: 'bg-neutral-300',
  connecting: 'bg-neutral-400 animate-pulse',
  speaking: 'bg-neutral-800 scale-110 animate-pulse',
  listening: 'bg-neutral-500 scale-100',
  saving: 'bg-neutral-400 animate-pulse',
  done: 'bg-emerald-500',
  error: 'bg-red-500',
};

export function VoiceOrb({ state }: { state: VoiceState }) {
  return (
    <div className="flex flex-col items-center gap-3">
      <div
        aria-label={`voice ${state}`}
        className={`h-40 w-40 rounded-full transition-all duration-500 ${STATE_CLASS[state]}`}
      />
      <span className="text-xs uppercase tracking-wide text-neutral-500">{state}</span>
    </div>
  );
}
