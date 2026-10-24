import { useCallback, useEffect, useRef, useState } from 'react';
import { useParams } from 'react-router-dom';
import type { SimCallClientMessage, SimCallServerMessage, Urgency } from '../api/types';
import { simCallMessages } from '../api/fixtures';
import { USE_MOCKS, wsUrl } from '../lib/env';
import { fmtTimeSec, nowTs } from '../lib/time';

// TODO(T20): placeholder phone UI for the browser call simulator

type Phase = 'waiting' | 'ringing' | 'in_call' | 'ended';

interface Line {
  speaker: 'agent' | 'user';
  text: string;
  ts: number;
  urgency?: Urgency;
}

function speak(text: string) {
  if (!('speechSynthesis' in window)) return;
  window.speechSynthesis.cancel();
  window.speechSynthesis.speak(new SpeechSynthesisUtterance(text));
}

export default function CallSimulator() {
  const { callId = '' } = useParams();
  const [phase, setPhase] = useState<Phase>('waiting');
  const [ring, setRing] = useState<{ purpose: string; attempt: number; caller: string } | null>(null);
  const [lines, setLines] = useState<Line[]>([]);
  const [hangupReason, setHangupReason] = useState<string | null>(null);
  const [draft, setDraft] = useState('');
  const ws = useRef<WebSocket | null>(null);
  const mockTimers = useRef<number[]>([]);

  const handle = useCallback((msg: SimCallServerMessage) => {
    switch (msg.type) {
      case 'ring':
        setRing(msg.data);
        setHangupReason(null);
        setPhase('ringing');
        break;
      case 'say':
        setPhase('in_call');
        setLines((l) => [...l, { speaker: 'agent', text: msg.data.text, ts: msg.ts, urgency: msg.data.urgency }]);
        speak(msg.data.text);
        break;
      case 'hangup':
        setHangupReason(msg.data.reason);
        setPhase('ended');
        break;
    }
  }, []);

  // Mock server: answers each client message with the matching simcall_*.json fixture.
  const mockReply = useCallback(
    (msg: SimCallClientMessage) => {
      const later = (ms: number, m: SimCallServerMessage) =>
        mockTimers.current.push(window.setTimeout(() => handle({ ...m, ts: nowTs(), data: { ...m.data, call_id: callId } } as SimCallServerMessage), ms));
      if (msg.type === 'answer' || msg.type === 'speech') later(600, simCallMessages.say);
      if (msg.type === 'decline') later(300, simCallMessages.hangup);
    },
    [callId, handle],
  );

  useEffect(() => {
    if (USE_MOCKS) {
      const { ring: r } = simCallMessages;
      const t = window.setTimeout(() => handle({ ...r, ts: nowTs(), data: { ...r.data, call_id: callId } }), 800);
      mockTimers.current.push(t);
      return () => {
        mockTimers.current.forEach((id) => window.clearTimeout(id));
        mockTimers.current = [];
      };
    }
    const sock = new WebSocket(wsUrl(`/ws/sim-call/${encodeURIComponent(callId)}`));
    ws.current = sock;
    sock.onmessage = (e) => {
      try {
        handle(JSON.parse(String(e.data)) as SimCallServerMessage);
      } catch {
        console.warn('sim-call: bad message', e.data);
      }
    };
    return () => {
      sock.close();
      ws.current = null;
    };
  }, [callId, handle]);

  useEffect(() => () => window.speechSynthesis?.cancel(), []);

  const send = (msg: SimCallClientMessage) => {
    if (USE_MOCKS) mockReply(msg);
    else if (ws.current?.readyState === WebSocket.OPEN) ws.current.send(JSON.stringify(msg));
  };

  const answer = () => {
    setPhase('in_call');
    send({ type: 'answer', ts: nowTs(), data: { call_id: callId } });
  };
  const decline = () => send({ type: 'decline', ts: nowTs(), data: { call_id: callId } });
  const say = (e: React.FormEvent) => {
    e.preventDefault();
    const text = draft.trim();
    if (!text) return;
    setLines((l) => [...l, { speaker: 'user', text, ts: nowTs() }]);
    setDraft('');
    send({ type: 'speech', ts: nowTs(), data: { call_id: callId, text } });
  };

  return (
    <main className="mx-auto flex min-h-screen max-w-sm flex-col gap-4 p-6">
      <header className="text-center">
        <div className="text-xs uppercase tracking-wide text-neutral-500">call simulator · {callId}</div>
        <div className="text-2xl font-bold">{ring?.caller ?? 'Enro'}</div>
        <div className="text-sm text-neutral-600">
          {phase === 'waiting' && 'Waiting for a call...'}
          {phase === 'ringing' && ring && `Incoming ${ring.purpose} call, attempt ${ring.attempt}`}
          {phase === 'in_call' && 'On call'}
          {phase === 'ended' && `Call ended${hangupReason ? ` (${hangupReason})` : ''}`}
        </div>
      </header>

      {phase === 'ringing' && (
        <div className="flex justify-center gap-6">
          <button onClick={decline} className="h-16 w-16 rounded-full bg-red-600 text-sm text-white">
            Decline
          </button>
          <button onClick={answer} className="h-16 w-16 animate-pulse rounded-full bg-emerald-600 text-sm text-white">
            Answer
          </button>
        </div>
      )}

      <ul className="flex-1 space-y-2 text-sm">
        {lines.map((l, i) => (
          <li key={i} className={l.speaker === 'agent' ? '' : 'text-right'}>
            <div className="text-xs text-neutral-500">
              {l.speaker} · {fmtTimeSec(l.ts)}
              {l.urgency && <> · {l.urgency}</>}
            </div>
            <div className={`inline-block rounded px-2 py-1 ${l.speaker === 'agent' ? 'bg-neutral-100' : 'bg-neutral-800 text-white'}`}>
              {l.text}
            </div>
          </li>
        ))}
      </ul>

      {phase === 'in_call' && (
        <form onSubmit={say} className="flex gap-2">
          <input
            value={draft}
            onChange={(e) => setDraft(e.target.value)}
            placeholder="Say something..."
            className="flex-1 rounded border border-neutral-300 px-2 py-1"
          />
          <button type="submit" className="rounded bg-neutral-900 px-3 py-1 text-white">
            Send
          </button>
        </form>
      )}
    </main>
  );
}
