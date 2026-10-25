'use strict';
const events = [
  { title: 'Design team standup', place: 'Downtown studio', source: 'Sample Calendar event', start: 540 },
  { title: 'Coffee with Maya', place: 'Neighborhood café', source: 'Sample Gmail invitation', start: 600 },
];
const modes = { drive: { label: 'Drive', minutes: 25 }, transit: { label: 'Transit', minutes: 40 }, walk: { label: 'Walk', minutes: 50 } };
let step = 0, selected = 0, mode = 'drive', prep = 35, answered = false, awake = false, traffic = false, finished = false;
const screen = document.querySelector('#screen');
const time = minutes => `${Math.floor(minutes / 60) % 12 || 12}:${String(minutes % 60).padStart(2, '0')} AM`;
const event = () => events[selected];
const travel = () => modes[mode].minutes;
const leave = () => event().start - 10 - travel();
const wake = () => leave() - prep;
const title = (label, heading, description) => `<p class="step-label">${label}</p><h2 tabindex="-1">${heading}</h2><p class="description">${description}</p>`;
const back = () => '<button class="text-button back" data-action="back">← Back</button>';
const timeline = () => `<div class="timeline" aria-live="polite"><div><span>Wake up call<small>${prep} minutes to get ready</small></span><strong>${time(wake())}</strong></div><div><span>Head out<small>${travel()} minutes by ${mode === 'transit' ? 'transit' : mode === 'walk' ? 'foot' : 'car'}</small></span><strong>${time(leave())}</strong></div><div><span>Arrive<small>10 minutes to settle in</small></span><strong>${time(event().start - 10)}</strong></div><div><span>${event().title}</span><strong>${time(event().start)}</strong></div></div>`;
function render(focus = true) {
  document.querySelectorAll('.steps li').forEach((item, index) => {
    if (index === step) item.setAttribute('aria-current', 'step'); else item.removeAttribute('aria-current');
    item.classList.toggle('done', index < step);
  });
  if (step === 0) {
    screen.innerHTML = title('01 / YOUR DAY', 'What\'s on your morning?', 'Enro brings your commitments together. Choose a sample event to plan around.') +
      `<div class="events">${events.map((item, index) => `<button class="event" data-event="${index}" aria-pressed="${selected === index}"><time>${time(item.start).replace(' AM', '')}<small>AM</small></time><span><strong>${item.title}</strong><small>${item.place}<br>${item.source}</small></span><span class="choice" aria-hidden="true"></span></button>`).join('')}</div><button class="primary" data-action="next">Plan this morning <span aria-hidden="true">→</span></button><p class="tiny">In the product, you choose which calendar events and invitations to include.</p>`;
  } else if (step === 1) {
    screen.innerHTML = title('02 / YOUR PLAN', 'Work backward. Arrive early.', `Your ${event().title.toLowerCase()} starts at ${time(event().start)}. Try changing how you get there or how long you need.`) +
      `<fieldset><legend>How are you getting there?</legend><div class="modes">${Object.entries(modes).map(([key, value]) => `<button class="mode" data-mode="${key}" aria-pressed="${mode === key}">${value.label}</button>`).join('')}</div></fieldset><label class="range-label" for="prep">Time to get ready <output id="prep-value" for="prep">${prep} min</output></label><input id="prep" type="range" min="15" max="60" step="5" value="${prep}"><div id="plan">${timeline()}</div><button class="primary" data-action="next">Try my wake up call <span aria-hidden="true">→</span></button><p class="tiny">Travel times are sample estimates for this demo.</p>${back()}`;
  } else if (step === 2) {
    screen.innerHTML = title('03 / YOUR WAKE UP', 'A call with a reason to get up.', 'Enro ties your wake up to what\'s ahead. Try the call, then move the sample morning forward.') +
      `<div class="phone"><div class="phone-time">${time(wake())} · DEMO CALL</div><div class="call-icon"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" aria-hidden="true"><path d="M8 3H4c-1 0-1 1-1 2 0 8 8 16 16 16 1 0 2 0 2-1v-4l-5-2-2 2c-3-1-5-3-6-6l2-2-2-5Z"/></svg></div><h3>${answered ? 'Good morning.' : 'enro is calling'}</h3>${answered ? `<blockquote>"Your ${event().title.toLowerCase()} is at ${time(event().start)}. You have ${prep} minutes to get ready. Let\'s head out by ${time(leave())}."</blockquote>` : '<p>Your morning, with a little backup.</p><button class="primary" data-action="answer">Answer demo call</button>'}</div>` +
      (awake ? '<div class="status">Demo check in complete. Your sample day is moving.</div><button class="primary" data-action="next">Continue to departure →</button>' : answered ? '<button class="primary" data-action="awake">Simulate getting up & checking in →</button>' : '') +
      '<p class="tiny">Simulated call and activity. No call is placed or movement measured.</p>' + back();
  } else if (finished) {
    const arrival = leave() - 10 + travel() + (traffic ? 10 : 0);
    screen.innerHTML = '<div class="success-mark" aria-hidden="true">✓</div>' + title('YOUR SAMPLE MORNING / COMPLETE', 'You\'re on your way.', `You\'re heading to ${event().place.toLowerCase()}, with an estimated arrival of ${time(arrival)}  ·  ${event().start - arrival} minutes before your commitment.`) +
      `<div class="timeline"><div><span>Your commitment</span><strong>${time(event().start)}</strong></div><div><span>Your wake up</span><strong>${time(wake())}</strong></div><div><span>Your departure</span><strong>${time(leave() - 10)}</strong></div><div><span>Your arrival</span><strong>${time(arrival)}</strong></div></div><p class="description">That\'s Enro: a plan around your day, a call to get you started, and a nudge when it\'s time to leave.</p><button class="primary" data-action="restart">Try another morning ↺</button><a class="secondary" href="/">Back to enro →</a>`;
  } else {
    const extra = traffic ? 10 : 0;
    const cause = mode === 'drive' ? 'traffic' : mode === 'transit' ? 'a transit delay' : 'a route detour';
    screen.innerHTML = title('04 / YOUR DEPARTURE', 'A nudge before you\'re late.', `It\'s ${time(leave() - 10)} in your sample morning. See how a change to your journey changes your leave time.`) +
      `<div class="departure" aria-live="polite"><small>${traffic ? 'UPDATED DEPARTURE' : 'LEAVE BY'}</small><strong>${time(leave() - extra)}</strong><p>${traffic ? 'Time to head out now.' : 'You have 10 minutes before it\'s time to go.'}<br>${travel() + extra} minute journey · Arrive by ${time(event().start - 10)}</p></div><button class="secondary" data-action="traffic" aria-pressed="${traffic}">${traffic ? 'Remove the delay' : `Add 10 minutes of ${mode === 'drive' ? 'traffic' : 'travel delay'}`}</button><p class="traffic" role="status">${traffic ? `With ${cause}, Enro moves your departure 10 minutes earlier to keep your arrival buffer.` : 'Plans can change. Try a delay to see your departure update.'}</p><button class="primary" data-action="finish">I\'m heading out →</button><p class="tiny">This update is a demo scenario, not live traffic.</p>${back()}`;
  }
  if (focus) screen.querySelector('h2').focus({ preventScroll: true });
}
function restart() { step = 0; selected = 0; mode = 'drive'; prep = 35; answered = false; awake = false; traffic = false; finished = false; render(); }
screen.addEventListener('click', e => {
  const button = e.target.closest('button');
  if (!button) return;
  if (button.dataset.event !== undefined) { selected = Number(button.dataset.event); render(false); screen.querySelector(`[data-event="${selected}"]`).focus(); return; }
  if (button.dataset.mode) { mode = button.dataset.mode; render(false); screen.querySelector(`[data-mode="${mode}"]`).focus(); return; }
  const action = button.dataset.action;
  if (action === 'next') step = Math.min(3, step + 1);
  if (action === 'back') { step = Math.max(0, step - 1); if (step === 1) { answered = false; awake = false; traffic = false; } }
  if (action === 'answer') answered = true;
  if (action === 'awake') awake = true;
  if (action === 'traffic') traffic = !traffic;
  if (action === 'finish') finished = true;
  if (action === 'restart') { restart(); return; }
  render();
});
screen.addEventListener('input', e => {
  if (e.target.id !== 'prep') return;
  prep = Number(e.target.value);
  document.querySelector('#prep-value').textContent = `${prep} min`;
  document.querySelector('#plan').innerHTML = timeline();
});
document.querySelector('#restart').addEventListener('click', restart);
render(false);
