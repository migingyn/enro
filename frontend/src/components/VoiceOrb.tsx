import type { VoiceState } from '../hooks/useVoiceSession';

// Animated voice orb. Color and motion follow `state`; size and glow follow `level` (0..1).

const PALETTE: Record<VoiceState, [string, string]> = {
  idle: ['#d4d4d4', '#a3a3a3'],
  connecting: ['#c7d2fe', '#818cf8'],
  speaking: ['#a5b4fc', '#4f46e5'],
  listening: ['#99f6e4', '#14b8a6'],
  saving: ['#fde68a', '#f59e0b'],
  done: ['#a7f3d0', '#10b981'],
  error: ['#fecaca', '#ef4444'],
};

const MOTION: Partial<Record<VoiceState, string>> = {
  connecting: 'enro-orb-spin 2.4s linear infinite',
  speaking: 'enro-orb-breathe 1.1s ease-in-out infinite',
  listening: 'enro-orb-breathe 2.6s ease-in-out infinite',
  saving: 'enro-orb-spin 1.2s linear infinite',
};

const KEYFRAMES = `
@keyframes enro-orb-breathe { 0%,100% { transform: scale(1) } 50% { transform: scale(1.06) } }
@keyframes enro-orb-spin { to { transform: rotate(360deg) } }
@keyframes enro-orb-ripple { from { transform: scale(1); opacity: .45 } to { transform: scale(1.6); opacity: 0 } }
@media (prefers-reduced-motion: reduce) { .enro-orb * { animation: none !important } }
`;

export function VoiceOrb({ state, level = 0 }: { state: VoiceState; level?: number }) {
  const [inner, outer] = PALETTE[state];
  const lv = Math.max(0, Math.min(1, level));
  const active = state === 'speaking' || state === 'listening';
  const id = `orb-${state}`;

  return (
    <div className="enro-orb flex flex-col items-center gap-3">
      <style>{KEYFRAMES}</style>
      <div className="relative h-48 w-48" role="img" aria-label={`voice ${state}`}>
        {active &&
          [0, 1].map((i) => (
            <span
              key={i}
              className="absolute inset-4 rounded-full"
              style={{
                background: outer,
                animation: `enro-orb-ripple ${state === 'speaking' ? 1.4 : 2.4}s ease-out ${i * 0.7}s infinite`,
              }}
            />
          ))}
        <div
          className="absolute inset-4"
          style={{
            transform: `scale(${1 + lv * 0.25})`,
            transition: 'transform 80ms linear',
            filter: `drop-shadow(0 0 ${8 + lv * 28}px ${outer})`,
          }}
        >
          <svg viewBox="0 0 100 100" className="h-full w-full" style={{ animation: MOTION[state] }}>
            <defs>
              <radialGradient id={id} cx="38%" cy="35%" r="70%">
                <stop offset="0%" stopColor="#ffffff" stopOpacity="0.9" />
                <stop offset="35%" stopColor={inner} />
                <stop offset="100%" stopColor={outer} />
              </radialGradient>
            </defs>
            <circle cx="50" cy="50" r="48" fill={`url(#${id})`} style={{ transition: 'fill 400ms' }} />
            {(state === 'connecting' || state === 'saving') && (
              <circle
                cx="50"
                cy="50"
                r="44"
                fill="none"
                stroke="#ffffff"
                strokeOpacity="0.7"
                strokeWidth="3"
                strokeLinecap="round"
                strokeDasharray="60 220"
              />
            )}
          </svg>
        </div>
      </div>
      <span className="text-xs uppercase tracking-wide text-neutral-500">{state}</span>
    </div>
  );
}
