// STUB
// TODO(T17): OpenAI Realtime over WebRTC per PRD 8
import { useCallback, useEffect, useRef, useState } from 'react';
import type { OnboardingQuestion } from '../api/types';
import { onboardingComplete, onboardingSession } from '../api/client';
import { fixtures } from '../api/fixtures';

export type VoiceState = 'idle' | 'connecting' | 'speaking' | 'listening' | 'saving' | 'done' | 'error';

export interface Caption {
  speaker: 'agent' | 'user';
  text: string;
}

// Fake answers per onboarding slot, so the stub interview reads like a real one.
const FAKE_ANSWERS: Record<string, string> = {
  name: "I'm Sam.",
  phone: '858 555 0142.',
  home_area: 'La Jolla, near Nobel Drive.',
  alarm_time: 'Seven. Yeah, I set it myself.',
  prep_minutes: 'Fifteen for casual, forty for formal.',
  preferred_mode: 'I walk to class and take the bus off campus.',
  harshness_pref: 'Honest. Not mean.',
  contacts: 'My mom, 619 555 4417. And my roommate Jules, 858 555 0199.',
};

const AGENT_MS = 1800;
const USER_MS = 1400;

/**
 * Walks the onboarding_session questions with fake captions in both mock and live mode,
 * then posts the onboarding_complete_request fixture.
 */
export function useVoiceSession(): {
  state: VoiceState;
  captions: Caption[];
  start: () => void;
  stop: () => void;
} {
  const [state, setState] = useState<VoiceState>('idle');
  const [captions, setCaptions] = useState<Caption[]>([]);
  const timers = useRef<number[]>([]);
  const cancelled = useRef(false);

  const clearTimers = () => {
    timers.current.forEach((t) => window.clearTimeout(t));
    timers.current = [];
  };

  const wait = (ms: number) =>
    new Promise<void>((resolve) => timers.current.push(window.setTimeout(resolve, ms)));

  const start = useCallback(() => {
    clearTimers();
    cancelled.current = false;
    setCaptions([]);
    setState('connecting');

    const run = async () => {
      const session = await onboardingSession();
      const questions: OnboardingQuestion[] = session.questions;
      for (const q of questions) {
        if (cancelled.current) return;
        setState('speaking');
        setCaptions((c) => [...c, { speaker: 'agent', text: q.prompt }]);
        await wait(AGENT_MS);
        if (cancelled.current) return;
        setState('listening');
        await wait(USER_MS);
        if (cancelled.current) return;
        setCaptions((c) => [...c, { speaker: 'user', text: FAKE_ANSWERS[q.slot] ?? 'Sure.' }]);
      }
      if (cancelled.current) return;
      setState('saving');
      await onboardingComplete(fixtures.onboardingCompleteRequest);
      setCaptions((c) => [...c, { speaker: 'agent', text: "I've got it from here." }]);
      await wait(AGENT_MS);
      if (!cancelled.current) setState('done');
    };

    run().catch((err: unknown) => {
      console.error('voice session failed', err);
      setState('error');
    });
  }, []);

  const stop = useCallback(() => {
    cancelled.current = true;
    clearTimers();
    setState('idle');
  }, []);

  useEffect(
    () => () => {
      cancelled.current = true;
      clearTimers();
    },
    [],
  );

  return { state, captions, start, stop };
}
