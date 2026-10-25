import { simulate, scenarios } from './model.js';

const $ = (id) => document.getElementById(id);
const query = new URLSearchParams(location.search);
const stageNames = { resting: 'Still lying down', preparing: 'Getting ready', departing: 'Heading out' };
let stage = Object.hasOwn(scenarios, query.get('stage')) ? query.get('stage') : 'resting';
const readAdjustment = (name, max) => Math.min(max, Math.max(0, Number(query.get(name)) || 0));
$('traffic').value = readAdjustment('traffic', 20);
$('delay').value = readAdjustment('delay', 15);
for (const mode of ['capture', 'corner', 'chroma']) {
  for (const element of [document.documentElement, document.body]) element.classList.toggle(mode, query.get(mode) === '1');
}

const media = matchMedia('(prefers-reduced-motion: reduce)');
let paused = false;
let frame = 0;
let elapsed = 0;
let lastFrame = 0;
let sequenceTimer;
let sequenceStarted = 0;
let sequenceStage = -1;
let currentForecast;

function formatTime(minutes, period = false) {
  const rounded = Math.round(minutes);
  const hour = Math.floor(rounded / 60) % 24;
  return `${hour % 12 || 12}:${String(rounded % 60).padStart(2, '0')}${period ? (hour >= 12 ? ' PM' : ' AM') : ''}`;
}

function updateLinks() {
  const settings = { stage, traffic: $('traffic').value, delay: $('delay').value };
  $('capture-link').href = `?${new URLSearchParams({ capture: '1', ...settings })}`;
  $('corner-link').href = `?${new URLSearchParams({ corner: '1', chroma: '1', ...settings })}`;
}

function updateForecast() {
  currentForecast = simulate({ stage, trafficMinutes: Number($('traffic').value), delayMinutes: Number($('delay').value) });
  const result = currentForecast;
  $('traffic-value').textContent = `${result.trafficMinutes} min`;
  $('delay-value').textContent = `${result.delayMinutes} min`;
  $('scene-label').textContent = stageNames[stage];
  document.querySelectorAll('[data-stage]').forEach((button) => {
    const selected = button.dataset.stage === stage;
    button.classList.toggle('selected', selected);
    button.setAttribute('aria-pressed', String(selected));
  });
  $('eta').textContent = formatTime(result.medianArrival);
  $('eta-period').textContent = Math.round(result.medianArrival) >= 720 ? 'PM' : 'AM';
  const delta = Math.round(result.medianArrival) - result.eventAt;
  $('timing-status').textContent = delta > 0 ? `${delta} min late` : delta < 0 ? `${-delta} min early` : 'Right on time';
  $('overlay').classList.toggle('on-time', delta <= 0);
  $('late-risk').textContent = result.pLate > .99 ? '>99%' : result.pLate < .01 ? '<1%' : `${Math.round(result.pLate * 100)}%`;
  $('sample-count').textContent = result.samples.toLocaleString('en-US');
  $('forecast-note').textContent = 'Forecast updated';

  // Every bar comes from actual samples, with a fixed axis across all scenes.
  const from = result.bins[0].from;
  const to = result.bins.at(-1).to;
  const largest = Math.max(...result.bins.map((bin) => bin.count), 1);
  const barWidth = 460 / result.bins.length;
  const deadlineX = (result.eventAt - from) / (to - from) * 460;
  const bars = result.bins.map((bin, index) => {
    const height = bin.count ? Math.max(1.5, bin.count / largest * 29) : 1;
    const fill = bin.count ? (bin.from >= result.eventAt ? '#d79179' : '#8eb3a0') : '#edf0eb';
    return `<rect x="${index * barWidth + 1}" y="${32 - height}" width="${barWidth - 3}" height="${height}" rx="1" fill="${fill}"><title>${formatTime(bin.from)} to ${formatTime(bin.to)}: ${bin.count} possible mornings</title></rect>`;
  }).join('');
  $('histogram').innerHTML = `${bars}<line x1="${deadlineX}" x2="${deadlineX}" y1="0" y2="34" stroke="#6b7e6b" stroke-width="1" stroke-dasharray="2 2"/>`;
  $('histogram').setAttribute('aria-label', `2,048 simulated arrival times. Middle 80 percent: ${formatTime(result.p10Arrival, true)} to ${formatTime(result.p90Arrival, true)}. Commitment at 9 AM.`);
  $('range-start').textContent = formatTime(from);
  $('range-end').textContent = formatTime(to);
  document.querySelector('.distribution-labels span:nth-child(2)').style.left = `${deadlineX / 4.6}%`;
  updateLinks();
  drawGraph();
}

function selectStage(nextStage, fromSequence = false) {
  if (!fromSequence) stopSequence();
  stage = nextStage;
  elapsed = 0;
  updateForecast();
}

function stopSequence() {
  clearInterval(sequenceTimer);
  sequenceTimer = undefined;
  document.body.classList.remove('sequence-active');
  $('play').innerHTML = 'Play 18s sequence <span>↗</span>';
  $('play').setAttribute('aria-pressed', 'false');
  $('sequence-state').textContent = 'READY TO REHEARSE';
}

function playSequence() {
  if (sequenceTimer) return stopSequence();
  sequenceStarted = performance.now();
  sequenceStage = -1;
  document.body.classList.add('sequence-active');
  $('play').innerHTML = 'Stop sequence <span>■</span>';
  $('play').setAttribute('aria-pressed', 'true');
  const advance = () => {
    const seconds = (performance.now() - sequenceStarted) / 1000;
    if (seconds >= 18) { stopSequence(); $('sequence-state').textContent = 'SEQUENCE COMPLETE / R TO RESET'; return; }
    const index = Math.floor(seconds / 6);
    if (index !== sequenceStage) { sequenceStage = index; selectStage(['resting', 'preparing', 'departing'][index], true); }
    $('sequence-state').textContent = `${String(Math.floor(seconds)).padStart(2, '0')} / 18 SECONDS · SCENE ${index + 1}`;
  };
  advance();
  sequenceTimer = setInterval(advance, 150);
}

function reset() {
  $('traffic').value = 0;
  $('delay').value = 0;
  selectStage('resting');
}

// Adapted visual grammar from the HealthLink lookahead graph: clustered colored
// dots, curved connections, blue active paths, and soft activity rings. These
// illustrative nodes are factors/sample paths, not separate reasoning agents.
const canvas = $('graph');
const ctx = canvas.getContext('2d');
const groups = [
  { label: 'Start delay', x: 169, y: 113, color: '#8958be', lx: 78, ly: 60 },
  { label: 'Get ready', x: 321, y: 85, color: '#315feb', lx: 382, ly: 32 },
  { label: 'Travel', x: 401, y: 198, color: '#d47930', lx: 479, ly: 165 },
  { label: 'Arrival', x: 292, y: 253, color: '#139789', lx: 354, ly: 317 },
  { label: 'Commitment', x: 145, y: 234, color: '#c45b80', lx: 67, ly: 296 },
];
let seed = 1241;
function random() { seed = Math.imul(seed, 1664525) + 1013904223 | 0; return (seed >>> 0) / 4294967296; }
const nodes = [];
const links = [];
const hubs = groups.map((group, index) => {
  const node = { ...group, group: index, radius: 6.8, phase: random() * 6, hub: true };
  nodes.push(node);
  return node;
});
const center = { x: 268, y: 174, radius: 11, color: '#223856', phase: 0, group: -1, hub: true };
nodes.push(center);
function connect(a, b, group) {
  links.push({ a, b, group, bend: (random() - .5) * 28, phase: random(), trace: random() < .28 });
}
groups.forEach((group, index) => {
  const leaves = [];
  for (let i = 0; i < 20; i += 1) {
    const angle = i * 2.399963 + index * .75;
    const radius = 19 + Math.sqrt(i / 19) * (47 + random() * 9);
    const node = { x: group.x + Math.cos(angle) * radius, y: group.y + Math.sin(angle) * radius * .79, group: index, color: group.color, radius: 2.8 + random() * .9, phase: random() * Math.PI * 2 };
    nodes.push(node); leaves.push(node); connect(hubs[index], node, index);
    if (i > 1 && i % 2 === 0) connect(leaves[i - 2], node, index);
    if (i % 5 === 0) connect(node, center, index);
    if (i % 4 === 0) connect(node, hubs[(index + 2) % hubs.length], index);
  }
  connect(hubs[index], center, index);
  connect(hubs[index], hubs[(index + 1) % hubs.length], index);
});

let canvasWidth = 1;
let canvasHeight = 1;
function resize() {
  const bounds = canvas.getBoundingClientRect();
  canvasWidth = bounds.width;
  canvasHeight = bounds.height;
  const dpr = Math.min(devicePixelRatio || 1, 2);
  canvas.width = Math.max(1, Math.round(canvasWidth * dpr));
  canvas.height = Math.max(1, Math.round(canvasHeight * dpr));
  drawGraph();
}

function drawGraph() {
  if (!ctx || !canvasWidth || !canvasHeight) return;
  const dpr = canvas.width / canvasWidth;
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, canvasWidth, canvasHeight);
  const scale = Math.min(canvasWidth / 560, canvasHeight / 348);
  ctx.translate((canvasWidth - 560 * scale) / 2, (canvasHeight - 348 * scale) / 2 - 3);
  ctx.scale(scale, scale);
  const seconds = media.matches ? 0 : elapsed / 1000;
  const stageGroup = { resting: 0, preparing: 1, departing: 2 }[stage];
  const activeGroup = media.matches || paused ? stageGroup : (stageGroup + Math.floor(seconds / 2.4)) % 4;
  const positions = new Map(nodes.map((node) => [node, {
    x: node.x + (node.hub || media.matches ? 0 : Math.sin(seconds * .45 + node.phase) * 1.5),
    y: node.y + (node.hub || media.matches ? 0 : Math.cos(seconds * .4 + node.phase) * 1.5),
  }]));
  for (const link of links) {
    const a = positions.get(link.a), b = positions.get(link.b);
    const cx = (a.x + b.x) / 2 + link.bend, cy = (a.y + b.y) / 2 - link.bend;
    const active = link.group === activeGroup;
    ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.quadraticCurveTo(cx, cy, b.x, b.y);
    ctx.strokeStyle = active ? '#315feb9c' : '#8995a35b'; ctx.lineWidth = active ? 1.4 : .9; ctx.stroke();
    if (active && link.trace && !media.matches && !paused) {
      const t = (seconds * .43 + link.phase) % 1;
      const x = (1 - t) ** 2 * a.x + 2 * (1 - t) * t * cx + t ** 2 * b.x;
      const y = (1 - t) ** 2 * a.y + 2 * (1 - t) * t * cy + t ** 2 * b.y;
      ctx.beginPath(); ctx.arc(x, y, 2, 0, Math.PI * 2); ctx.fillStyle = '#315feb'; ctx.fill();
    }
  }
  for (const node of nodes) {
    const position = positions.get(node);
    const active = node.group === activeGroup;
    if (node.hub && active) {
      const pulse = media.matches || paused ? .35 : (Math.sin(seconds * 2.2) + 1) / 2;
      ctx.beginPath(); ctx.arc(position.x, position.y, 11 + pulse * 5, 0, Math.PI * 2);
      ctx.fillStyle = `${node.color}10`; ctx.fill(); ctx.strokeStyle = `${node.color}35`; ctx.lineWidth = 1; ctx.stroke();
    }
    ctx.beginPath(); ctx.arc(position.x, position.y, node.radius, 0, Math.PI * 2);
    ctx.fillStyle = node.hub && node !== center ? '#ed805d' : node.color; ctx.fill();
    ctx.lineWidth = node.hub ? 2 : 1.5; ctx.strokeStyle = '#fff'; ctx.stroke();
  }
  ctx.textAlign = 'center'; ctx.font = '12px -apple-system, BlinkMacSystemFont, sans-serif';
  groups.forEach((group) => {
    const width = ctx.measureText(group.label).width;
    ctx.fillStyle = '#ffffffeb'; ctx.fillRect(group.lx - width / 2 - 4, group.ly - 11, width + 8, 17);
    ctx.fillStyle = '#69757a'; ctx.fillText(group.label, group.lx, group.ly + 1);
  });
  ctx.font = '9px -apple-system, BlinkMacSystemFont, sans-serif';
  ctx.fillStyle = '#fff'; ctx.fillText('you', center.x, center.y + 3);
}

function animate(time) {
  frame = 0;
  if (lastFrame) elapsed += Math.min(time - lastFrame, 64);
  lastFrame = time;
  drawGraph();
  if (!paused && !media.matches && !document.hidden) frame = requestAnimationFrame(animate);
}

function refreshMotion() {
  cancelAnimationFrame(frame); frame = 0; lastFrame = 0;
  $('motion').textContent = media.matches ? 'Motion reduced' : paused ? 'Resume motion' : 'Pause motion';
  $('motion').setAttribute('aria-pressed', String(paused || media.matches));
  $('motion').disabled = media.matches;
  drawGraph();
  if (!paused && !media.matches && !document.hidden) frame = requestAnimationFrame(animate);
}

document.querySelectorAll('[data-stage]').forEach((button) => button.addEventListener('click', () => selectStage(button.dataset.stage)));
for (const id of ['traffic', 'delay']) $(id).addEventListener('input', () => { stopSequence(); updateForecast(); });
$('play').addEventListener('click', playSequence);
$('motion').addEventListener('click', () => { paused = !paused; refreshMotion(); });
document.addEventListener('keydown', (event) => {
  if (event.altKey || event.ctrlKey || event.metaKey || event.target.closest('input, textarea, select, [contenteditable]')) return;
  if (event.repeat) return;
  const choice = { '1': 'resting', '2': 'preparing', '3': 'departing' }[event.key];
  if (choice) { event.preventDefault(); selectStage(choice); }
  if (event.code === 'Space' && !event.target.closest('button, a, summary')) { event.preventDefault(); playSequence(); }
  if (event.key.toLowerCase() === 'r') reset();
  if (event.key.toLowerCase() === 'm') { paused = !paused; refreshMotion(); }
});
media.addEventListener('change', refreshMotion);
document.addEventListener('visibilitychange', refreshMotion);
window.addEventListener('pagehide', () => { cancelAnimationFrame(frame); stopSequence(); });
window.addEventListener('pageshow', refreshMotion);
const observer = new ResizeObserver(resize);
observer.observe(canvas);
updateForecast(); resize(); refreshMotion();
if (query.get('play') === '1') playSequence();
