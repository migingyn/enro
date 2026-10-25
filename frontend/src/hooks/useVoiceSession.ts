// Voice onboarding session (PRD 5, 8).
// Live: OpenAI Realtime over WebRTC with the ephemeral key from POST /api/onboarding/session.
// The model fills a save_profile tool call, which we post to /api/onboarding/complete.
// Fake (mock mode, provider "fake", or no key): a scripted text interview over the same
// questions. The user can type an answer, or the scripted answer fills in after a pause.
import { useCallback, useEffect, useRef, useState } from 'react';
import type { OnboardingCompleteRequest, OnboardingQuestion, OnboardingSessionResponse } from '../api/types';
import { onboardingComplete, onboardingSession } from '../api/client';
import { fixtures } from '../api/fixtures';

export type VoiceState = 'idle' | 'connecting' | 'speaking' | 'listening' | 'saving' | 'done' | 'error';

export interface Caption {
  speaker: 'agent' | 'user';
  text: string;
}

export type VoiceMode = 'realtime' | 'scripted';

// Scripted answers per onboarding slot, used when the user doesn't type one.
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
const USER_MS = 4000;
const REALTIME_URL = 'https://api.openai.com/v1/realtime/calls';

// JSON schema for the tool the Realtime model calls once the interview is complete.
const SAVE_PROFILE_TOOL = {
  type: 'function',
  name: 'save_profile',
  description: 'Save the onboarding profile once every question is answered and contacts are read back.',
  parameters: {
    type: 'object',
    properties: {
      profile: {
        type: 'object',
        properties: {
          name: { type: 'string' },
          phone: { type: 'string', description: 'E.164, e.g. +18585550142' },
          timezone: { type: 'string', description: 'IANA zone, default America/Los_Angeles' },
          home_area: { type: 'string' },
          alarm_time: { type: 'string', description: 'HH:MM 24h' },
          sets_own_alarm: { type: 'boolean' },
          preferred_mode: {
            type: 'object',
            description: 'Keys campus, off_campus, night. Values walk, bike, bus, drive, scooter.',
            additionalProperties: { type: 'string' },
          },
          prep_minutes: {
            type: 'object',
            description: 'Keys casual, formal. Values minutes.',
            additionalProperties: { type: 'number' },
          },
          harshness_pref: { type: 'number', description: '0 gentle to 1 brutal' },
        },
        required: ['name', 'phone', 'home_area', 'alarm_time', 'prep_minutes', 'preferred_mode', 'harshness_pref'],
      },
      contacts: {
        type: 'array',
        items: {
          type: 'object',
          properties: {
            spoken_name: { type: 'string' },
            relationship: { type: 'string' },
            phone: { type: 'string', description: 'E.164' },
          },
          required: ['spoken_name', 'relationship', 'phone'],
        },
      },
    },
    required: ['profile', 'contacts'],
  },
};

function isRealtime(session: OnboardingSessionResponse): boolean {
  return session.provider === 'openai' && !!session.client_secret && session.client_secret !== 'fake-ephemeral-key';
}

// Fill gaps in a model-produced profile with safe defaults so the request stays valid.
function normalizeProfile(raw: Partial<OnboardingCompleteRequest> | null): OnboardingCompleteRequest {
  const base = fixtures.onboardingCompleteRequest;
  const p = (raw?.profile ?? {}) as Partial<OnboardingCompleteRequest['profile']>;
  return {
    profile: {
      name: p.name ?? '',
      phone: p.phone ?? '',
      timezone: p.timezone || Intl.DateTimeFormat().resolvedOptions().timeZone || base.profile.timezone,
      home_area: p.home_area ?? '',
      alarm_time: p.alarm_time ?? base.profile.alarm_time,
      sets_own_alarm: p.sets_own_alarm ?? true,
      preferred_mode: p.preferred_mode ?? base.profile.preferred_mode,
      prep_minutes: p.prep_minutes ?? base.profile.prep_minutes,
      harshness_pref: typeof p.harshness_pref === 'number' ? p.harshness_pref : base.profile.harshness_pref,
    },
    contacts: Array.isArray(raw?.contacts) ? raw.contacts : [],
  };
}

export function useVoiceSession(): {
  state: VoiceState;
  captions: Caption[];
  /** 0..1 audio level for the orb (mic or agent voice; simulated in scripted mode). */
  level: number;
  mode: VoiceMode | null;
  start: () => void;
  stop: () => void;
  /** Scripted mode: answer the current question by text. */
  answer: (text: string) => void;
} {
  const [state, setState] = useState<VoiceState>('idle');
  const [captions, setCaptions] = useState<Caption[]>([]);
  const [level, setLevel] = useState(0);
  const [mode, setMode] = useState<VoiceMode | null>(null);
  const stateRef = useRef<VoiceState>('idle');
  const timers = useRef<number[]>([]);
  const cancelled = useRef(false);
  const pendingAnswer = useRef<((text: string | null) => void) | null>(null);
  const pc = useRef<RTCPeerConnection | null>(null);
  const mic = useRef<MediaStream | null>(null);
  const audioEl = useRef<HTMLAudioElement | null>(null);
  const audioCtx = useRef<AudioContext | null>(null);
  const raf = useRef<number | null>(null);

  const set = (s: VoiceState) => {
    stateRef.current = s;
    setState(s);
  };
  const say = (speaker: Caption['speaker'], text: string) => {
    const t = text.trim();
    if (t) setCaptions((c) => [...c, { speaker, text: t }]);
  };

  const clearTimers = () => {
    timers.current.forEach((t) => window.clearTimeout(t));
    timers.current = [];
  };
  const wait = (ms: number) =>
    new Promise<void>((resolve) => timers.current.push(window.setTimeout(resolve, ms)));

  const teardown = () => {
    clearTimers();
    pendingAnswer.current?.(null);
    pendingAnswer.current = null;
    if (raf.current !== null) cancelAnimationFrame(raf.current);
    raf.current = null;
    pc.current?.close();
    pc.current = null;
    mic.current?.getTracks().forEach((t) => t.stop());
    mic.current = null;
    if (audioEl.current) audioEl.current.srcObject = null;
    audioEl.current = null;
    void audioCtx.current?.close().catch(() => undefined);
    audioCtx.current = null;
    setLevel(0);
  };

  // Drive `level` from the loudest of the given streams.
  const meter = (streams: MediaStream[]) => {
    const ctx = audioCtx.current ?? new AudioContext();
    audioCtx.current = ctx;
    const analysers = streams.map((s) => {
      const a = ctx.createAnalyser();
      a.fftSize = 256;
      ctx.createMediaStreamSource(s).connect(a);
      return a;
    });
    const buf = new Uint8Array(128);
    const tick = () => {
      let peak = 0;
      for (const a of analysers) {
        a.getByteTimeDomainData(buf);
        for (const v of buf) peak = Math.max(peak, Math.abs(v - 128) / 128);
      }
      setLevel((l) => l * 0.7 + Math.min(1, peak * 2) * 0.3);
      raf.current = requestAnimationFrame(tick);
    };
    if (raf.current !== null) cancelAnimationFrame(raf.current);
    tick();
  };

  const save = async (body: OnboardingCompleteRequest) => {
    set('saving');
    await onboardingComplete(body);
    say('agent', "I've got it from here.");
    await wait(AGENT_MS);
    if (!cancelled.current) set('done');
  };

  const runScripted = async (questions: OnboardingQuestion[]) => {
    setMode('scripted');
    const answers: Record<string, string> = {};
    for (const q of questions) {
      if (cancelled.current) return;
      set('speaking');
      setLevel(0.6);
      say('agent', q.prompt);
      await wait(AGENT_MS);
      if (cancelled.current) return;
      set('listening');
      setLevel(0.15);
      const typed = await new Promise<string | null>((resolve) => {
        pendingAnswer.current = resolve;
        timers.current.push(window.setTimeout(() => resolve(FAKE_ANSWERS[q.slot] ?? 'Sure.'), USER_MS));
      });
      pendingAnswer.current = null;
      if (cancelled.current || typed === null) return;
      answers[q.slot] = typed;
      say('user', typed);
    }
    if (cancelled.current) return;
    setLevel(0);
    // No LLM in the browser: the seeded profile stands in, with the typed name if given.
    const body = structuredClone(fixtures.onboardingCompleteRequest);
    const name = answers.name?.replace(/^(i'?m|my name is|call me)\s+/i, '').replace(/[.!]+$/, '').trim();
    if (name && answers.name !== FAKE_ANSWERS.name) body.profile.name = name;
    await save(body);
  };

  const runRealtime = async (session: OnboardingSessionResponse) => {
    setMode('realtime');
    const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
    if (cancelled.current) return stream.getTracks().forEach((t) => t.stop());
    mic.current = stream;

    const peer = new RTCPeerConnection();
    pc.current = peer;
    const el = new Audio();
    el.autoplay = true;
    audioEl.current = el;
    peer.ontrack = (e) => {
      el.srcObject = e.streams[0];
      meter([stream, e.streams[0]]);
    };
    stream.getTracks().forEach((t) => peer.addTrack(t, stream));
    peer.onconnectionstatechange = () => {
      if (peer.connectionState === 'failed' && !cancelled.current) set('error');
    };

    const dc = peer.createDataChannel('oai-events');
    let saved = false;
    dc.onopen = () => {
      dc.send(
        JSON.stringify({
          type: 'session.update',
          session: {
            type: 'realtime',
            instructions:
              session.instructions +
              '\nAsk these in order:\n' +
              session.questions.map((q) => `- ${q.prompt}`).join('\n') +
              '\nWhen done, call save_profile, then say a short goodbye.',
            tools: [SAVE_PROFILE_TOOL],
            audio: {
              input: { transcription: { model: 'gpt-4o-mini-transcribe' } },
              ...(session.voice ? { output: { voice: session.voice } } : {}),
            },
          },
        }),
      );
      dc.send(JSON.stringify({ type: 'response.create' }));
      set('listening');
    };
    dc.onmessage = (msg) => {
      let ev: Record<string, unknown>;
      try {
        ev = JSON.parse(String(msg.data)) as Record<string, unknown>;
      } catch {
        return;
      }
      switch (ev.type) {
        case 'output_audio_buffer.started':
        case 'response.output_audio.delta':
        case 'response.audio.delta':
          if (stateRef.current !== 'saving' && stateRef.current !== 'done') set('speaking');
          break;
        case 'output_audio_buffer.stopped':
        case 'input_audio_buffer.speech_started':
          if (stateRef.current === 'speaking') set('listening');
          break;
        case 'response.output_audio_transcript.done':
        case 'response.audio_transcript.done':
          say('agent', String(ev.transcript ?? ''));
          break;
        case 'conversation.item.input_audio_transcription.completed':
          say('user', String(ev.transcript ?? ''));
          break;
        case 'response.function_call_arguments.done': {
          if (ev.name !== 'save_profile' || saved) break;
          saved = true;
          let args: Partial<OnboardingCompleteRequest> | null = null;
          try {
            args = JSON.parse(String(ev.arguments ?? '{}')) as Partial<OnboardingCompleteRequest>;
          } catch {
            args = null;
          }
          save(normalizeProfile(args))
            .then(() => teardown())
            .catch((err: unknown) => {
              console.error('profile save failed', err);
              set('error');
            });
          break;
        }
        case 'error':
          console.error('realtime error', ev);
          break;
      }
    };

    const offer = await peer.createOffer();
    await peer.setLocalDescription(offer);
    const model = session.model ? `?model=${encodeURIComponent(session.model)}` : '';
    const res = await fetch(`${REALTIME_URL}${model}`, {
      method: 'POST',
      body: offer.sdp,
      headers: { Authorization: `Bearer ${session.client_secret}`, 'Content-Type': 'application/sdp' },
    });
    if (!res.ok) throw new Error(`realtime SDP exchange failed: HTTP ${res.status}`);
    if (cancelled.current) return;
    await peer.setRemoteDescription({ type: 'answer', sdp: await res.text() });
  };

  const start = useCallback(() => {
    teardown();
    cancelled.current = false;
    setCaptions([]);
    setMode(null);
    set('connecting');

    const run = async () => {
      const session = await onboardingSession();
      if (cancelled.current) return;
      if (isRealtime(session) && typeof navigator.mediaDevices?.getUserMedia === 'function' && typeof RTCPeerConnection !== 'undefined') {
        try {
          await runRealtime(session);
          return;
        } catch (err) {
          console.warn('realtime unavailable, falling back to scripted interview', err);
          teardown();
          if (cancelled.current) return;
        }
      }
      await runScripted(session.questions);
    };

    run().catch((err: unknown) => {
      console.error('voice session failed', err);
      teardown();
      set('error');
    });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const stop = useCallback(() => {
    cancelled.current = true;
    teardown();
    set('idle');
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const answer = useCallback((text: string) => {
    const t = text.trim();
    if (t && pendingAnswer.current) pendingAnswer.current(t);
  }, []);

  useEffect(
    () => () => {
      cancelled.current = true;
      teardown();
    },
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [],
  );

  return { state, captions, level, mode, start, stop, answer };
}
