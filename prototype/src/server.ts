import express, { type Request, type Response, type NextFunction } from 'express';
import { randomUUID, timingSafeEqual } from 'node:crypto';
import path from 'node:path';
import { z } from 'zod';
import { config, saveConfig } from './config.js';
import { Store, localKeys, readPrivate, writePrivate } from './store.js';
import { MotionTracker } from './core/motion.js';
import { LocationTracker } from './core/location.js';
import { PoseTracker } from './core/pose.js';
import { FlatInactivityTracker, inactivityDecision, inactivityWatchKey } from './core/inactivity.js';
import { locationContext } from './core/location-context.js';
import { assessDeparture } from './core/departure.js';
import { departureDemo } from './core/departure-demo.js';
import { createPlan, dueJobs } from './core/planning.js';
import { GoogleAccounts, type GoogleAccountTokens } from './providers/google-accounts.js';
import { estimateRoute, estimateOsrmDrive } from './providers/routes.js';
import { destinationForTravel, travelContext } from './providers/travel-destination.js';
import { assessTrip, tripSatisfiesReminder } from './core/trip.js';
import { preparationRequirements } from './core/requirements.js';
import { voiceReady, placeCall, stopCall, getCall, hasActivePhoneCall, validSignature, voiceResponse, greeting } from './providers/voice.js';
import type { CallSession, CallStatus, EnroEvent, MorningPlan } from './types.js';

const store = new Store();
const keys = localKeys();
const motion = new MotionTracker();
const location = new LocationTracker();
const pose = new PoseTracker();
const inactivity = new FlatInactivityTracker();
const app = express();
app.disable('x-powered-by');
app.use(express.json({ limit: '2mb' }));
app.use(express.urlencoded({ extended: false, limit: '32kb' }));
app.use((_req, res, next) => {
  res.setHeader('X-Content-Type-Options', 'nosniff');
  res.setHeader('Referrer-Policy', 'no-referrer');
  res.setHeader('Cache-Control', 'no-store');
  next();
});
const secureEqual = (left: string, right: string) => Buffer.byteLength(left) === Buffer.byteLength(right) && timingSafeEqual(Buffer.from(left), Buffer.from(right));
const isLocal = (req: Request) => ['127.0.0.1', '::1', '::ffff:127.0.0.1'].includes(req.socket.remoteAddress || '') && ['localhost', '127.0.0.1', '[::1]'].includes(req.hostname);
function localRequest(req: Request) {
  if (!isLocal(req)) return false;
  if (req.get('sec-fetch-site') === 'cross-site') return false;
  const origin = req.get('origin');
  return !origin || origin === `http://${req.get('host')}`;
}
function admin(req: Request, res: Response, next: NextFunction) {
  const token = (req.get('authorization') || '').replace(/^Bearer /, '');
  if (!secureEqual(token, keys.admin)) { res.status(401).json({ error: 'Enter your local console access token.' }); return; }
  next();
}
const google = new GoogleAccounts(() => {
  const c = config();
  return { clientId: c.clientId, clientSecret: c.clientSecret, redirectUri: c.googleRedirectUri };
}, { read: () => readPrivate<GoogleAccountTokens>('google-accounts.json'), write: accounts => writePrivate('google-accounts.json', accounts) });
const coordinatesSchema = z.object({ latitude: z.number().min(-90).max(90), longitude: z.number().min(-180).max(180) });
const preferencesSchema = z.object({
  origin: z.string().trim().max(500).default(''),
  travelModes: z.array(z.enum(['DRIVE', 'WALK', 'TRANSIT'])).min(1).max(3).default(['DRIVE', 'WALK']),
  preparationMinutes: z.number().int().min(0).max(240).default(45),
  bufferMinutes: z.number().int().min(0).max(120).default(10),
  googleAccounts: z.array(z.email()).max(10).default([]),
  locationEnabled: z.boolean().default(false),
  placeAnchor: coordinatesSchema.extend({ label: z.string().trim().min(1).max(100), radiusMeters: z.number().min(50).max(5000), sourceUrl: z.string().url().max(500).optional() }).optional(),
});
const preferences = () => preferencesSchema.parse(readPrivate('preferences.json') || {});
function departureFor(plan: MorningPlan, now = Date.now()) {
  const evidence = motion.snapshot(now, now - 120_000);
  const prefs = preferences();
  const anchor = !plan.originCoordinates && plan.origin === prefs.origin ? prefs.placeAnchor : undefined;
  return assessDeparture({ plan, now, motion: evidence, origin: plan.originCoordinates ?? anchor, originRadiusMeters: anchor?.radiusMeters,
    fixes: preferences().locationEnabled ? location.fixes(now) : [] });
}
function tripFor(plan: MorningPlan, now = Date.now()) {
  const evidence = motion.snapshot(now, now - 120_000);
  const phone = pose.snapshot(now);
  const stream = evidence.deviceId && evidence.sessionId ? {deviceId:evidence.deviceId,sessionId:evidence.sessionId} : undefined;
  return assessTrip(plan, preferences().locationEnabled ? location.fixes(now, stream) : [], now,
    phone.phonePosition.position, evidence.activity.status === 'available' ? evidence.activity.classification : 'unknown');
}
function inactivityFor(plan: MorningPlan, now = Date.now()) {
  const rule = plan.flatInactivity;
  const snapshot = inactivity.snapshot(now, rule?.durationSeconds ?? 300, rule ? Date.parse(rule.armedAt) : -Infinity);
  const key = inactivityWatchKey(plan);
  return { ...snapshot, ...inactivityDecision(plan,snapshot,tripFor(plan,now),now), key,
    jobState: store.state.jobs[key]?.state ?? null, sensorStatus: snapshot.status };
}
const terminal = new Set(['completed', 'busy', 'failed', 'no-answer', 'canceled']);
let lastCallAttempt = 0;
let callDispatching = false;
let syncBusy = false;
const oauthStates = new Map<string, { expiresAt: number; email?: string }>();
const recentSensorPosts = new Map<string, { at: number; count: number }>();
const planGenerations = new Map<string, number>();

app.get('/health', (_req, res) => res.json({ ok: true, service: 'enro' }));
app.get('/api/bootstrap', (req, res) => {
  if (!localRequest(req)) { res.status(403).json({ error: 'Open the console on localhost to initialize access.' }); return; }
  res.json({ token: keys.admin });
});
app.post('/sensor/:token', (req, res) => {
  if (!secureEqual(String(req.params.token), keys.sensor)) { res.sendStatus(401); return; }
  const now = Date.now();
  const entry = recentSensorPosts.get(keys.sensor);
  const bucket = entry && now - entry.at < 1000 ? entry : { at: now, count: 0 };
  bucket.count++;
  recentSensorPosts.set(keys.sensor, bucket);
  if (bucket.count > 20) { res.status(429).json({ error: 'Reduce sensor HTTP push frequency.' }); return; }
  const result = motion.ingest(req.body, now);
  pose.ingest(req.body, now);
  inactivity.ingest(req.body, now);
  location.setEnabled(preferences().locationEnabled);
  location.ingest(req.body, now);
  if (result.accepted) {
    for (const session of store.state.calls.filter(call => call.status === 'in-progress' && call.answeredAt)) {
      const evidence = motion.snapshot(now, Date.parse(session.answeredAt!));
      session.motionEvidence = evidence;
      session.movementObserved ||= evidence.status === 'moving';
    }
    store.save();
  }
  res.json({ ok: true, ...result });
});

app.get('/oauth/google/callback', async (req, res) => {
  const state = String(req.query.state || '');
  const pending = oauthStates.get(state);
  oauthStates.delete(state);
  if (!pending || Date.now() > pending.expiresAt) { res.status(400).send('Connection state expired. Start again from Enro.'); return; }
  if (req.query.error) { res.status(400).send('Google connection was not completed. Return to Enro to try again.'); return; }
  const code = String(req.query.code || '');
  if (!code) { res.status(400).send('Missing authorization code.'); return; }
  await google.connect(code, pending.email);
  store.audit('google-connected', 'Google read-only access connected.');
  res.type('html').send('<!doctype html><title>Enro connected</title><h1>Google account connected.</h1><p>Your other accounts remain connected.</p><p><a href="/#connections">Return to Enro to connect another account or sync events.</a></p>');
});

app.use('/api', admin);
app.get('/api/state', (req, res) => {
  const c = config();
  const prefs = preferences();
  const position = locationContext(prefs.locationEnabled, location.fixes(), prefs.placeAnchor);
  const phonePose = pose.snapshot();
  res.json({
    config: {
      voiceReady: voiceReady(c),
      voiceCredentialsConfigured: /^AC[\da-f]{32}$/i.test(c.accountSid) && c.authToken.length >= 16,
      voiceFromNumberConfigured: /^\+[1-9]\d{7,14}$/.test(c.from),
      voiceTestPhoneConfigured: /^\+[1-9]\d{7,14}$/.test(c.phone),
      voicePublicUrlConfigured: c.baseUrl.startsWith('https://'),
      googleReady: !!(c.clientId && c.clientSecret && c.googleRedirectUri.startsWith('http')), googleConnected: google.list().some(account => account.connected), googleAccounts: google.list(), routesReady: !!c.mapsKey, publicBaseUrl: c.baseUrl, phoneLast4: c.phone.slice(-4), sensorPushUrl: c.baseUrl ? `${c.baseUrl}/sensor/${keys.sensor}` : '', localConfigAllowed: localRequest(req),
    },
    preferences: prefs, pose: phonePose, location: {...position,diagnostic:location.diagnostic()},
    agentContext: { location: position.summary, gpsMovement: position.movement, phonePosition: phonePose.phonePosition.position,
      limitation: 'Phone pose describes the device. GPS movement and being near a place do not establish wakefulness or the intended destination.' },
    motion: motion.snapshot(Date.now(), (() => {
      const active = store.state.calls.find(call => call.status === 'in-progress' && call.answeredAt);
      return active?.answeredAt ? Date.parse(active.answeredAt) : Date.now() - 120_000;
    })()), events: store.state.events.map(event => ({ ...event, travel: travelContext(event) })), plans: store.state.plans,
    departures: store.state.plans.filter(plan => Date.parse(plan.event.startAt) > Date.now()).map(plan => ({ planId: plan.id, ...departureFor(plan) })),
    trips: store.state.plans.filter(plan => Date.parse(plan.event.startAt) > Date.now()).map(plan => ({ planId: plan.id, ...tripFor(plan), ...(plan.flatInactivity ? {inactivity:inactivityFor(plan)} : {}) })),
    calls: store.state.calls.slice(0, 25), audit: store.state.audit,
  });
});
app.post('/api/config', (req, res) => {
  if (!localRequest(req)) { res.status(403).json({ error: 'Save credentials from http://localhost:3333 only.' }); return; }
  saveConfig(req.body);
  store.audit('configuration', 'Local service configuration updated.');
  res.json({ ok: true });
});
app.post('/api/preferences', (req, res) => {
  const updated = preferencesSchema.parse(req.body);
  writePrivate('preferences.json', updated);
  location.setEnabled(updated.locationEnabled);
  res.json(updated);
});
// Isolated demonstration: no fixture samples enter the live trackers, plans, or scheduler.
app.post('/api/demo/departure', (req, res) => {
  const input = z.object({ scenario: z.enum(['stationary', 'pickup', 'shaking', 'walking', 'departed-gps']),
    travelMinutes: z.number().min(1).max(180), clockMinutes: z.number().min(-120).max(120) }).strict().parse(req.body);
  res.json(departureDemo(input));
});
app.get('/api/google/connect', (req, res) => {
  const c = config();
  if (!(c.clientId && c.clientSecret && c.googleRedirectUri.startsWith('http'))) { res.status(409).json({ error: 'Add Google OAuth credentials and the callback URL first.' }); return; }
  const email = req.query.email === undefined ? undefined : z.email().parse(req.query.email).toLowerCase();
  const state = randomUUID();
  for (const [key, pending] of oauthStates) if (pending.expiresAt < Date.now()) oauthStates.delete(key);
  oauthStates.set(state, { expiresAt: Date.now() + 600_000, email });
  res.json({ url: google.authorizationUrl(state, email) });
});
async function syncEvents(emailOnly = false) {
  if (syncBusy) throw new Error('A Google sync is already running.');
  syncBusy = true;
  try {
    const imported = await google.import(emailOnly);
    const calendar = imported.calendar;
    const invitations = imported.invitations.map(event => ({ ...event, status: event.status === 'tentative' && store.state.acceptedEmailIds?.includes(event.id) ? 'confirmed' as const : event.status }));
    const events = [...calendar, ...invitations];
    const isManaged = (event: EnroEvent) => event.id.startsWith('email-ics:') || (!emailOnly && event.source.type === 'calendar');
    const ids = new Set(events.map(event => event.id));
    store.state.events = [...store.state.events.filter(event => !isManaged(event)), ...events];
    for (const plan of store.state.plans) {
      if (!isManaged(plan.event)) continue;
      const latest = events.find(event => event.id === plan.event.id);
      if (!ids.has(plan.event.id) || latest?.status !== 'confirmed') {
        planGenerations.set(plan.id, (planGenerations.get(plan.id) || 0) + 1);
        if (plan.armed) store.audit('plan-disarmed', `${plan.event.title}: no longer a confirmed event in the current bounded import.`);
        plan.armed = false;
      } else if (latest && (latest.startAt !== plan.event.startAt || latest.location !== plan.event.location)) {
        // Disarm immediately so a network failure cannot dispatch an outdated plan.
        const wasArmed = plan.armed;
        plan.armed = false;
        store.save();
        try { await refreshPlan(plan, latest, wasArmed); }
        catch { store.audit('plan-refresh-failed', `${latest.title}: event changed; plan disarmed until its route can be recalculated.`); }
      } else if (latest) {
        plan.event = latest;
      }
    }
    store.audit('google-sync', `${calendar.length} calendar events and ${invitations.length} structured invitations synced.`);
    return events;
  } finally { syncBusy = false; }
}
app.post('/api/google/sync', async (_req, res) => res.json({ events: await syncEvents() }));
app.post('/api/emails/import', async (_req, res) => res.json({ events: await syncEvents(true) }));
app.get('/api/emails', async (_req, res) => res.json({ candidates: await google.candidates() }));
app.post('/api/events/:id/confirm', (req, res) => {
  z.object({ attending: z.literal(true) }).parse(req.body);
  const event = store.state.events.find(event => event.id === req.params.id);
  if (!event || !event.id.startsWith('email-ics:') || event.status === 'cancelled') { res.status(409).json({ error: 'Choose an active imported invitation first.' }); return; }
  event.status = 'confirmed';
  store.state.acceptedEmailIds = [...new Set([...(store.state.acceptedEmailIds || []), event.id])];
  store.audit('attendance-confirmed', `${event.title}: attendance confirmed for planning.`);
  res.json(event);
});

const eventInput = z.object({ title: z.string().trim().min(1).max(200), startAt: z.string().datetime({ offset: true }), timeZone: z.string().min(1).max(100), location: z.string().trim().min(1).max(500), requirements: z.array(z.string().max(150)).max(10).default([]), demoInvitation: z.object({subject:z.string().trim().min(1).max(200),body:z.string().min(1).max(4000)}).optional(), emailId: z.string().max(200).optional() });
app.post('/api/events', (req, res) => {
  const input = eventInput.parse(req.body);
  new Intl.DateTimeFormat('en-US', { timeZone: input.timeZone });
  if (Date.parse(input.startAt) <= Date.now()) { res.status(400).json({ error: 'Choose a future event time.' }); return; }
  const id = randomUUID();
  const event: EnroEvent = { id, title: input.title, startAt: input.startAt, timeZone: input.timeZone, location: input.location, requirements: [...new Set([...input.requirements, ...preparationRequirements(input.demoInvitation?.body ?? '')])], ...(input.demoInvitation ? {demoInvitation:input.demoInvitation} : {}), status: 'confirmed', source: { type: input.emailId ? 'email' : 'manual', id: input.emailId || id } };
  store.state.events.push(event);
  store.audit('event-confirmed', `${event.title}: time and location confirmed.`);
  res.status(201).json(event);
});
const planInput = z.object({ eventId: z.string(), origin: z.string().trim().min(1).max(500), originCoordinates: coordinatesSchema.optional(), destinationCoordinates: coordinatesSchema.optional(), originRadiusMeters: z.number().min(50).max(5000).optional(), destinationRadiusMeters: z.number().min(50).max(5000).optional(), routeProvider: z.enum(['google-routes','osrm']).default('google-routes'), reminderKinds: z.array(z.enum(['wake','leave'])).min(1).max(2).optional(), accessMinutes: z.number().int().min(0).max(120).default(0), travelMode: z.enum(['DRIVE', 'WALK', 'TRANSIT']), preparationMinutes: z.number().int().min(0).max(240), bufferMinutes: z.number().int().min(0).max(120) });
app.post('/api/plans', async (req, res) => {
  const input = planInput.parse(req.body);
  const event = store.state.events.find(event => event.id === input.eventId);
  if (!event) { res.status(404).json({ error: 'Event not found. Sync or confirm an event first.' }); return; }
  if (input.routeProvider === 'osrm' && (input.travelMode !== 'DRIVE' || !input.originCoordinates || !input.destinationCoordinates)) throw new Error('OSRM requires driving and explicit origin and destination landmarks.');
  const route = input.routeProvider === 'osrm' ? await estimateOsrmDrive(input.originCoordinates!, input.destinationCoordinates!)
    : await estimateRoute({ apiKey: config().mapsKey, origin: input.origin, destination: destinationForTravel(event), travelMode: input.travelMode, arrivalAt: new Date(Date.parse(event.startAt) - (input.bufferMinutes + input.accessMinutes) * 60_000).toISOString() });
  const plan = createPlan({ event, origin: input.origin, travelMode: input.travelMode, preparationMinutes: input.preparationMinutes, bufferMinutes: input.bufferMinutes, accessMinutes: input.accessMinutes, route });
  if (input.originCoordinates) plan.originCoordinates = input.originCoordinates;
  if (input.destinationCoordinates) plan.destinationCoordinates = input.destinationCoordinates;
  if (input.originRadiusMeters) plan.originRadiusMeters = input.originRadiusMeters;
  if (input.destinationRadiusMeters) plan.destinationRadiusMeters = input.destinationRadiusMeters;
  if (input.reminderKinds) plan.reminderKinds = [...new Set(input.reminderKinds)];
  store.state.plans.unshift(plan);
  store.audit('plan-created', `${event.title}: schedule calculated from ${route.provider === 'osrm' ? 'OSRM driving without live traffic' : 'Google Routes'}.`);
  res.status(201).json(plan);
});
async function refreshPlan(plan: MorningPlan, event = store.state.events.find(e => e.id === plan.event.id) || plan.event, armed = plan.armed) {
  const generation = (planGenerations.get(plan.id) || 0) + 1;
  planGenerations.set(plan.id, generation);
  if (plan.route.provider === 'osrm' && (!plan.originCoordinates || !plan.destinationCoordinates || event.location !== plan.event.location)) throw new Error('Confirm the updated destination landmark before refreshing this OSRM plan.');
  const route = plan.route.provider === 'osrm' ? await estimateOsrmDrive(plan.originCoordinates!, plan.destinationCoordinates!)
    : await estimateRoute({ apiKey: config().mapsKey, origin: plan.origin, destination: destinationForTravel(event), travelMode: plan.travelMode, arrivalAt: new Date(Date.parse(event.startAt) - (plan.bufferMinutes + (plan.accessMinutes ?? 0)) * 60_000).toISOString() });
  if (planGenerations.get(plan.id) !== generation) return plan;
  const updated = createPlan({ event, origin: plan.origin, travelMode: plan.travelMode, preparationMinutes: plan.preparationMinutes, bufferMinutes: plan.bufferMinutes, accessMinutes: plan.accessMinutes, route });
  const oldWake = plan.wakeAt;
  const oldLeave = plan.leaveAt;
  Object.assign(plan, updated, { id: plan.id, createdAt: plan.createdAt, armed });
  // Preserve completion of each phase when traffic changes; never wake someone twice.
  for (const kind of ['wake', 'leave'] as const) {
    const oldTime = kind === 'wake' ? oldWake : oldLeave;
    const newTime = kind === 'wake' ? plan.wakeAt : plan.leaveAt;
    const oldJob = store.state.jobs[`${plan.id}:${kind}:${oldTime}`];
    if (oldJob) store.state.jobs[`${plan.id}:${kind}:${newTime}`] = { ...oldJob };
  }
  store.audit('plan-refreshed', `${event.title}: route and schedule updated.`);
  return plan;
}
app.post('/api/plans/:id/refresh', async (req, res) => {
  const plan = store.state.plans.find(plan => plan.id === req.params.id);
  if (!plan) { res.sendStatus(404); return; }
  res.json(await refreshPlan(plan));
});
app.post('/api/plans/:id/arm', (req, res) => {
  const { armed } = z.object({ armed: z.boolean() }).parse(req.body);
  const plan = store.state.plans.find(plan => plan.id === req.params.id);
  if (!plan) { res.sendStatus(404); return; }
  if (armed && !voiceReady(config())) { res.status(409).json({ error: 'Configure phone calling before arming reminders.' }); return; }
  if (armed && (plan.event.status !== 'confirmed' || Date.parse(plan.event.startAt) <= Date.now() || !plan.flatInactivity && Date.parse(plan.leaveAt) < Date.now())) { res.status(409).json({ error: 'This event or departure time is no longer eligible. Recalculate first.' }); return; }
  if (armed && (plan.event.source.type === 'calendar' || plan.event.id.startsWith('email-ics:'))) {
    const current = store.state.events.find(event => event.id === plan.event.id && event.status === 'confirmed');
    if (!current || current.startAt !== plan.event.startAt || current.location !== plan.event.location) {
      res.status(409).json({ error: 'The calendar event was removed or changed. Sync and recalculate before arming.' }); return;
    }
  }
  planGenerations.set(plan.id, (planGenerations.get(plan.id) || 0) + 1);
  if (armed && !plan.armed && plan.flatInactivity) plan.flatInactivity.armedAt = new Date().toISOString();
  plan.armed = armed;
  store.audit(armed ? 'plan-armed' : 'plan-disarmed', `${plan.event.title}: ${armed ? `${(plan.reminderKinds ?? ['wake','leave']).join(' and ')} calls enabled for the configured test phone` : 'calls disabled'}.`);
  res.json(plan);
});
app.post('/api/plans/:id/inactivity-rule', (req,res) => {
  const input = z.object({enabled:z.boolean(),durationSeconds:z.union([z.literal(60),z.literal(300)]).default(300),armed:z.boolean().default(false)}).strict().parse(req.body);
  const plan=store.state.plans.find(p=>p.id===req.params.id);
  if (!plan) {res.sendStatus(404);return;}
  if (input.enabled && (!plan.originCoordinates || !plan.destinationCoordinates || plan.event.status !== 'confirmed' || Date.parse(plan.event.startAt)<=Date.now())) {res.status(409).json({error:'Choose a confirmed future event with origin and destination landmarks.'});return;}
  if (input.enabled && input.armed && (!voiceReady(config()) || !preferences().locationEnabled)) {res.status(409).json({error:'Calling and GPS collection must be enabled before arming this rule.'});return;}
  if (input.enabled && input.armed && (plan.event.source.type==='calendar' || plan.event.id.startsWith('email-ics:'))) {
    const current=store.state.events.find(e=>e.id===plan.event.id && e.status==='confirmed');
    if (!current || current.startAt!==plan.event.startAt || current.location!==plan.event.location) {res.status(409).json({error:'The source event changed. Sync and recalculate first.'});return;}
  }
  plan.armed = input.enabled && input.armed;
  if (input.enabled) plan.flatInactivity={durationSeconds:input.durationSeconds,armedAt:new Date().toISOString()};
  else delete plan.flatInactivity;
  planGenerations.set(plan.id,(planGenerations.get(plan.id)||0)+1);
  store.audit('inactivity-rule',`${plan.event.title}: ${input.enabled ? `${input.durationSeconds}-second continuous flat/still departure rule ${plan.armed ? 'armed' : 'configured, disarmed'}` : 'inactivity rule disabled; calls disarmed'}.`);
  res.json(plan);
});

async function startCall(kind: CallSession['kind'], plan?: MorningPlan, rehearsal = false, inactivityTrigger = false): Promise<CallSession> {
  if (callDispatching || store.state.calls.some(call => !terminal.has(call.status))) throw new Error('Another call is active or unresolved. Stop or reconcile it before calling again.');
  if (Date.now() - lastCallAttempt < 15_000) throw new Error('Wait a few seconds before another call attempt.');
  if (!voiceReady(config())) throw new Error('Twilio calling is not configured. Add credentials and a voice number locally first.');
  callDispatching = true;
  lastCallAttempt = Date.now();
  const session: CallSession = { id: randomUUID(), kind, ...(plan ? { planId: plan.id } : {}), ...(rehearsal ? {rehearsal: true} : {}), status: 'starting', createdAt: new Date().toISOString(), verbalConfirmed: false, movementObserved: false, rounds: 0, greeting: greeting(kind, plan, rehearsal) };
  if (inactivityTrigger && plan) {
    const snapshot=inactivityFor(plan);
    session.inactivityEvidence={observedSeconds:snapshot.observedSeconds,requiredSeconds:snapshot.requiredSeconds,lastSampleAt:snapshot.lastSampleAt,observedAt:new Date().toISOString()};
    session.greeting=`Before this call, your phone stayed flat with little motion for at least ${snapshot.requiredSeconds===60 ? 'one minute' : 'five minutes'}, and GPS still placed it near ${plan.origin}. `+session.greeting;
  }
  const prefs = preferences();
  const position = locationContext(prefs.locationEnabled, location.fixes(), prefs.placeAnchor);
  if (!plan && !['off', 'waiting'].includes(position.status)) session.greeting = position.summary + ' ' + session.greeting;
  if (kind === 'leave' && plan) {
    const trip = tripFor(plan);
    session.tripEvidence = trip;
  }
  store.state.calls.unshift(session);
  store.audit('call-starting', `${kind} call requested for the configured test phone.`);
  try {
    const call = await placeCall(config(), session);
    session.callSid = call.sid;
    if (session.status === 'starting') session.status = 'queued';
    store.audit('call-accepted', `Carrier accepted ${kind} call.`);
    return session;
  } catch (error) {
    const providerError = error as { status?: number; code?: number };
    const definiteRejection = typeof providerError.status === 'number' && providerError.status >= 400 && providerError.status < 500;
    session.status = definiteRejection ? 'failed' : 'unknown';
    session.result = definiteRejection ? `Carrier rejected call${providerError.code ? ` (code ${providerError.code})` : ''}. Check trial verification, balance, and number configuration.` : 'Carrier outcome unknown. Check Twilio call logs before another attempt; no retry was made.';
    store.audit('call-error', session.result);
    throw new Error(session.result);
  } finally { callDispatching = false; store.save(); }
}
app.post('/api/calls/test', async (_req, res) => {
  if (voiceReady(config()) && motion.snapshot().sensors.pedometer.status !== 'live') {
    res.status(409).json({ error: 'Enable Pedometer and start native recording. Wait for a fresh step-count reading before the step-and-voice call test.' }); return;
  }
  res.status(201).json(await startCall('test'));
});
app.post('/api/plans/:id/call', async (req, res) => {
  z.object({ rehearsal: z.literal(true) }).strict().parse(req.body);
  const plan = store.state.plans.find(p => p.id === req.params.id);
  if (!plan || plan.event.status !== 'confirmed' || Date.parse(plan.event.startAt) <= Date.now()) { res.status(409).json({error:'Choose a confirmed future plan for the rehearsal.'}); return; }
  res.status(201).json(await startCall('leave', plan, true));
});
app.post('/api/calls/:id/stop', async (req, res) => {
  const session = store.state.calls.find(call => call.id === req.params.id);
  if (!session) { res.sendStatus(404); return; }
  // Stopping an ended call is idempotent and must not rewrite its evidence.
  if (terminal.has(session.status)) { res.json(session); return; }
  if (!session.callSid) { res.status(409).json({ error: 'No carrier call ID is known. Check Twilio call logs before resolving this attempt.' }); return; }
  const call = await stopCall(config(), session.callSid);
  // A signed terminal callback can arrive while the provider request is pending.
  if (terminal.has(session.status)) { res.json(session); return; }
  session.status = call.status as CallStatus;
  if (terminal.has(session.status)) {
    session.finishedAt ||= call.endTime?.toISOString() || new Date().toISOString();
    // stopCall may have merely fetched an already-ended carrier call. Its
    // response establishes status, not that this console request ended it.
    session.result ||= `carrier-${session.status}`;
  }
  store.audit('call-stop-reconciled', `Console stop request processed; carrier reports ${session.status}.`);
  res.status(terminal.has(session.status) ? 200 : 202).json(session);
});
app.post('/api/calls/:id/reconcile', async (req, res) => {
  const session = store.state.calls.find(call => call.id === req.params.id);
  if (!session?.callSid) { res.status(409).json({ error: 'No carrier call ID known; inspect Twilio call logs.' }); return; }
  const call = await getCall(config(), session.callSid);
  session.status = call.status as CallStatus;
  store.audit('call-reconciled', `Carrier reports ${call.status}.`);
  res.json(session);
});

app.use('/voice', (req, res, next) => {
  if (!validSignature(config(), req.get('x-twilio-signature') || '', req.originalUrl, req.body)) { res.sendStatus(403); return; }
  if (req.body.AccountSid !== config().accountSid) { res.sendStatus(403); return; }
  next();
});
app.post('/voice/:id', (req, res) => {
  const session = store.state.calls.find(call => call.id === req.params.id);
  if (!session) { res.sendStatus(404); return; }
  if (session.callSid && session.callSid !== req.body.CallSid) { res.sendStatus(403); return; }
  session.callSid = String(req.body.CallSid);
  if (terminal.has(session.status)) { res.type('text/xml').send('<Response><Hangup/></Response>'); return; }
  session.status = 'in-progress';
  session.answeredAt ||= new Date().toISOString();
  const snapshot = motion.snapshot(Date.now(), Date.parse(session.answeredAt));
  session.motionEvidence = snapshot;
  const plan = store.state.plans.find(plan => plan.id === session.planId);
  const trip = plan ? tripFor(plan) : undefined;
  if (trip) session.tripEvidence = trip;
  const xml = voiceResponse(session, snapshot, { speech: req.body.SpeechResult, digits: req.body.Digits }, plan, Date.now(), trip);
  store.save();
  res.type('text/xml').send(xml);
});
app.post('/voice/:id/status', (req, res) => {
  const session = store.state.calls.find(call => call.id === req.params.id);
  if (!session) { res.sendStatus(404); return; }
  if (session.callSid && session.callSid !== req.body.CallSid) { res.sendStatus(403); return; }
  const sequence = Number(req.body.SequenceNumber);
  if (Number.isFinite(sequence) && session.callbackSequence !== undefined && sequence <= session.callbackSequence) { res.sendStatus(204); return; }
  const status = String(req.body.CallStatus) as CallStatus;
  if (!['queued', 'ringing', 'in-progress', 'completed', 'busy', 'failed', 'no-answer', 'canceled'].includes(status)) { res.sendStatus(400); return; }
  session.callSid = String(req.body.CallSid);
  if (Number.isFinite(sequence)) session.callbackSequence = sequence;
  if (!terminal.has(session.status)) session.status = status;
  if (status === 'in-progress') session.answeredAt ||= new Date().toISOString();
  if (terminal.has(status)) session.finishedAt ||= new Date().toISOString();
  store.audit('call-status', `${session.kind} call: ${status}.`);
  res.sendStatus(204);
});

app.use(express.static(path.resolve('public')));
app.use((error: unknown, _req: Request, res: Response, _next: NextFunction) => {
  if (error instanceof z.ZodError) { res.status(400).json({ error: error.issues.map(issue => `${issue.path.join('.')}: ${issue.message}`).join('; ') }); return; }
  const message = error instanceof Error ? error.message : 'Request failed.';
  // Do not log request bodies, OAuth tokens, sensor coordinates, or provider credentials.
  res.status(400).json({ error: message.slice(0, 600) });
});

let ticking = false;
async function tick() {
  if (ticking || !voiceReady(config())) return;
  ticking = true;
  try {
    const now = Date.now();
    for (const plan of store.state.plans.filter(plan => plan.armed)) {
      for (const kind of plan.reminderKinds ?? ['wake', 'leave'] as const) {
        if (kind==='leave' && plan.flatInactivity) continue;
        const at = kind === 'wake' ? plan.wakeAt : plan.leaveAt;
        const key = `${plan.id}:${kind}:${at}`;
        if (!store.state.jobs[key] && now - Date.parse(at) > 120_000) store.state.jobs[key] = { state: 'missed', at: new Date(now).toISOString(), error: 'Schedule was missed; no late catch-up call made.' };
      }
    }
    store.save();
    const inactivityJobs=store.state.plans.filter(p=>p.armed && p.flatInactivity).flatMap(plan=>{
      const watch=inactivityFor(plan,now);
      if (store.state.jobs[watch.key]) return [];
      if (watch.satisfied) {
        store.state.jobs[watch.key]={state:'satisfied',at:new Date(now).toISOString(),error:'Fresh GPS shows travel progress or the destination area; no inactivity call needed.'};
        store.audit('inactivity-satisfied',`${plan.event.title}: departure watch satisfied by GPS; no call placed.`);
        return [];
      }
      return watch.shouldCall ? [{key:watch.key,kind:'leave' as const,planId:plan.id,dueAt:plan.leaveAt,inactivityTrigger:true}] : [];
    });
    for (const job of [...dueJobs(store.state.plans, store.state.jobs, now).map(job=>({...job,inactivityTrigger:false})),...inactivityJobs]) {
      if (callDispatching || store.state.calls.some(call => !terminal.has(call.status))) break;
      if (Date.now() - lastCallAttempt < 15_000) break;
      const plan = store.state.plans.find(plan => plan.id === job.planId);
      if (!plan?.armed) continue;
      if (job.inactivityTrigger) {
        if (await hasActivePhoneCall(config())) break;
        const watch=inactivityFor(plan);
        if (!watch.shouldCall || watch.key!==job.key || store.state.jobs[job.key] || !plan.armed) continue;
      } else if (!dueJobs([plan], store.state.jobs, Date.now()).some(current => current.key === job.key)) continue;
      const trip = tripFor(plan);
      if (tripSatisfiesReminder(trip, job.kind)) {
        store.state.jobs[job.key] = { state: 'satisfied', at: new Date().toISOString(), error: trip.destinationConfirmed ? 'Repeated fresh GPS already places the phone at the destination; no reminder needed.' : 'Substantial fresh GPS progress toward the destination suggests departure is underway; no leave reminder needed.' };
        store.audit('reminder-satisfied', `${plan.event.title}: ${trip.destinationConfirmed ? 'phone already near destination' : 'substantial GPS progress toward destination'}; ${job.kind} call suppressed.`);
        store.save();
        continue;
      }
      store.state.jobs[job.key] = { state: 'dispatching', at: new Date().toISOString() };
      store.save();
      try {
        const session = await startCall(job.kind, plan, false, job.inactivityTrigger);
        store.state.jobs[job.key] = { state: 'sent', at: new Date().toISOString(), callId: session.id };
      } catch {
        const latest = store.state.calls[0];
        store.state.jobs[job.key] = { state: latest?.status === 'unknown' ? 'unknown' : 'failed', at: new Date().toISOString(), ...(latest ? { callId: latest.id } : {}), error: 'Call could not be confirmed. Check call evidence; no automatic retry.' };
      }
      store.save();
    }
  } finally { ticking = false; }
}
setInterval(() => void tick().catch(() => store.audit('scheduler-error', 'Scheduler failed; inspect plans and call evidence.')), 5000).unref();
setInterval(() => {
  if (!google.list().some(account => account.connected) || !store.state.plans.some(plan => plan.armed)) return;
  void syncEvents().catch(() => store.audit('sync-error', 'Calendar refresh failed; last known schedule remains in use.'));
}, 300_000).unref();
let routesRefreshing = false;
setInterval(() => {
  if (routesRefreshing || !config().mapsKey) return;
  routesRefreshing = true;
  void (async () => {
    for (const plan of store.state.plans.filter(plan => plan.armed && Date.parse(plan.leaveAt) > Date.now())) {
      try { await refreshPlan(plan); } catch { store.audit('route-refresh-error', `${plan.event.title}: travel refresh unavailable; keeping last known schedule.`); }
    }
  })().finally(() => { routesRefreshing = false; });
}, 300_000).unref();

app.listen(config().port, '127.0.0.1', () => console.log(`Enro console: http://localhost:${config().port}`));
