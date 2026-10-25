import { useCallback, useEffect, useRef, useState } from 'react';
import { useParams } from 'react-router-dom';
import type { SimCallClientMessage, SimCallServerMessage, Urgency } from '../api/types';
import { simCallMessages } from '../api/fixtures';
import { USE_MOCKS, wsUrl } from '../lib/env';
import { fmtTimeSec, nowTs } from '../lib/time';

const QUICK_REPLIES = ['Five more minutes', "I'm up", 'Leave me alone', "I'm out the door"];

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
  const sayText = (raw: string) => {
    const text = raw.trim();
    if (!text) return;
    setLines((l) => [...l, { speaker: 'user', text, ts: nowTs() }]);
    send({ type: 'speech', ts: nowTs(), data: { call_id: callId, text } });
  };
  const say = (e: React.FormEvent) => {
    e.preventDefault();
    sayText(draft);
    setDraft('');
  };

  const status =
    phase === 'waiting'
      ? 'Waiting for a call...'
      : phase === 'ringing'
        ? `Incoming ${ring?.purpose ?? ''} call · attempt ${ring?.attempt ?? 1}`
        : phase === 'in_call'
          ? 'On call'
          : `Call ended${hangupReason ? ` · ${hangupReason}` : ''}`;

  return (
    <main className="flex min-h-screen items-center justify-center bg-neutral-200 p-4">
      <div className="flex h-[720px] max-h-[95vh] w-full max-w-sm flex-col overflow-hidden rounded-[2.5rem] border-8 border-neutral-900 bg-neutral-950 text-white shadow-2xl">
        <header className="px-6 pb-4 pt-8 text-center">
          <div className="text-[10px] uppercase tracking-widest text-neutral-500">call simulator · {callId}</div>
          <div className="mx-auto mt-4 flex h-20 w-20 items-center justify-center rounded-full bg-emerald-600 text-3xl font-bold">
            {(ring?.caller ?? 'Enro').slice(0, 1)}
          </div>
          <div className="mt-3 text-2xl font-semibold">{ring?.caller ?? 'Enro'}</div>
          <div className="text-sm text-neutral-400">{status}</div>
        </header>

        <ul className="flex-1 space-y-3 overflow-y-auto px-4 text-sm">
          {lines.map((l, i) => (
            <li key={i} className={l.speaker === 'agent' ? 'text-left' : 'text-right'}>
              <div className="mb-0.5 text-[10px] text-neutral-500">
                {l.speaker === 'agent' ? 'Enro' : 'You'} · {fmtTimeSec(l.ts)}
                {l.urgency && <> · {l.urgency}</>}
              </div>
              <div
                className={`inline-block max-w-[85%] rounded-2xl px-3 py-2 text-left ${
                  l.speaker === 'agent' ? 'bg-neutral-800' : 'bg-emerald-600'
                } ${l.urgency === 'URGENT' ? 'ring-1 ring-red-500' : ''}`}
              >
                {l.text}
              </div>
            </li>
          ))}
        </ul>

        <footer className="space-y-3 p-4">
          {phase === 'ringing' && (
            <div className="flex justify-around py-4">
              <button onClick={decline} className="h-16 w-16 rounded-full bg-red-600 text-xs font-medium hover:bg-red-500">
                Decline
              </button>
              <button onClick={answer} className="h-16 w-16 animate-pulse rounded-full bg-emerald-600 text-xs font-medium hover:bg-emerald-500">
                Answer
              </button>
            </div>
          )}

          {phase === 'in_call' && (
            <>
              <div className="flex flex-wrap gap-2">
                {QUICK_REPLIES.map((q) => (
                  <button
                    key={q}
                    onClick={() => sayText(q)}
                    className="rounded-full bg-neutral-800 px-3 py-1 text-xs hover:bg-neutral-700"
                  >
                    {q}
                  </button>
                ))}
              </div>
              <form onSubmit={say} className="flex gap-2">
                <input
                  value={draft}
                  onChange={(e) => setDraft(e.target.value)}
                  placeholder="Say something..."
                  className="min-w-0 flex-1 rounded-full bg-neutral-800 px-4 py-2 text-sm text-white placeholder:text-neutral-500 focus:outline-none"
                />
                <button type="submit" className="rounded-full bg-emerald-600 px-4 py-2 text-sm font-medium hover:bg-emerald-500">
                  Send
                </button>
              </form>
              <div className="flex justify-center">
                <button onClick={decline} className="h-12 w-12 rounded-full bg-red-600 text-[10px] font-medium hover:bg-red-500">
                  End
                </button>
              </div>
            </>
          )}

          {phase === 'ended' && <p className="pb-4 text-center text-sm text-neutral-400">Call ended. Refresh to wait for the next one.</p>}
        </footer>
      </div>
    </main>
  );
}
