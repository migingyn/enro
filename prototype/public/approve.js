import { upcomingEvents, isTravelEvent, calculateSelected } from './event-planning.js';

const $ = id => document.getElementById(id);
const zone = Intl.DateTimeFormat().resolvedOptions().timeZone;
const selected = new Set();
let state, token = sessionStorage.getItem('enro-token') || '', busy = false;
const node = (tag, className, text) => { const el = document.createElement(tag); el.className = className; if (text !== undefined) el.textContent = text; return el; };
const format = (value, options = {}, timeZone = zone) => new Intl.DateTimeFormat(undefined, { ...options, timeZone }).format(new Date(value));
const time = (value, timeZone) => format(value, { hour: 'numeric', minute: '2-digit' }, timeZone);

function notice(message, error = false) {
  $('notice').textContent = message;
  $('notice').className = `app-notice${error ? ' error' : ''}`;
  $('notice').hidden = !message;
}
async function api(path, options = {}) {
  const response = await fetch(path, { ...options, headers: { Authorization: `Bearer ${token}`, ...(options.body ? { 'Content-Type': 'application/json' } : {}) } });
  const data = await response.json().catch(() => ({}));
  if (!response.ok) throw new Error(response.status === 401 ? 'Open the console to reconnect this browser, then return here.' : typeof data.error === 'string' ? data.error : `Request failed (${response.status}). Please try again.`);
  return data;
}
function refreshButtons() {
  const events = state ? upcomingEvents(state.events).filter(isTravelEvent) : [];
  $('continue').disabled = busy || !selected.size || !state?.config.routesReady;
  $('sync').disabled = busy || !state?.config.googleConnected;
  $('approve-all').disabled = busy || !events.length;
  $('approved-count').textContent = selected.size;
  for (const button of document.querySelectorAll('.account-row button')) button.disabled = busy || !state?.config.googleReady;
  for (const button of document.querySelectorAll('.actions button, #event-form button')) button.disabled = busy || button.dataset.travelDisabled === 'true';
  for (const input of document.querySelectorAll('#travel-form input')) input.disabled = busy;
}
async function run(task) {
  if (busy) return;
  busy = true; refreshButtons();
  try { await task(); } catch (error) { notice(error.message || 'Something went wrong. Please try again.', true); }
  finally { busy = false; refreshButtons(); }
}
function renderAccounts() {
  const connected = (state.config.googleAccounts || []).filter(account => account.connected);
  $('account-status').textContent = connected.length ? `${connected.length} Google account${connected.length === 1 ? '' : 's'} connected` : 'Connect your accounts';
  $('account-dot').classList.toggle('live', connected.length > 0);
  const emails = [...new Set([...(state.preferences?.googleAccounts || []), ...(state.config.googleAccounts || []).map(account => account.email)])];
  $('account-list').replaceChildren();
  for (const email of emails) {
    const row = node('div', 'account-row'); row.append(node('span', 'account-email', email));
    if (connected.some(account => account.email === email)) row.append(node('span', 'account-label', 'Connected'));
    else {
      const button = node('button', 'linkish', 'Connect'); button.type = 'button';
      button.disabled = !state.config.googleReady;
      button.addEventListener('click', () => run(async () => {
        const data = await api(`/api/google/connect?email=${encodeURIComponent(email)}`);
        const url = new URL(data.url);
        if (url.protocol !== 'https:' || url.hostname !== 'accounts.google.com') throw new Error('Google sign-in is unavailable. Open Connections in the console.');
        location.assign(url.href);
      }));
      row.append(button);
    }
    $('account-list').append(row);
  }
  if (!emails.length) $('account-list').append(node('p', 'small-copy', 'Add a Google account in the console to bring in your calendar and invitations.'));
}
function renderEvents() {
  const events = upcomingEvents(state.events);
  for (const id of selected) if (!events.some(event => event.id === id && isTravelEvent(event))) selected.delete(id);
  $('events').replaceChildren();
  $('found').textContent = `${events.length} upcoming event${events.length === 1 ? '' : 's'}`;
  $('empty').hidden = events.length > 0;
  for (const event of events) {
    const online = !isTravelEvent(event);
    const item = node('li', `event${selected.has(event.id) ? ' approved' : ''}`);
    const when = node('div', 'when');
    when.append(node('div', 'day', format(event.startAt, { weekday: 'short', month: 'short' }, event.timeZone)), node('div', 'date', format(event.startAt, { day: 'numeric' }, event.timeZone)), node('div', 'time', time(event.startAt, event.timeZone)));
    const info = node('div', 'info'); info.append(node('h2', '', event.title), node('p', 'place', event.location));
    const chips = node('div', 'plan');
    chips.append(node('span', 'chip', event.source.type === 'email' ? 'Email invitation' : event.source.type === 'calendar' ? 'Calendar' : 'Added by you'));
    if (online) chips.append(node('span', 'chip', 'Online · no travel'));
    if (event.status === 'tentative') chips.append(node('span', 'chip', 'Attendance needs confirmation'));
    info.append(chips, node('p', 'event-source', `${event.source.account ? `${event.source.account} · ` : ''}${event.timeZone.replaceAll('_', ' ')}`));
    if (event.travel?.destination && event.travel.destination !== event.location) info.append(node('p', 'event-source', `Route to building: ${event.travel.destination}`));
    const controls = node('div', 'actions');
    const button = node('button', 'btn approve'); button.type = 'button'; button.setAttribute('aria-label', `Select ${event.title}`); button.setAttribute('aria-pressed', String(selected.has(event.id)));
    button.dataset.travelDisabled = String(online); button.disabled = busy || online;
    const icon = node('span', '', online ? ' · ' : selected.has(event.id) ? '✓' : '+'); icon.setAttribute('aria-hidden', 'true'); button.append(icon, node('span', 'lbl', online ? 'Online' : selected.has(event.id) ? 'Selected' : 'Select'));
    button.addEventListener('click', () => { selected.has(event.id) ? selected.delete(event.id) : selected.add(event.id); renderEvents(); refreshButtons(); $('events').querySelectorAll('.approve')[events.indexOf(event)]?.focus(); });
    controls.append(button); item.append(when, info, controls); $('events').append(item);
  }
}
async function load(initial = false) {
  state = await api('/api/state');
  if (initial) {
    $('origin').value = state.preferences?.origin || '';
    $('preparation').value = state.preferences?.preparationMinutes ?? 45;
    $('buffer').value = state.preferences?.bufferMinutes ?? 10;
    const mode = state.preferences?.travelModes?.[0] === 'WALK' ? 'WALK' : 'DRIVE';
    document.querySelector(`#travel-form input[value="${mode}"]`).checked = true;
  }
  renderAccounts(); renderEvents();
  $('route-status').textContent = state.config.routesReady ? 'Times use a live travel estimate. Calculating an email invitation confirms you\'re attending. Review the plan before enabling calls.' : 'Travel estimates are waiting for Google Maps setup. You can add and review events now.';
  refreshButtons();
}
function renderResults(plans) {
  $('results').hidden = !plans.length; $('result-list').replaceChildren();
  for (const plan of plans) {
    const item = node('article', 'event'); item.append(node('h3', '', plan.event.title));
    const chips = node('div', 'plan');
    chips.append(node('span', 'chip', `Wake ${time(plan.wakeAt, plan.event.timeZone)}`), node('span', 'chip leave', `Leave ${time(plan.leaveAt, plan.event.timeZone)}`), node('span', 'chip', `Arrive by ${time(new Date(Date.parse(plan.event.startAt) - plan.bufferMinutes * 60_000), plan.event.timeZone)}`));
    item.append(chips, node('p', 'small-copy', `${format(plan.event.startAt, { month: 'short', day: 'numeric' }, plan.event.timeZone)} · ${Math.ceil(plan.route.durationSeconds / 60)} min ${plan.travelMode === 'WALK' ? 'walk' : 'drive'} · ${plan.event.timeZone.replaceAll('_', ' ')}. ${plan.armed ? 'Calls already enabled for this plan.' : 'Calls are off until you enable them.'}`));
    $('result-list').append(item);
  }
}
$('approve-all').addEventListener('click', () => { for (const event of upcomingEvents(state.events).filter(isTravelEvent)) selected.add(event.id); renderEvents(); refreshButtons(); });
$('sync').addEventListener('click', () => run(async () => { notice('Refreshing your accounts…'); await api('/api/google/sync', { method: 'POST', body: '{}' }); await load(); notice('Events refreshed. Only upcoming events with a time and place appear here.'); }));
$('event-form').addEventListener('submit', event => {
  event.preventDefault(); const form = event.currentTarget; const values = new FormData(form);
  run(async () => {
    const startAt = new Date(values.get('start'));
    if (!Number.isFinite(startAt.getTime()) || startAt <= new Date()) throw new Error('Choose a future date and time.');
    const added = await api('/api/events', { method: 'POST', body: JSON.stringify({ title: String(values.get('title')).trim(), startAt: startAt.toISOString(), timeZone: zone, location: String(values.get('location')).trim(), requirements: [] }) });
    selected.add(added.id); form.reset(); await load(); notice('Event added. Review your travel settings to calculate times.'); $('add-event').open = false;
  });
});
$('travel-form').addEventListener('submit', event => {
  event.preventDefault(); const values = new FormData(event.currentTarget);
  const settings = { origin: String(values.get('origin')).trim(), travelMode: values.get('travelMode'), preparationMinutes: Number(values.get('preparationMinutes')), bufferMinutes: Number(values.get('bufferMinutes')) };
  run(async () => {
    if (!selected.size) return;
    const latest = await api('/api/state');
    if (!latest.config.routesReady) throw new Error('Finish Google Maps setup before calculating travel times.');
    notice('Calculating wake and leave times…');
    const { results, errors } = await calculateSelected({ ids: [...selected], state: latest, settings, api });
    renderResults(results); await load();
    notice(errors.length ? errors.map(error => error.message).join(' ') : `${results.length} plan${results.length === 1 ? '' : 's'} ready to review.`, errors.length > 0);
    if (results.length) $('results').scrollIntoView({ behavior: matchMedia('(prefers-reduced-motion: reduce)').matches ? 'instant' : 'smooth', block: 'center' });
  });
});
$('time-zone').textContent = `Times entered in ${zone.replaceAll('_', ' ')}.`;
async function initialize() {
  try {
    const response = await fetch('/api/bootstrap');
    if (response.ok) { const data = await response.json(); if (typeof data.token === 'string') { token = data.token; sessionStorage.setItem('enro-token', token); } }
    if (!token) throw new Error('Open the console to connect this browser first.');
    await load(true);
  } catch (error) { $('account-status').textContent = 'Connection unavailable'; $('account-list').replaceChildren(node('p', 'small-copy', 'Open the console to check your connection.')); $('found').textContent = 'Events unavailable'; notice(error.message, true); }
}
initialize();
