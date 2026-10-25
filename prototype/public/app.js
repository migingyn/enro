'use strict';
const $ = (id) => document.getElementById(id);
const localZone = Intl.DateTimeFormat().resolvedOptions().timeZone;
let token = new URLSearchParams(location.hash.slice(1)).get('token') || sessionStorage.getItem('enro-token') || '';
if (token) { sessionStorage.setItem('enro-token', token); if (location.hash.includes('token=')) history.replaceState(null, '', location.pathname + location.search); }
let state = null;
let polling = false;
let motionHistory = [];
let lastMotionTimestamp = null;
let lastEventKey = '';
let lastPlanKey = '';
let lastCallKey = '';
let lastAuditKey = '';
let lastGoogleAccountKey = '';
let preferencesLoaded = false;
let demoScenario = null;
let demoAssessmentPending = false;
let demoRequestVersion = 0;
let noticeTimer;
let placeLoaded = false;
const activeStatuses = new Set(['starting', 'queued', 'ringing', 'in-progress', 'unknown']);

function node(tag, className, content) { const el = document.createElement(tag); if (className) el.className = className; if (content !== undefined) el.textContent = String(content); return el; }
function setText(id, value) { $(id).textContent = value; }
function time(value, zone = localZone) { try { return new Intl.DateTimeFormat(undefined, { hour: 'numeric', minute: '2-digit', timeZone: zone }).format(new Date(value)); } catch { return 'Unavailable'; } }
function date(value, zone = localZone) { try { return new Intl.DateTimeFormat(undefined, { weekday: 'short', month: 'short', day: 'numeric', timeZone: zone }).format(new Date(value)); } catch { return 'Unavailable'; } }
function safeUrl(value) { try { const u = new URL(value); return ['https:', 'http:'].includes(u.protocol) ? u.href : null; } catch { return null; } }
function relative(timestamp) { if (!timestamp) return 'No samples'; const seconds = Math.max(0, Math.floor((Date.now() - new Date(timestamp).getTime()) / 1000)); return seconds < 2 ? 'just now' : seconds < 60 ? `${seconds}s ago` : `${Math.floor(seconds / 60)}m ago`; }
function notify(message, error = false) { clearTimeout(noticeTimer); $('notice').className = `notice${error ? ' error' : ''}`; $('notice').textContent = message; if (!error) noticeTimer = setTimeout(() => $('notice').classList.add('hidden'), 8000); }
async function api(path, options = {}) {
  const response = await fetch(path, { ...options, headers: { Authorization: `Bearer ${token}`, ...(options.body ? { 'Content-Type': 'application/json' } : {}), ...options.headers } });
  const data = await response.json().catch(() => ({}));
  if (!response.ok) { if (response.status === 401) $('access-panel').classList.remove('hidden'); throw new Error(typeof data.error === 'string' ? data.error : `Request failed (${response.status}).`); }
  return data;
}
async function action(button, task) { const wasDisabled = button.disabled; button.disabled = true; button.setAttribute('aria-busy', 'true'); try { await task(); await poll(); } catch (error) { notify(error.message || 'Something went wrong.', true); } finally { button.removeAttribute('aria-busy'); button.disabled = wasDisabled; if (state) renderConfig(state.config); } }
function tickClock() { const now = new Date(); setText('clock', time(now)); setText('today-date', new Intl.DateTimeFormat(undefined, { weekday: 'long', month: 'long', day: 'numeric' }).format(now).toUpperCase()); setText('clock-zone', localZone.replaceAll('_', ' ')); }
function tag(id, text, style = '') { $(id).textContent = text; $(id).className = `tag ${style}`.trim(); }
function stagedTime(minutesAfterSix) {
  const total = ((18 * 60 + Math.round(minutesAfterSix)) % 1440 + 1440) % 1440;
  const hours = Math.floor(total / 60);
  return `${hours % 12 || 12}:${String(total % 60).padStart(2, '0')} ${hours >= 12 ? 'PM' : 'AM'}`;
}
function departureView(assessment, hypothetical = false) {
  const view = node('div', 'departure-copy');
  if (!assessment || typeof assessment.status !== 'string') { view.append(node('p', 'fineprint', 'Departure evidence is not available yet.')); return view; }
  const labels = {
    'not-yet-time': 'Before planned leave time',
    'time-to-leave': 'Time to leave',
    'still-inactive': 'Time to leave · no activity observed',
    'activity-departure-unconfirmed': 'Activity detected · departure unconfirmed',
    'outside-origin-confirmed': 'Phone outside the starting area',
  };
  const confirmed = assessment.status === 'outside-origin-confirmed';
  const warning = assessment.timeToLeave && !confirmed;
  view.append(node('span', `tag ${confirmed ? 'live' : warning ? 'warning' : ''}`, labels[assessment.status] || 'Departure unknown'));
  if (assessment.reason) view.append(node('p', 'departure-reason', assessment.reason));
  const evidence = assessment.evidence || {};
  const stepDetail = Number.isFinite(evidence.stepDelta) ? `${evidence.stepDelta} extra steps` : 'Steps unknown';
  const activityDetail = assessment.activity === 'active' ? 'Activity observed' : assessment.activity === 'inactive' ? 'No recent activity observed' : 'Activity unknown';
  const locationDetail = {
    unknown: 'GPS off / no usable location evidence',
    'inside-origin': 'Phone within the starting area',
    'outside-origin-unconfirmed': 'Outside-area readings · waiting for confirmation',
    'outside-origin-confirmed': 'Outside-area GPS evidence confirmed',
  }[assessment.location] || 'Location unknown';
  view.append(node('p', 'departure-evidence', `${stepDetail} · ${activityDetail} · ${locationDetail}`));
  if (evidence.gps?.reason) view.append(node('p', 'fineprint', evidence.gps.reason));
  view.append(node('p', 'fineprint departure-limit', confirmed
    ? `${hypothetical ? 'Staged GPS' : 'GPS'} places the phone outside the starting area. It does not establish its route or arrival at the destination.`
    : 'Steps, pickup, or shaking alone do not show that the phone has left for this destination.'));
  return view;
}
function renderDemoSchedule(data = null) {
  const enteredTravel = Number($('demo-travel').value);
  const travelMinutes = data?.travelMinutes ?? (Number.isFinite(enteredTravel) ? enteredTravel : 20);
  const clockMinutes = data?.clockMinutes ?? Number($('demo-clock').value);
  const bufferMinutes = data?.bufferMinutes ?? 10;
  const preparationMinutes = data?.preparationMinutes ?? 45;
  const leaveMinutes = data?.leaveClockMinutes ?? 30 - travelMinutes - bufferMinutes;
  const wakeMinutes = data?.wakeClockMinutes ?? leaveMinutes - preparationMinutes;
  setText('demo-clock-label', stagedTime(clockMinutes));
  setText('demo-wake', stagedTime(wakeMinutes));
  setText('demo-leave', stagedTime(leaveMinutes));
  setText('demo-assumption', `Assumes ${travelMinutes} min travel, ${bufferMinutes} min arrival buffer, and ${preparationMinutes} min preparation.${leaveMinutes < 0 ? ' Departure would need to be before the staged 6:00 PM start.' : ''} No real event is created or call placed.`);
}
function invalidateDemo() {
  demoRequestVersion++;
  demoScenario = null;
  renderDemoSchedule();
  for (const button of document.querySelectorAll('[data-departure-scenario]')) button.setAttribute('aria-pressed', 'false');
  $('demo-departure-result').replaceChildren(node('p', 'fineprint', 'Times changed. Choose a scenario to evaluate these assumptions.'));
}
async function runDepartureDemo(scenario) {
  if (demoAssessmentPending) return;
  const travelInput = $('demo-travel');
  if (!travelInput.reportValidity()) return;
  const travelMinutes = Number(travelInput.value);
  const clockMinutes = Number($('demo-clock').value);
  const requestVersion = ++demoRequestVersion;
  const buttons = [...document.querySelectorAll('[data-departure-scenario]')];
  demoAssessmentPending = true;
  for (const button of buttons) { button.disabled = true; button.setAttribute('aria-busy', String(button.dataset.departureScenario === scenario)); }
  try {
    const data = await api('/api/demo/departure', { method: 'POST', body: JSON.stringify({ scenario, travelMinutes, clockMinutes }) });
    if (requestVersion !== demoRequestVersion) return;
    if (!data.assessment) throw new Error('No hypothetical assessment was returned.');
    demoScenario = scenario;
    renderDemoSchedule(data);
    const names = { stationary: 'Still phone', pickup: 'Pickup', shaking: 'Shaking', walking: 'Walking', 'departed-gps': 'Outside origin · GPS' };
    $('demo-departure-result').replaceChildren(node('p', 'demo-result-label', `Staged ${stagedTime(clockMinutes)} · ${names[scenario] || 'Scenario'}`), departureView(data.assessment, true));
  } catch (error) {
    if (requestVersion === demoRequestVersion) $('demo-departure-result').replaceChildren(node('p', 'demo-error', error.message || 'The hypothetical assessment could not be evaluated.'));
  } finally {
    demoAssessmentPending = false;
    for (const button of buttons) { button.disabled = false; button.removeAttribute('aria-busy'); button.setAttribute('aria-pressed', String(button.dataset.departureScenario === demoScenario)); }
  }
}
function renderConfig(config) {
  if (!config) return;
  $('config-settings').classList.toggle('hidden', !config.localConfigAllowed);
  $('local-config-help').classList.toggle('hidden', !!config.localConfigAllowed);
  const busy = (id) => $(id).getAttribute('aria-busy') === 'true';
  const motionLive = state?.motion?.sensors?.pedometer?.status === 'live';
  const missingVoiceSetup = [
    !config.voiceCredentialsConfigured && 'Twilio credentials',
    !config.voiceFromNumberConfigured && 'a Twilio calling number',
    !config.voiceTestPhoneConfigured && 'the test phone number',
    !config.voicePublicUrlConfigured && 'a public HTTPS URL',
  ].filter(Boolean);
  const voiceSetupHelp = `Add ${missingVoiceSetup.join(', ')} to configure calling.`;
  const activeCall = (state?.calls || []).some(call => !['completed', 'busy', 'failed', 'no-answer', 'canceled'].includes(call.status));
  $('test-call').disabled = !config.voiceReady || !motionLive || activeCall || busy('test-call');
  $('google-connect').disabled = !config.googleReady || busy('google-connect');
  $('google-sync').disabled = !config.googleConnected || busy('google-sync');
  $('load-emails').disabled = !config.googleConnected || busy('load-emails');
  $('import-invitations').disabled = !config.googleConnected || busy('import-invitations');
  renderEventSelection();
  $('copy-sensor').disabled = !config.sensorPushUrl;
  $('sensor-url').value = config.sensorPushUrl || '';
  setText('phone-target', config.phoneLast4 ? `Authorized test phone · •••• ${config.phoneLast4}` : 'Your authorized test phone');
  setText('call-help', config.voiceReady ? motionLive ? 'Pedometer is live. The experimental call requires new counter increments, fresh walking Activity, and a spoken check in.' : 'Calling is configured. Enable Pedometer and Activity, start recording, and wait for a fresh counter reading to enable the test.' : `${voiceSetupHelp}${motionLive ? '' : ' Start Pedometer and Activity recording for the combined test.'}`);
  setText('voice-config-label', config.voiceReady ? 'Ready to place a real call' : `${config.voiceCredentialsConfigured ? 'Twilio credentials configured. ' : ''}${voiceSetupHelp}`);
  setText('google-config-label', config.googleConnected ? `${(config.googleAccounts || []).filter(a => a.connected).length} Google account(s) connected` : config.googleReady ? 'Ready for your Google authorization' : 'OAuth credentials needed');
  setText('google-sync-help', config.googleConnected ? 'Imports events from all connected Google accounts.' : 'Connect Google below to import confirmed events.');
  setText('google-connect', 'Connect account ↗');
  const accounts = [...new Set([...(state?.preferences?.googleAccounts || []), ...(config.googleAccounts || []).map(a => a.email)])];
  const accountKey = JSON.stringify([accounts, config.googleAccounts]);
  if (accountKey !== lastGoogleAccountKey) {
    const selectedAccount = $('google-account').value;
    $('google-account').replaceChildren();
    for (const email of accounts) { const option = node('option', '', email); option.value = email; $('google-account').append(option); }
    const other = node('option', '', 'Another Google account'); other.value = ''; $('google-account').append(other);
    if (accounts.includes(selectedAccount)) $('google-account').value = selectedAccount;
    $('google-accounts-list').replaceChildren(...(config.googleAccounts || []).map(account => node('p', '', `${account.email} · ${account.connected ? 'Connected' : 'Reconnect needed'}`)));
    lastGoogleAccountKey = accountKey;
  }
  if (!preferencesLoaded && state?.preferences) {
    $('plan-origin').value = state.preferences.origin || '';
    $('travel-mode').value = state.preferences.travelModes?.[0] || 'DRIVE';
    $('prep-minutes').value = state.preferences.preparationMinutes;
    $('buffer-minutes').value = state.preferences.bufferMinutes;
    preferencesLoaded = true;
  }
  setText('routes-config-label', config.routesReady ? 'Google Routes key configured' : 'Google Routes key needed');
  $('routes-status-dot').className = `status-dot${config.routesReady ? ' good' : ''}`;
  setText('routes-help', config.routesReady ? 'Uses a real Google Routes estimate. Review the calculated times before arming calls.' : 'Connect Google Routes to calculate a live travel estimate.');
  if (!$('cfg-public').value && config.publicBaseUrl) $('cfg-public').placeholder = config.publicBaseUrl;
}
function renderMotion(motion) {
  if (!motion) return;
  const received = motion.lastReceivedAt;
  const stale = received && Date.now() - Number(received) > 15000;
  const status = stale ? 'stale' : motion.status;
  const labels = { unavailable: 'Not connected', stale: 'Signal stale', stationary: 'Live · Still', moving: 'Live · Moving' };
  tag('motion-tag', labels[status] || 'Unknown', status === 'moving' || status === 'stationary' ? 'live' : status === 'stale' ? 'warning' : '');
  tag('sensor-setup-status', status === 'moving' || status === 'stationary' ? 'Streaming live' : status === 'stale' ? 'Stream stale' : 'Not streaming', status === 'moving' || status === 'stationary' ? 'live' : status === 'stale' ? 'warning' : '');
  setText('motion-seconds', motion.sampleCount ? Math.round(Number(motion.movementSeconds) * 10) / 10 : ' · ');
  setText('motion-samples', motion.sampleCount ? Number(motion.sampleCount).toLocaleString() : ' · ');
  setText('motion-updated', received ? `Received ${relative(received)}${motion.latencyMs != null ? ` · ${Math.max(0, Math.round(motion.latencyMs))}ms latency` : ''}` : 'No stream connected');
  const walking = motion.walking;
  const walkingCountKnown = walking && Number.isFinite(walking.stepsSinceBaseline) && walking.baselineSteps != null;
  setText('motion-steps', walkingCountKnown ? walking.stepsSinceBaseline : ' · ');
  const threshold = walking?.threshold ?? 6;
  const walkingLabels = {
    unavailable: 'Pedometer unavailable. Acceleration alone cannot confirm steps.',
    'baseline-needed': 'Waiting for a fresh Pedometer baseline before counting extra steps.',
    baseline: 'Baseline received. Waiting for extra steps.',
    stale: 'Pedometer signal stale. The displayed step count is historical.',
    insufficient: `${walking?.stepsSinceBaseline ?? 0} of ${threshold} extra steps observed. Keep the phone with you.`,
    'counter-reset': 'Pedometer counter reset. Waiting for a new baseline.',
    'step-threshold-met': walking?.activityCorroborated ? 'Counter threshold and fresh walking Activity agree. Physical validation is still required.' : `${threshold} pedometer increments detected. Walking Activity has not corroborated them; shaking can raise this counter.`,
    'steps-detected': 'Counter increments detected. Walking remains unconfirmed.',
  };
  setText('walking-evidence', walkingLabels[walking?.status] || 'Waiting for Pedometer readings. Acceleration alone cannot confirm steps.');
  $('walking-evidence').classList.toggle('confirmed', !!walking?.thresholdMet && !!walking?.activityCorroborated && walking.status !== 'stale');
  const gravity = motion.gravity;
  const pose = state?.pose;
  setText('orientation-evidence', pose?.orientation?.status === 'live' ? `${pose.phonePosition.position} phone · ${Math.round(pose.orientation.rotationFromBaselineDegrees || 0)}° rotation from baseline` : `Orientation ${pose?.orientation?.status || 'unavailable'}`);
  setText('gyroscope-evidence', pose?.gyroscope?.status === 'live' ? `${pose.gyroscope.rotationRate.toFixed(2)} rad/s rotation rate` : `Gyroscope ${pose?.gyroscope?.status || 'unavailable'}`);
  setText('pickup-evidence', gravity?.status === 'available'
    ? gravity.pickupPossible ? `Possible pickup · ${Math.round(gravity.orientationChangeDegrees || 0)}° phone rotation` : 'No pickup inferred from recent phone orientation'
    : gravity?.status === 'stale' ? 'Unknown · Gravity reading stale' : 'Unknown · Gravity unavailable');
  const activity = motion.activity;
  setText('activity-evidence', activity?.status === 'available'
    ? `Phone reports ${activity.classification} · supporting evidence only${walking?.activityCorroborated && walking?.thresholdMet ? ' · corroborates new steps' : ''}`
    : activity?.status === 'unknown-schema' ? 'Unknown · reading format unrecognized' : activity?.status === 'stale' ? 'Unknown · Activity reading stale' : 'Unknown · no recognized Activity reading');
  const liveLocation = state?.location;
  setText('gps-evidence', state?.preferences?.locationEnabled === false ? 'Off · location evidence is not collected'
    : liveLocation?.current ? `${liveLocation.status === 'imprecise' ? 'Imprecise' : 'Live'} · ±${Math.round(liveLocation.current.accuracyMeters)} m · ${liveLocation.fixCount} recent fixes`
    : state?.preferences?.locationEnabled ? 'Enabled · awaiting a fresh location fix' : 'Off / no usable location evidence');
  setText('motion-evidence', status === 'stale' ? 'Acceleration is stale. Check the separate sensor statuses above. Phone orientation never establishes body position or destination.' : 'A phone\'s orientation does not describe your body position. Movement alone does not show where you are going.');
  if (motion.lastSampleAt !== lastMotionTimestamp && Number.isFinite(motion.latestMagnitude)) {
    lastMotionTimestamp = motion.lastSampleAt;
    motionHistory.push({ at: motion.lastSampleAt, value: motion.latestMagnitude });
    motionHistory = motionHistory.slice(-60);
  }
  const hasChart = motionHistory.length > 1;
  $('chart-empty').classList.toggle('hidden', hasChart);
  if (!hasChart) setText('chart-empty', motion.sampleCount ? 'Collecting observed samples…' : 'Waiting for real sensor data');
  const max = Math.max(1, ...motionHistory.map(s => s.value));
  $('motion-line').setAttribute('points', hasChart ? motionHistory.map((sample, i) => `${i / (motionHistory.length - 1) * 420},${74 - sample.value / max * 65}`).join(' ') : '');
  $('motion-chart').setAttribute('aria-label', hasChart ? `${motionHistory.length} observed acceleration samples; latest magnitude ${motion.latestMagnitude?.toFixed(2)}` : 'No motion observations yet');
}
function renderLocation() {
  const gps = state?.location;
  if (!gps) return;
  const labels = {off:'GPS off',waiting:'Waiting for GPS',imprecise:'GPS imprecise',live:'GPS live','near-place':'Near reference place','staying-near-place':'Still near reference place','moving-near-place':'Moving within area','moving-away':'Moving away','outside-place':'Outside reference area',uncertain:'Boundary uncertain'};
  tag('location-tag', labels[gps.status] || 'Unknown', ['imprecise','uncertain'].includes(gps.status) ? 'warning' : gps.current ? 'live' : '');
  setText('location-summary', gps.summary);
  setText('location-distance', gps.distanceFromPlaceMeters == null ? 'Unknown' : `${Math.round(gps.distanceFromPlaceMeters)} m`);
  setText('location-accuracy', gps.current ? `±${Math.round(gps.current.accuracyMeters)} m` : 'Unknown');
  setText('location-speed', gps.current?.speedMetersPerSecond == null ? 'Unavailable' : `${(gps.current.speedMetersPerSecond * 3.6).toFixed(1)} km/h${gps.current.speedAccuracyMetersPerSecond == null ? ' · accuracy unknown' : ` ±${(gps.current.speedAccuracyMetersPerSecond * 3.6).toFixed(1)}`}`);
  setText('location-movement', `${gps.movement || 'unknown'}${gps.trend !== 'unknown' ? ` · ${gps.trend} reference` : ''}`);
  setText('location-updated', gps.current ? relative(gps.current.timestamp) : 'No fresh fix');
  const nextTripPlan=[...(state.plans || [])].filter(p=>Date.parse(p.event.startAt)>Date.now()).sort((a,b)=>Date.parse(a.event.startAt)-Date.parse(b.event.startAt))[0];
  const nextTrip=(state.trips || []).find(t=>t.planId===nextTripPlan?.id);
  setText('agent-context', `${nextTrip?.summary || state.agentContext?.location || gps.summary} Phone position: ${state.pose?.phonePosition?.position || 'unknown'}. Activity reports: ${state.motion?.activity?.status === 'available' ? state.motion.activity.classification : 'unknown or stale'}.`);
  $('location-enabled').checked = !!state.preferences.locationEnabled;
  setText('gps-coordinate-details', gps.current ? `Latest GPS: ${gps.current.latitude.toFixed(6)}, ${gps.current.longitude.toFixed(6)}. ${gps.fixCount} fresh fixes across ${Math.round(gps.spanSeconds)} seconds.` : 'No fresh GPS coordinates.');
  setText('pose-details', state.pose?.orientation?.quaternion ? `Orientation quaternion [w,x,y,z]: ${state.pose.orientation.quaternion.map(n => n.toFixed(3)).join(', ')}. Phone pose does not establish body posture.` : 'Waiting for a valid orientation quaternion.');
  setText('sensor-catalog', `Sensor names received: ${(state.motion?.observedSensors || []).join(', ') || 'none'}. The console decodes orientation, gyroscope, gravity, acceleration, Pedometer, Activity, and location.`);
  if (!placeLoaded && state.preferences.placeAnchor) {
    const a = state.preferences.placeAnchor;
    $('place-label').value = a.label; $('place-lat').value = a.latitude; $('place-lon').value = a.longitude; $('place-radius').value = a.radiusMeters;
    placeLoaded = true;
  }
  const trail = gps.trail || [];
  const range = Math.max(gps.place?.radiusMeters || 300, ...trail.flatMap(p => [Math.abs(p.east),Math.abs(p.north)])) * 1.15;
  const scale = 110 / range;
  setText('plot-place', gps.place?.label || 'Set place reference');
  $('place-ring').setAttribute('r', gps.place ? gps.place.radiusMeters * scale : 0);
  $('gps-trail').setAttribute('points', trail.map(p => `${200+p.east*scale},${130-p.north*scale}`).join(' '));
  const latest = trail.at(-1);
  for (const id of ['gps-dot','gps-accuracy']) {
    $(id).setAttribute('cx', latest ? 200+latest.east*scale : 200);
    $(id).setAttribute('cy', latest ? 130-latest.north*scale : 130);
    $(id).setAttribute('r', latest ? id === 'gps-dot' ? 5 : Math.min(120,gps.current.accuracyMeters*scale) : 0);
  }
  $('location-plot').setAttribute('aria-label', `${gps.summary} GPS movement: ${gps.movement}.`);
}
function renderEventSelection() {
  const selected = (state?.events || []).find(event => event.id === $('plan-event').value);
  const tentative = selected?.status === 'tentative';
  const emailInvitation = tentative && selected?.source?.type === 'email';
  $('event-attendance').classList.toggle('hidden', !tentative);
  $('confirm-attendance').classList.toggle('hidden', !emailInvitation);
  $('confirm-attendance').disabled = !emailInvitation || $('confirm-attendance').getAttribute('aria-busy') === 'true';
  setText('attendance-help', emailInvitation ? 'This email invitation is tentative. Confirm you\'re attending before calculating or arming calls.' : tentative ? 'Confirm your attendance in Google Calendar, then sync again before calculating a morning.' : '');
  $('plan-submit').disabled = !state?.config?.routesReady || selected?.status !== 'confirmed' || $('plan-submit').getAttribute('aria-busy') === 'true';
}
function renderEvents(events) {
  const key = JSON.stringify(events);
  if (key === lastEventKey) { renderEventSelection(); return; }
  lastEventKey = key;
  const selected = $('plan-event').value;
  const available = events.filter(e => e.status !== 'cancelled').sort((a, b) => new Date(a.startAt) - new Date(b.startAt));
  $('plan-event').replaceChildren(node('option', '', available.length ? 'Choose an event' : 'Add or sync an event first'));
  $('plan-event').firstChild.value = '';
  for (const event of available) { const option = node('option', '', `${event.title}${event.source?.account ? ` · ${event.source.account}` : ''}${event.status === 'tentative' ? ' · Tentative' : ''} · ${date(event.startAt, event.timeZone)} ${time(event.startAt, event.timeZone)}`); option.value = event.id; $('plan-event').append(option); }
  if (available.some(e => e.id === selected)) $('plan-event').value = selected;
  else if (available.length === 1) $('plan-event').value = available[0].id;
  renderEventSelection();
}
function renderNextPlan(plans) {
  const next = [...plans].filter(p => new Date(p.event.startAt) > new Date()).sort((a, b) => new Date(a.event.startAt) - new Date(b.event.startAt))[0];
  $('next-plan-empty').classList.toggle('hidden', !!next); $('next-plan-content').classList.toggle('hidden', !next);
  tag('plan-tag', next ? next.armed ? 'Calls armed' : 'Review your plan' : 'No plan yet', next?.armed ? 'live' : '');
  if (!next) return;
  const zone = next.event.timeZone || localZone;
  setText('next-event-title', next.event.title); setText('next-event-location', next.event.location);
  setText('next-wake', time(next.wakeAt, zone)); setText('next-leave', time(next.leaveAt, zone)); setText('next-arrive', time(next.event.startAt, zone));
  const prepLabel=$('next-prep-label');if(prepLabel)prepLabel.textContent=next.preparationMinutes ? 'WAKE UP' : 'READY BY';
  setText('next-plan-math', `${next.preparationMinutes} min preparation · ${next.accessMinutes || 0} min to car · ${Math.ceil(next.route.durationSeconds / 60)} min travel · ${next.bufferMinutes} min arrival buffer`);
  setText('next-plan-date', `${date(next.event.startAt, zone)} · ${zone.replaceAll('_', ' ')}`);
}
function renderPlans(plans) {
  renderNextPlan(plans);
  document.querySelector('.departure-demo')?.classList.toggle('hidden',plans.some(p=>p.event.demoInvitation));
  const key = JSON.stringify([plans, state?.departures, state?.trips]);
  if (key === lastPlanKey) return; lastPlanKey = key;
  $('plans-list').replaceChildren();
  for (const plan of [...plans].sort((a, b) => new Date(a.event.startAt) - new Date(b.event.startAt))) {
    const article = node('article', 'card plan-item'); article.id=`plan-${plan.id}`; const body = node('div');
    body.append(node('h3', '', plan.event.title), node('p', '', `${date(plan.event.startAt, plan.event.timeZone)} · ${plan.event.location}`));
    if (plan.event.demoInvitation) {
      const invitation=node('details','departure-assessment');invitation.open=true;
      invitation.append(node('summary','','Staged email invitation · live phone and GPS'),node('p','',`Subject: ${plan.event.demoInvitation.subject}`));
      const message=node('p','',plan.event.demoInvitation.body);message.style.whiteSpace='pre-line';invitation.append(message,node('p','fineprint','This invitation was staged from your scenario; it was not received or sent through Gmail.'));body.append(invitation);
    }
    const times = node('div', 'plan-time-row');
    for (const [label, value] of [[plan.preparationMinutes ? 'Wake' : 'Ready by', plan.wakeAt], ['Leave', plan.leaveAt], ['Event', plan.event.startAt]]) { const part = node('span', '', `${label} `); part.append(node('strong', '', time(value, plan.event.timeZone))); times.append(part); }
    body.append(times, node('p', '', `${plan.origin} → ${plan.event.location} · ${plan.travelMode.toLowerCase()} · ${Math.ceil(plan.route.durationSeconds / 60)} min travel`), node('p', 'fineprint', `Route fetched ${date(plan.route.fetchedAt)} at ${time(plan.route.fetchedAt)} · Times in ${plan.event.timeZone}. ${plan.armed ? `Real ${(plan.reminderKinds || ['wake','leave']).join(' and ')} calls are armed.` : 'Calls are not armed.'}`));
    const assessment = (state?.departures || []).find(item => item.planId === plan.id);
    const trip = (state?.trips || []).find(item => item.planId === plan.id);
    if (trip) {
      const context = node('div','departure-assessment');
      context.append(node('span', `tag ${trip.destinationConfirmed ? 'live' : trip.status === 'still-at-origin' ? 'warning' : ''}`, trip.status.replaceAll('-',' ')), node('p','departure-reason',trip.summary), node('p','departure-evidence',`Phone ${trip.phonePosition} · Activity ${trip.activity} · GPS ${trip.gpsMovement}${trip.distanceToDestinationMeters == null ? '' : ` · ${(trip.distanceToDestinationMeters/1000).toFixed(2)} km to destination`}`));
      body.append(context);
    }
    if (plan.flatInactivity) {
      const watch=trip?.inactivity;
      const panel=node('div','departure-assessment');
      panel.append(node('span',`tag ${watch?.jobState==='sent' ? 'live' : ''}`,`${plan.flatInactivity.durationSeconds===60 ? 'Demo: 1 minute' : '5 minutes'} flat + still`));
      panel.append(node('p','departure-reason',watch?.jobState ? `Watch result: ${watch.jobState}. No automatic repeat call.` : !plan.armed ? 'Watch is disarmed.' : `${Math.min(plan.flatInactivity.durationSeconds,Math.floor(watch?.observedSeconds || 0))} / ${plan.flatInactivity.durationSeconds} seconds continuously observed.`));
      const explanations={'before-leave-time':'Waiting for the calculated leave time.','waiting-for-origin-location':'Waiting for GPS to place the phone within the starting area.','waiting-for-sensors':'Waiting for fresh motion sensors.','waiting-for-flat-inactivity':'Waiting for the full flat/still interval.','call-due':'Rule met. Calling once if the phone remains at the starting place.','already-on-way-or-near-destination':'GPS shows progress or the destination area; no call needed.','expired':'The appointment time has passed; this watch will not call.'};
      if (plan.armed && !watch?.jobState) panel.append(node('p','departure-evidence',explanations[watch?.status] || 'Checking live evidence.'));
      if (watch?.reason) panel.append(node('p','fineprint',watch.reason));
      panel.append(node('p','fineprint','Movement, tilt, a recording change, or a sensor gap resets the timer. This describes the phone, not your attention or body posture.'));
      body.append(panel);
    }
    if (plan.event.requirements?.length) body.append(node('p','',`Remember: ${plan.event.requirements.join(', ')}`));
    if (plan.route.provider === 'osrm') {
      const credit = node('p','fineprint',`OSRM driving estimate without live traffic · ${plan.accessMinutes || 0} min assumed to reach your car. `);
      for (const [label,url] of [['OpenStreetMap contributors','https://www.openstreetmap.org/copyright'],['Routing by OSRM / FOSSGIS','https://routing.openstreetmap.de/about.html'],['Fix the map','https://www.openstreetmap.org/fixthemap']]) { const link=node('a','',label); link.href=url;link.target='_blank';link.rel='noopener noreferrer';credit.append(link,document.createTextNode(' · ')); }
      body.append(credit);
    }
    if (assessment && !trip) { const departure = node('div', 'departure-assessment'); departure.append(departureView(assessment)); body.append(departure); }
    const controls = node('div', 'plan-actions'); const refresh = node('button', 'button secondary small', 'Refresh route ↻'); refresh.type = 'button'; refresh.addEventListener('click', () => action(refresh, async () => { await api(`/api/plans/${encodeURIComponent(plan.id)}/refresh`, { method: 'POST', body: '{}' }); notify('Route and morning times refreshed.'); }));
    const arm = node('button', `button small ${plan.armed ? 'secondary' : 'dark'}`, plan.armed ? 'Disarm calls' : 'Arm real calls'); arm.type = 'button'; arm.disabled = !state.config.voiceReady && !plan.armed;
    arm.addEventListener('click', () => action(arm, async () => { await api(`/api/plans/${encodeURIComponent(plan.id)}/arm`, { method: 'POST', body: JSON.stringify({ armed: !plan.armed }) }); notify(plan.armed ? 'Calls disarmed.' : 'Real wake and leave calls armed. Keep the server and motion stream running.'); }));
    const rehearsal = node('button','button secondary small','Call me now · rehearsal');rehearsal.type='button';rehearsal.disabled=!state.config.voiceReady || (state.calls || []).some(c=>activeStatuses.has(c.status));
    rehearsal.addEventListener('click',()=>action(rehearsal,async()=>{await api(`/api/plans/${encodeURIComponent(plan.id)}/call`,{method:'POST',body:JSON.stringify({rehearsal:true})});notify('Live rehearsal call requested. It does not change the scheduled departure reminder.');}));
    controls.append(refresh, arm, rehearsal);
    if (plan.originCoordinates && plan.destinationCoordinates && Date.parse(plan.event.startAt)>Date.now()) {
      for (const [label,seconds] of [['Watch · 1 min demo',60],['Watch · 5 min',300]]) {
        const watchButton=node('button','button secondary small',label);watchButton.type='button';watchButton.disabled=!state.config.voiceReady || !state.preferences.locationEnabled;
        watchButton.addEventListener('click',()=>action(watchButton,async()=>{await api(`/api/plans/${encodeURIComponent(plan.id)}/inactivity-rule`,{method:'POST',body:JSON.stringify({enabled:true,durationSeconds:seconds,armed:true})});notify(`Fresh ${seconds}-second watch armed. One departure call after leave time only if flat, still, and still at the origin.`);}));
        controls.append(watchButton);
      }
    }
    article.append(body, controls); $('plans-list').append(article);
  }
}
function renderCalls(calls) {
  const key = JSON.stringify(calls); if (key === lastCallKey) return; lastCallKey = key; $('call-list').replaceChildren();
  for (const call of [...calls].sort((a, b) => new Date(b.createdAt) - new Date(a.createdAt)).slice(0, 5)) {
    const row = node('article', 'call-item'); const content = node('div');
    const walking = call.motionEvidence?.walking;
    const physicalEvidence = walking ? `${walking.stepsSinceBaseline ?? 0}/${walking.threshold ?? 6} pedometer increments · ${walking.activityCorroborated ? 'walking Activity corroborates' : 'walking unconfirmed'}` : call.movementObserved ? 'Acceleration observed · no step evidence recorded' : 'No physical movement observed';
    const falsePositive = call.validation?.outcome === 'false-positive';
    content.append(node('h3', '', `${call.kind === 'test' ? 'Test call' : call.kind === 'wake' ? 'Wake up call' : 'Departure call'} · ${falsePositive ? 'false positive' : call.status.replaceAll('-', ' ')}`), node('p', '', `${date(call.createdAt)} at ${time(call.createdAt)} · ${call.id}`), node('p', 'call-evidence', `${physicalEvidence} · ${call.verbalConfirmed ? 'Spoken check-in confirmed' : 'Spoken check-in pending'}`));
    if (falsePositive) content.append(node('p', 'call-result', `User-reported false positive: ${call.validation.note}`));
    if (call.result) content.append(node('p', 'call-result', `${falsePositive ? 'Original app result: ' : ''}${call.result}`));
    if (call.lastSpeech) content.append(node('p', 'call-result', `Heard: "${call.lastSpeech}"`));
    if (call.tripEvidence) content.append(node('p','call-result',`Live trip check: ${call.tripEvidence.summary} Phone ${call.tripEvidence.phonePosition}; Activity ${call.tripEvidence.activity}.`));
    if (call.inactivityEvidence) content.append(node('p','call-result',`Trigger: ${Math.floor(call.inactivityEvidence.observedSeconds)} seconds flat/still before dialing; ${call.inactivityEvidence.requiredSeconds} seconds required.`));
    row.append(content);
    if (activeStatuses.has(call.status)) { const stop = node('button', 'button secondary small', 'End call'); stop.type = 'button'; stop.addEventListener('click', () => action(stop, async () => { await api(`/api/calls/${encodeURIComponent(call.id)}/stop`, { method: 'POST', body: '{}' }); notify('Call end requested.'); })); row.append(stop); }
    $('call-list').append(row);
  }
}
function renderAudit(audit) {
  const entries = [...audit].sort((a, b) => new Date(b.at) - new Date(a.at)).slice(0, 18); const key = JSON.stringify(entries); if (key === lastAuditKey) return; lastAuditKey = key;
  $('audit-list').replaceChildren();
  if (!entries.length) { $('audit-list').append(node('p', 'muted activity-empty', 'No activity yet. Real events will appear here.')); return; }
  for (const entry of entries) { const row = node('div', 'audit-item'); row.append(node('span', 'audit-time', time(entry.at)), node('span', 'audit-type', entry.type.replaceAll('.', ' / ').replaceAll('_', ' ')), node('span', 'audit-message', entry.message)); $('audit-list').append(row); }
}
async function poll() {
  if (polling || !token) return;
  polling = true;
  try { state = await api('/api/state'); $('access-panel').classList.add('hidden'); setText('connection-label', 'Console connected'); $('connection-dot').className = 'status-dot good'; renderConfig(state.config); renderMotion(state.motion); renderLocation(); renderEvents(state.events || []); renderPlans(state.plans || []); renderCalls(state.calls || []); renderAudit(state.audit || []); }
  catch (error) { setText('connection-label', 'Connection interrupted'); $('connection-dot').className = 'status-dot bad'; if (!state) notify(error.message, true); else if (state.motion) renderMotion(state.motion); }
  finally { polling = false; }
}
$('access-form').addEventListener('submit', event => { event.preventDefault(); token = $('access-token').value.trim(); sessionStorage.setItem('enro-token', token); $('access-token').value = ''; poll(); });
$('location-enabled').addEventListener('change', () => action($('location-enabled'), async () => {
  await api('/api/preferences', {method:'POST',body:JSON.stringify({...state.preferences,locationEnabled:$('location-enabled').checked})});
}));
$('place-form').addEventListener('submit', event => { event.preventDefault(); const button=event.submitter;
  action(button, async () => { await api('/api/preferences', {method:'POST',body:JSON.stringify({...state.preferences,placeAnchor:{label:$('place-label').value.trim(),latitude:Number($('place-lat').value),longitude:Number($('place-lon').value),radiusMeters:Number($('place-radius').value)}})}); notify('Place reference saved.'); });
});
for (const button of document.querySelectorAll('[data-departure-scenario]')) button.addEventListener('click', () => runDepartureDemo(button.dataset.departureScenario));
$('demo-travel').addEventListener('input', invalidateDemo);
$('demo-clock').addEventListener('input', invalidateDemo);
$('test-call').addEventListener('click', () => action($('test-call'), async () => { await api('/api/calls/test', { method: 'POST', body: '{}' }); notify('Real call requested. Watch the call status and live sensor evidence.'); }));
$('google-connect').addEventListener('click', () => action($('google-connect'), async () => { const email = $('google-account').value; const data = await api(`/api/google/connect${email ? `?email=${encodeURIComponent(email)}` : ''}`); const url = safeUrl(data.url); if (!url) throw new Error('The server did not return a valid Google authorization URL.'); window.location.assign(url); }));
$('google-sync').addEventListener('click', () => action($('google-sync'), async () => { await api('/api/google/sync', { method: 'POST', body: '{}' }); notify('Google Calendar sync completed. Review your imported events.'); }));
$('copy-sensor').addEventListener('click', () => action($('copy-sensor'), async () => { if (!state?.config.sensorPushUrl) return; try { await navigator.clipboard.writeText(state.config.sensorPushUrl); notify('Private sensor URL copied. Paste it into Sensor Logger only.'); } catch { $('sensor-url').type = 'text'; $('sensor-url').select(); notify('Select and copy the private sensor URL, then paste it into Sensor Logger.'); } }));
$('event-start').addEventListener('input', () => { const value = $('event-start').value; setText('event-time-preview', value ? `Will save ${date(value)} at ${time(value)} · ${localZone}` : 'Times use this browser\'s local time zone.'); });
$('event-form').addEventListener('submit', event => { event.preventDefault(); const form = event.currentTarget; const values = new FormData(form); action(form.querySelector('button[type=submit]'), async () => { const startAt = new Date(values.get('start')).toISOString(); await api('/api/events', { method: 'POST', body: JSON.stringify({ title: String(values.get('title')).trim(), startAt, timeZone: localZone, location: String(values.get('location')).trim(), requirements: String(values.get('requirements') || '').split(',').map(s => s.trim()).filter(Boolean) }) }); form.reset(); setText('event-time-preview', 'Times use this browser\'s local time zone.'); notify('Event saved. Select it to calculate wake and leave times.'); }); });
$('plan-event').addEventListener('change', renderEventSelection);
$('confirm-attendance').addEventListener('click', () => action($('confirm-attendance'), async () => {
  const selected = (state?.events || []).find(event => event.id === $('plan-event').value);
  if (!selected || selected.status !== 'tentative' || selected.source?.type !== 'email') throw new Error('Select a tentative email invitation to confirm.');
  await api(`/api/events/${encodeURIComponent(selected.id)}/confirm`, { method: 'POST', body: JSON.stringify({ attending: true }) });
  notify('Attendance confirmed. You can now calculate your morning.');
}));
$('import-invitations').addEventListener('click', () => action($('import-invitations'), async () => {
  const data = await api('/api/emails/import', { method: 'POST', body: '{}' });
  const count = Array.isArray(data.events) ? data.events.length : 0;
  notify(count ? `${count} calendar invitation${count === 1 ? '' : 's'} imported. Select an event and confirm attendance before planning.` : 'No structured calendar invitations found. Review recent emails to enter confirmed event details.');
}));
$('plan-form').addEventListener('submit', event => { event.preventDefault(); const values = new FormData(event.currentTarget); action($('plan-submit'), async () => { const selected = (state?.events || []).find(event => event.id === values.get('eventId')); if (selected?.status !== 'confirmed') throw new Error('Confirm attendance for this event before calculating your morning.'); await api('/api/plans', { method: 'POST', body: JSON.stringify({ eventId: values.get('eventId'), origin: String(values.get('origin')).trim(), travelMode: values.get('travelMode'), preparationMinutes: Number(values.get('preparationMinutes')), bufferMinutes: Number(values.get('bufferMinutes')) }) }); notify('Morning calculated from a real route. Review the times, then arm calls when ready.'); }); });
$('config-form').addEventListener('submit', event => { event.preventDefault(); const form = event.currentTarget; const changes = Object.fromEntries([...new FormData(form)].map(([key, value]) => [key, String(value).trim()]).filter(([, value]) => value)); action(form.querySelector('button[type=submit]'), async () => { if (!Object.keys(changes).length) throw new Error('Enter at least one changed field to save.'); await api('/api/config', { method: 'POST', body: JSON.stringify(changes) }); form.reset(); lastPlanKey = ''; notify('Configuration saved. The connection indicators show what is ready.'); }); });
$('load-emails').addEventListener('click', () => action($('load-emails'), async () => { const data = await api('/api/emails'); $('email-list').replaceChildren(); if (!data.candidates?.length) $('email-list').append(node('p', 'fineprint', 'No email candidates were returned.')); for (const email of data.candidates || []) { const item = node('article', 'email-item'); item.append(node('h3', '', email.subject || '(No subject)'), node('div', 'email-meta', `${email.account ? `${email.account} · ` : ''}${date(email.receivedAt)} · ${time(email.receivedAt)}`), node('p', '', email.body || 'No message body available.')); const url = safeUrl(email.sourceUrl); if (url) { const link = node('a', 'text-link', 'Open source email ↗'); link.href = url; link.target = '_blank'; link.rel = 'noopener noreferrer'; item.append(link); } const select = node('button', 'button small secondary', 'Use subject as event name'); select.type = 'button'; select.addEventListener('click', () => { $('event-title').value = email.subject || ''; $('event-start').focus(); $('event-form').scrollIntoView({ behavior: 'smooth', block: 'center' }); notify('Subject added. Review and enter the event\'s actual date, time, and destination.'); }); item.append(document.createElement('br'), select); $('email-list').append(item); } }));
setText('event-timezone', localZone.replaceAll('_', ' ')); renderDemoSchedule(); tickClock(); setInterval(tickClock, 10000);
async function initialize() {
  if (!token) {
    try {
      const response = await fetch('/api/bootstrap');
      if (response.ok) { const data = await response.json(); if (typeof data.token === 'string' && data.token) { token = data.token; sessionStorage.setItem('enro-token', token); } }
    } catch { /* Manual access remains available when bootstrap is unavailable. */ }
  }
  if (!token) { $('access-panel').classList.remove('hidden'); setText('connection-label', 'Access needed'); }
  await poll();
  setInterval(poll, 2000);
}
initialize();
