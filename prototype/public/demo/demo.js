import { evaluateScene, formatTime, scenes } from './stage-model.js';

const $ = id => document.getElementById(id);
const text = (id, value) => { $(id).textContent = value; };
const titleCase = value => String(value || 'unknown').replaceAll('-', ' ').replace(/^./, c => c.toUpperCase());
const publicDemo = document.documentElement.dataset.publicDemo === 'true' || !['localhost', '127.0.0.1', '[::1]'].includes(location.hostname);
let token = sessionStorage.getItem('enro-token') || '';
let liveState = null;
let scene = 'stationary';
let checkedIn = false;
let playback = null;
let polling = false;
let assessmentVersion = 0;
let activeView = 'story';

async function api(path, options = {}) {
  const response = await fetch(path, { ...options, headers: { Authorization: `Bearer ${token}`, ...(options.body ? { 'Content-Type': 'application/json' } : {}) }, signal: AbortSignal.timeout(6500) });
  const result = await response.json().catch(() => ({}));
  if (!response.ok) throw new Error(response.status === 401 ? 'Open the console to reconnect this browser.' : 'The local service is unavailable.');
  return result;
}

function showView(view) {
  activeView = ['story', 'planner', 'evidence', 'lookahead'].includes(view) ? view : 'story';
  if (activeView !== 'story') stopPlayback();
  for (const element of document.querySelectorAll('.view')) element.hidden = element.id !== `view-${activeView}`;
  for (const button of document.querySelectorAll('[data-view]')) {
    if (button.dataset.view === activeView) button.setAttribute('aria-current', 'page'); else button.removeAttribute('aria-current');
  }
  const frame = $(`${activeView}-frame`);
  if (frame && !frame.getAttribute('src')) frame.setAttribute('src', frame.dataset.src);
  if (location.hash !== `#${activeView}`) history.replaceState(null, '', `#${activeView}`);
  window.scrollTo({ top: 0, behavior: 'instant' });
}
for (const button of document.querySelectorAll('[data-view]')) button.addEventListener('click', () => showView(button.dataset.view));
for (const link of document.querySelectorAll('[data-open]')) link.addEventListener('click', event => { event.preventDefault(); showView(link.dataset.open); });
window.addEventListener('hashchange', () => showView(location.hash.slice(1)));

function values() {
  return { scene, checkedIn, clock: Number($('clock').value), travel: Number($('travel').value), prep: Number($('prep').value), buffer: Number($('buffer').value) };
}

function renderScenario() {
  for (const id of ['travel', 'prep', 'buffer']) if (!$(id).validity.valid || !$(id).value) return;
  const input = values();
  const result = evaluateScene(input);
  for (const button of document.querySelectorAll('[data-scene]')) button.setAttribute('aria-pressed', String(button.dataset.scene === scene));
  document.querySelector('.forecast').dataset.scene = scene;
  text('wake-time', formatTime(result.wakeAt)); text('leave-time', formatTime(result.leaveAt)); text('stage-clock', formatTime(result.now)); text('phone-clock', formatTime(result.now).replace(' PM', ''));
  const arrival = formatTime(result.arrivalAt).split(' ');
  $('arrival-time').replaceChildren(document.createTextNode(arrival[0]), Object.assign(document.createElement('span'), { textContent: arrival[1] }));
  text('arrival-status', result.lateMinutes ? `${result.lateMinutes} min late` : result.earlyMinutes ? `${result.earlyMinutes} min early` : 'At event start');
  $('arrival-status').classList.toggle('early', !result.lateMinutes);
  text('forecast-reason', result.lateMinutes ? 'Waiting to leave puts the appointment at risk.' : result.bufferShortfall ? `The event is still reachable; ${result.bufferShortfall} minutes of the arrival buffer are lost.` : 'The selected assumptions leave time before the appointment.');
  text('forecast-math', `${formatTime(result.now)} + ${result.signal.wait} min delay + ${result.remainingPrep} min remaining prep + ${input.travel} min travel`);
  text('demo-counts', `${result.signal.counts} increments`); text('demo-activity', titleCase(result.signal.activity)); text('demo-position', titleCase(result.signal.position)); text('demo-departure', result.signal.outside ? 'Outside starting area' : 'Unconfirmed'); text('demo-decision', result.decision);
  let line = `Your Price Center appointment is at 6:30. Your planned leave time is ${formatTime(result.leaveAt)}. Let\'s check how you\'re doing.`;
  let callState = result.now >= result.leaveAt ? 'Time to head out.' : 'A little time to get ready.';
  if (scene === 'pickup') line = 'The phone changed position. That tells me it was handled; walking and departure are still unconfirmed.';
  if (scene === 'shaking') line = 'The counter reports seven increments, but Activity says stationary. Shaking can fool the pedometer. Walking is unconfirmed.';
  if (scene === 'walking') line = checkedIn ? 'The staged counts and walking Activity agree, and I heard your check in. Ending this preview.' : 'The staged counts and walking Activity agree. Say "I am up" to complete this preview.';
  if (scene === 'departed') { line = 'The staged GPS fixes place the phone outside Crossroads. They do not establish its route or arrival at Price Center.'; callState = 'Outside the starting area.'; }
  if (checkedIn && !result.confirmed && scene !== 'departed') line = scene === 'shaking' ? 'I heard your check in. The counter rose, but walking Activity has not corroborated it. This preview stays open.' : 'I heard your check in. I still need both new counter increments and walking Activity.';
  if (result.confirmed) callState = 'Preview check complete.';
  text('agent-line', line); text('call-state', callState);
  text('check-in', checkedIn ? 'Reset spoken check in ↺' : 'Say "I am up" ↗');
  text('scenario-note', publicDemo ? 'All signals above are staged. The revised walking check still needs physical validation. See Test evidence for the physical trial.' : 'All signals above are staged. The revised walking check still needs physical validation. Live phone readings stay in Live evidence.');
  return result;
}

async function assessWithPrototype() {
  if (publicDemo) return;
  const version = ++assessmentVersion;
  const input = values();
  if (!token || input.buffer !== 10 || !['travel', 'prep', 'buffer'].every(id => $(id).validity.valid && $(id).value)) return;
  try {
    const result = await api('/api/demo/departure', { method: 'POST', body: JSON.stringify({ scenario: scene === 'departed' ? 'departed-gps' : scene, travelMinutes: input.travel, clockMinutes: input.clock }) });
    if (version !== assessmentVersion) return;
    text('scenario-note', `Prototype departure assessment: ${String(result.assessment.status).replaceAll('-', ' ')}. Staged sensor inputs; live recordings and schedules are unchanged. This endpoint uses its fixed 10 minute arrival buffer.`);
  } catch { /* Local forecast remains clearly labeled if the prototype is unavailable. */ }
}

function stopPlayback() { clearInterval(playback); playback = null; text('play', 'Play the story ▶'); }
function chooseScene(next, fromPlayback = false) { if (!fromPlayback) stopPlayback(); scene = next; checkedIn = false; renderScenario(); assessWithPrototype(); }
for (const button of document.querySelectorAll('[data-scene]')) button.addEventListener('click', () => chooseScene(button.dataset.scene));
for (const id of ['travel', 'prep', 'buffer', 'clock']) $(id).addEventListener('input', () => { stopPlayback(); renderScenario(); assessmentVersion++; });
for (const id of ['travel', 'clock']) $(id).addEventListener('change', assessWithPrototype);
$('check-in').addEventListener('click', () => { stopPlayback(); checkedIn = !checkedIn; renderScenario(); });
$('play').addEventListener('click', () => {
  if (playback) { stopPlayback(); return; }
  const sequence = ['stationary', 'pickup', 'shaking', 'walking', 'departed']; let index = 0;
  chooseScene(sequence[0], true); text('play', 'Pause the story Ⅱ');
  playback = setInterval(() => {
    index++;
    if (index >= sequence.length) { stopPlayback(); return; }
    chooseScene(sequence[index], true);
    if (scene === 'shaking' || scene === 'walking') { checkedIn = true; renderScenario(); }
  }, 4000);
});

function node(tag, className, content) { const element = document.createElement(tag); element.className = className; if (content !== undefined) element.textContent = content; return element; }
function renderLive(state) {
  liveState = state;
  const config = state.config || {};
  const motion = state.motion || {};
  const live = Object.values(motion.sensors || {}).some(sensor => sensor.status === 'live');
  const accountCount = (config.googleAccounts || []).filter(account => account.connected).length;
  text('google-status', config.googleConnected ? `${accountCount || 1} account${accountCount === 1 ? '' : 's'} connected` : 'Not connected');
  text('sensor-status', live ? 'Receiving live data' : 'Waiting for stream');
  text('route-status', config.routesReady ? 'Key configured' : 'Manual demo timings');
  text('connection-note', config.routesReady ? 'Connections reflect the local prototype.' : 'Live routes await Maps setup. Scenario travel is assumed.');
  document.querySelector('.live-dot').classList.toggle('connected', live);
  text('live-stream', live ? 'Streaming live' : titleCase(motion.status)); text('live-samples', Number(motion.sampleCount || 0).toLocaleString()); text('live-latency', motion.latencyMs === null || motion.latencyMs === undefined ? ' · ' : `${Math.round(motion.latencyMs)} ms`);
  text('live-pedometer', motion.walking?.status === 'unavailable' ? 'No recent counter update' : `${motion.walking?.stepsSinceBaseline ?? 0} increments · ${titleCase(motion.walking?.status)}`);
  text('live-activity', motion.activity?.status === 'available' ? `Phone reports ${motion.activity.classification}` : titleCase(motion.activity?.status));
  text('live-pose', titleCase(state.pose?.phonePosition?.position || (motion.gravity?.pickupPossible ? 'Possible pickup' : 'unknown')));
  text('live-location', state.location?.summary || (state.preferences?.locationEnabled ? 'Waiting for usable GPS' : 'Collection off'));
  $('call-records').replaceChildren();
  const calls = (state.calls || []).slice(0, 3);
  if (!calls.length) $('call-records').append(node('p', 'muted', 'No real call has been recorded. Scenario previews do not create call history.'));
  for (const call of calls) {
    const falsePositive = call.validation?.outcome === 'false-positive';
    const record = node('article', `record${falsePositive ? ' false-positive' : ''}`);
    record.append(node('h3', '', falsePositive ? 'Walking trial · false positive' : `Real call · ${titleCase(call.status)}`));
    record.append(node('small', '', new Intl.DateTimeFormat('en-US', { hour: 'numeric', minute: '2-digit', timeZone: 'America/Los_Angeles' }).format(new Date(call.createdAt)) + ' Pacific'));
    if (falsePositive) record.append(node('p', '', 'The user shook the phone seven times without walking. The previous counter and voice rule ended incorrectly.'));
    else record.append(node('p', '', call.result === 'movement-and-voice-confirmed' ? 'Call transport and spoken response worked. Acceleration did not validate walking.' : call.result ? String(call.result).replaceAll('-', ' ') : 'Carrier outcome is still pending.'));
    $('call-records').append(record);
  }
  text('updated-at', `Live status updated ${new Intl.DateTimeFormat('en-US', { hour: 'numeric', minute: '2-digit', second: '2-digit' }).format(new Date())}`);
}

async function poll() {
  if (publicDemo) return;
  if (polling || document.hidden) return;
  polling = true;
  try { renderLive(await api('/api/state')); }
  catch (error) { text('connection-note', error.message); text('sensor-status', 'Connection unavailable'); text('updated-at', 'Live status unavailable · scenario controls still work'); document.querySelector('.live-dot').classList.remove('connected'); }
  finally { polling = false; }
}

function renderPublicDemo() {
  text('google-status', 'Sample events');
  text('sensor-status', 'Staged signals');
  text('route-status', 'Manual assumptions');
  text('connection-note', 'Public demo. Accounts and phone controls run locally.');
  document.querySelector('.source-label').lastChild.textContent = ' DEMO INPUTS';
  const consoleLink = document.querySelector('.operator-link');
  consoleLink.href = '/dashboard'; consoleLink.textContent = 'Team dashboard ↗';
  const planner = $('view-planner');
  planner.querySelector('.eyebrow').textContent = 'TRY A SAMPLE DAY';
  planner.querySelector('.intro').textContent = 'Choose an event, plan your travel, and preview a wake up call.';
  planner.querySelector('.pane-note').textContent = 'Sample events and simulated calls. This public demo does not connect accounts or place calls.';
  planner.querySelector('.outline-link').href = './day/';
  planner.querySelector('.outline-link').textContent = 'Open sample planner ↗';
  $('planner-frame').dataset.src = './day/';
  $('planner-frame').title = 'Sample Enro morning planner';
  const evidence = $('view-evidence');
  evidence.querySelector('h1').innerHTML = 'The test,<br>and what we learned.';
  evidence.querySelector('.intro').textContent = 'What the physical test established, and what still needs testing.';
  evidence.querySelector('.outline-link').href = 'https://github.com/migingyn/enro/blob/main/prototype/docs/PHONE_TEST.md';
  evidence.querySelector('.outline-link').textContent = 'Read the test record ↗';
  text('live-stream', 'Local only'); text('live-samples', 'Private'); text('live-latency', 'Not shared');
  text('live-pedometer', 'Not streaming to this page'); text('live-activity', 'Not streaming to this page');
  text('live-pose', 'Device position only'); text('live-location', 'Private');
  evidence.querySelectorAll('.card-label span').forEach(element => { if (element.textContent === 'LIVE STORED RESULTS') element.textContent = 'DOCUMENTED TEST'; });
  $('call-records').replaceChildren();
  const record = node('article', 'record false-positive');
  record.append(node('h3', '', 'Seven shakes exposed a false positive'));
  record.append(node('small', '', 'October 24, 2026 · Physical phone test'));
  record.append(node('p', '', 'The user moved the phone up and down seven times without walking, then said "I am up." The old counter and voice rule ended the call incorrectly.'));
  record.append(node('p', '', 'The revised experiment also requires fresh walking or running Activity. It still needs a physical walking versus shaking trial.'));
  $('call-records').append(record);
  const team = evidence.querySelector('.team-panel');
  team.querySelector('.muted').textContent = 'The C++ team build and TypeScript phone prototype are preserved in one repository. Their schedulers and call state remain separate. This demo brings their views together.';
  const localLink = team.querySelector('a[href="/console.html"]');
  localLink.href = 'https://github.com/migingyn/enro/tree/main/prototype'; localLink.textContent = 'Prototype source ↗';
  text('updated-at', 'Public demo · sample data');
  const next = document.querySelector('[data-open="planner"]');
  next.querySelector('span').textContent = 'Try a sample commitment';
  document.querySelector('[data-open="evidence"] b').textContent = 'Phone test evidence →';
  document.querySelector('[data-view="evidence"]').textContent = 'Test evidence';
}

async function initialize() {
  if (publicDemo) { renderPublicDemo(); showView(location.hash.slice(1)); renderScenario(); return; }
  showView(location.hash.slice(1)); renderScenario();
  try {
    const response = await fetch('/api/bootstrap', { signal: AbortSignal.timeout(3500) });
    if (response.ok) { const data = await response.json(); if (typeof data.token === 'string') { token = data.token; sessionStorage.setItem('enro-token', token); } }
  } catch { /* The local scenario works without a live connection. */ }
  if (token) await poll(); else { text('google-status', 'Open on localhost'); text('sensor-status', 'Not connected'); text('route-status', 'Manual demo timings'); text('connection-note', 'The illustrative scenario works offline. Open the console for live access.'); }
  setInterval(poll, 3000);
}
document.addEventListener('visibilitychange', () => { if (!document.hidden) poll(); });
window.addEventListener('pagehide', stopPlayback);
initialize();
