import twilio from 'twilio';
import type { CallSession, MorningPlan, MotionSnapshot } from '../types.js';
import { PEDOMETER_FRESH_MS } from '../core/motion.js';
import type { WalkingEvidence, ActivityDiagnostic } from '../core/sensor-types.js';
import type { TripContext } from '../core/trip.js';

const speechOptions = { voice: 'Google.en-US-Chirp3-HD-Aoede', language: 'en-US' } as const;

type CallMotionEvidence = MotionSnapshot & {
  walking?: Pick<WalkingEvidence, 'status' | 'thresholdMet' | 'threshold' | 'stepsSinceBaseline' | 'scopedTo' | 'lastSampleAt' | 'activityCorroborated'>;
  activity?: Pick<ActivityDiagnostic, 'status' | 'classification' | 'lastSampleAt' | 'lastReceivedAt'>;
};

export interface VoiceConfig { accountSid: string; authToken: string; from: string; phone: string; baseUrl: string; }
export function voiceReady(c: VoiceConfig) {
  return /^AC[\da-f]{32}$/i.test(c.accountSid) && c.authToken.length >= 16 && /^\+[1-9]\d{7,14}$/.test(c.from) && /^\+[1-9]\d{7,14}$/.test(c.phone) && c.baseUrl.startsWith('https://');
}
export async function placeCall(c: VoiceConfig, session: CallSession) {
  if (!voiceReady(c)) throw new Error('Configure a Twilio account, voice number, test phone, and public HTTPS URL first.');
  const client = twilio(c.accountSid, c.authToken, { autoRetry: false, timeout: 15_000 });
  // A timeout is not safe to retry: the carrier may already have accepted it.
  return client.calls.create({
    to: c.phone, from: c.from,
    url: `${c.baseUrl}/voice/${session.id}`, method: 'POST',
    statusCallback: `${c.baseUrl}/voice/${session.id}/status`, statusCallbackMethod: 'POST',
    statusCallbackEvent: ['initiated', 'ringing', 'answered', 'completed'],
    timeout: 25, timeLimit: 120, record: false,
  });
}
export async function stopCall(c: VoiceConfig, sid: string) {
  const resource = twilio(c.accountSid, c.authToken, { autoRetry: false, timeout: 10_000 }).calls(sid);
  const call = await resource.fetch();
  if (['completed', 'canceled', 'busy', 'failed', 'no-answer'].includes(call.status)) return call;
  return resource.update({ status: ['queued', 'ringing'].includes(call.status) ? 'canceled' : 'completed' });
}
export async function getCall(c: VoiceConfig, sid: string) {
  return twilio(c.accountSid, c.authToken, { autoRetry: false, timeout: 10_000 }).calls(sid).fetch();
}
export async function hasActivePhoneCall(c: VoiceConfig): Promise<boolean> {
  const client=twilio(c.accountSid,c.authToken,{autoRetry:false,timeout:10000});
  for (const status of ['in-progress','ringing','queued'] as const) {
    if ((await client.calls.list({to:c.phone,status,limit:1})).length) return true;
  }
  return false;
}
export function validSignature(c: VoiceConfig, signature: string, originalUrl: string, body: Record<string, string>) {
  return !!c.authToken && !!signature && twilio.validateRequest(c.authToken, signature, c.baseUrl + originalUrl, body);
}
export function greeting(kind: CallSession['kind'], plan?: MorningPlan, rehearsal = false) {
  if (!plan) return "Hey, it's Enro. Time for your wake-up test. If you can, take six steps with your phone. Then say, I am up. I'll check for new steps and walking activity. You can say stop, or press zero, at any time.";
  const format = (iso: string) => new Intl.DateTimeFormat('en-US', { timeZone: plan.event.timeZone, hour: 'numeric', minute: '2-digit' }).format(new Date(iso));
  const event = `${plan.event.title} is at ${format(plan.event.startAt)}, at ${plan.event.location}.`;
  const items = plan.event.requirements.length ? ` Remember ${plan.event.requirements.slice(0, 4).join(', ')}.` : '';
  return kind === 'leave'
    ? `Hey, it's Enro. ${rehearsal ? 'This is your live departure rehearsal.' : "It's time to head out."} ${event}${items} ${rehearsal ? 'Walk toward your parking structure when ready. ' : ''}Say check my progress, or I am leaving. Say stop or press zero to end this reminder.`
    : `Good morning. It's Enro. ${event} Leave by ${format(plan.leaveAt)}. You have ${plan.preparationMinutes} minutes to get ready.${items} If you can, take six steps with your phone. Then say, I am up. Say stop or press zero to end this call.`;
}
export function voiceResponse(session: CallSession, motion: CallMotionEvidence, input: { speech?: string; digits?: string }, plan?: MorningPlan, now = Date.now(), trip?: TripContext) {
  const response = new twilio.twiml.VoiceResponse();
  const speech = (input.speech || '').toLowerCase().trim();
  session.rounds++;
  if (speech) session.lastSpeech = input.speech?.slice(0, 500);
  const moving = motion.status === 'moving' && motion.movementSeconds >= 2;
  session.movementObserved ||= moving;
  const walking = motion.walking;
  const answeredAt = session.answeredAt ? Date.parse(session.answeredAt) : NaN;
  const stepsConfirmed = !!walking && Number.isFinite(answeredAt) && walking.scopedTo === answeredAt &&
    walking.status === 'step-threshold-met' && walking.thresholdMet && walking.stepsSinceBaseline !== null &&
    walking.stepsSinceBaseline >= walking.threshold && walking.lastSampleAt !== null &&
    walking.lastSampleAt >= answeredAt && now - walking.lastSampleAt <= PEDOMETER_FRESH_MS && walking.lastSampleAt <= now + 2_000;
  const activity = motion.activity;
  const activityConfirmed = walking?.activityCorroborated === true && activity?.status === 'available' &&
    (activity.classification === 'walking' || activity.classification === 'running') &&
    [activity.lastSampleAt, activity.lastReceivedAt].every(at => at !== null && Number.isFinite(at) &&
      at >= answeredAt && now - at <= PEDOMETER_FRESH_MS && at <= now + 2_000);
  const finish = (message: string, result: string) => {
    session.result = result;
    response.say(speechOptions, message);
    response.hangup();
    return response.toString();
  };
  // Stop intents take precedence over every readiness or movement decision.
  if (input.digits === '0' || /\b(stop|cancel|hang up|leave me alone)\b/.test(speech)) return finish('Okay. Ending your call.', 'stopped-by-user');
  if (session.rounds > 7 || now - Date.parse(session.createdAt) > 110_000) return finish('Ending this test now. Your progress has not been confirmed.', 'time-limit');
  const negative = /\b(not|never|cannot|can't|don't|asleep|sleeping|back to sleep)\b/.test(speech);
  if (negative) session.verbalConfirmed = false;
  const confirmed = !negative && (input.digits === '1' || /\b(i am up|i'm up|im up|awake|ready|leaving|on my way)\b/.test(speech));
  session.verbalConfirmed ||= confirmed;
  const departureReported = !negative && (input.digits === '1' || /^(?:yes[, ]+)?(?:i(?: am|'m|m) )?(?:leaving(?: now)?|on my way|walking to (?:the |my )?(?:parking(?: structure| garage)?|car))[.!]?$/.test(speech));
  const tripFresh = trip && now - Date.parse(trip.observedAt) <= 5000 && Date.parse(trip.observedAt) <= now + 2000;
  const sensorContext = tripFresh ? `${trip.summary} Your phone is ${trip.phonePosition}; activity reports ${trip.activity}.` : 'Live trip context is unavailable.';
  if (session.kind === 'leave' && departureReported) return finish(`I heard that you are heading out. ${tripFresh ? trip.summary : 'GPS departure is unconfirmed.'} Ending this reminder.`, 'departure-reported-by-user');
  if (session.kind === 'leave') session.verbalConfirmed = departureReported;
  if (session.verbalConfirmed && stepsConfirmed && activityConfirmed) return finish('Your phone reports new pedometer counts and walking activity, and I heard your check-in. Ending this check.', 'steps-activity-and-voice-confirmed');
  let prompt = session.rounds === 1 ? session.greeting : 'Take six steps with your phone if you can, then say I am up. Say stop or press zero to finish.';
  if (session.verbalConfirmed && !stepsConfirmed) prompt = !walking || ['unavailable', 'stale', 'baseline-needed', 'counter-reset'].includes(walking.status)
    ? 'I heard your check-in, but new step evidence is not live yet. Your phone may report steps with a delay. You can say stop to end the test.'
    : `I heard you. I have ${walking.stepsSinceBaseline ?? 0} pedometer counts out of ${walking.threshold}. Walk with your phone if you can, then check in again. You can always say stop.`;
  if (session.kind !== 'leave' && stepsConfirmed && !activityConfirmed) prompt = 'I received pedometer counts, but I do not have fresh walking activity to corroborate them. Shaking can increase the counter. Walk if you can, or say stop to end this test.';
  if (session.kind === 'leave') prompt = session.rounds === 1
    ? `${session.greeting} ${sensorContext}`
    : `${sensorContext} ${stepsConfirmed && activityConfirmed ? 'New pedometer counts and walking activity were received. ' : ''}Say check my progress, I am leaving, or stop.`;
  if (/\b(snooze|more minutes|ten minutes|five minutes)\b/.test(speech)) {
    const minutes = /\b(ten|10)\b/.test(speech) ? 10 : 5;
    if (plan) {
      const remaining = Math.floor((Date.parse(plan.leaveAt) - now) / 60_000);
      prompt = `${minutes} more minutes would leave ${Math.max(0, remaining - minutes)} minutes until your planned departure. I have not changed the schedule. Say stop to end this call, or I am up to check in.`;
    } else prompt = 'This is a live test, so I have not scheduled another call. Say stop to finish, or I am up to check in.';
  }
  if (session.kind === 'leave' && /\bwhere\b.*\b(parking|parked|car)\b/.test(speech)) {
    prompt = `I do not have the location of your parked car or parking structure. ${sensorContext} Say I am leaving when you head out, or stop to end this reminder.`;
  }
  const gather = response.gather({ input: ['speech', 'dtmf'], action: `/voice/${session.id}`, method: 'POST', timeout: 4, speechTimeout: 'auto', numDigits: 1, actionOnEmptyResult: true });
  gather.say(speechOptions, prompt);
  return response.toString();
}
