'use strict';
// The film page. A WebGL sheet draws the <video> and bends with scroll speed.
// Click pauses, a sideways drag scrubs, and the chapter rail seeks. Without
// WebGL, or with reduced motion, the native video stays in place.

const $ = (selector, scope = document) => scope.querySelector(selector);
const $$ = (selector, scope = document) => [...scope.querySelectorAll(selector)];
const clamp = (value, low, high) => Math.min(high, Math.max(low, value));
const clock = seconds => `${Math.floor(seconds / 60)}:${String(Math.floor(seconds % 60)).padStart(2, '0')}`;

const root = document.documentElement;
const film = $('#film');
const frame = $('.film-frame');
const controls = $('.film-controls');
const playButton = $('[data-action="play"]');
const fullButton = $('[data-action="fullscreen"]');
const nowLabel = $('[data-time="now"]');
const chapterLabel = $('[data-chapter-name]');
const chapters = $$('.chapters button').map(button => ({
  button, start: Number(button.dataset.start), end: Number(button.dataset.end), name: $('.name', button).textContent,
}));
const reducedMotion = matchMedia('(prefers-reduced-motion: reduce)');
const finePointer = matchMedia('(hover: hover) and (pointer: fine)');
const saveData = Boolean(navigator.connection?.saveData);
const motion = !reducedMotion.matches;
const duration = () => (Number.isFinite(film.duration) && film.duration > 0 ? film.duration : 56);

// Custom controls replace the native ones. The native player returns in full screen.
film.controls = false;
controls.hidden = false;
root.classList.add('film-enhanced');
if (!film.requestFullscreen && !film.webkitEnterFullscreen) fullButton.hidden = true;

function play() { film.play().catch(syncState); }
function togglePlay() { if (film.paused || film.ended) play(); else film.pause(); }
function syncState() {
  playButton.dataset.state = film.paused ? 'paused' : 'playing';
  playButton.setAttribute('aria-label', film.paused ? 'Play film' : 'Pause film');
}
function seek(seconds) {
  film.currentTime = clamp(seconds, 0, duration() - 0.04);
  renderTime(film.currentTime);
}
function step(direction) {
  const now = film.currentTime;
  const index = chapters.findIndex(chapter => now < chapter.end);
  const current = index < 0 ? chapters.length - 1 : index;
  // Back within the first moments of a chapter goes to the one before it.
  const target = direction < 0 && now - chapters[current].start > 1.5 ? current : current + direction;
  seek(chapters[clamp(target, 0, chapters.length - 1)].start);
}

film.addEventListener('play', syncState);
film.addEventListener('pause', syncState);
playButton.addEventListener('click', togglePlay);
fullButton.addEventListener('click', () => {
  if (film.requestFullscreen) film.requestFullscreen().catch(() => {});
  else film.webkitEnterFullscreen();
});
document.addEventListener('fullscreenchange', () => { film.controls = document.fullscreenElement === film; });
chapters.forEach(({ button, start }) => button.addEventListener('click', () => { seek(start); play(); }));
$$('[data-seek]').forEach(button => button.addEventListener('click', () => {
  seek(Number(button.dataset.seek));
  play();
  frame.scrollIntoView({ behavior: motion ? 'smooth' : 'auto', block: 'center' });
  frame.focus({ preventScroll: true });
}));
frame.addEventListener('keydown', event => {
  if (event.key === ' ' || event.key === 'Enter' || event.key === 'k') { event.preventDefault(); togglePlay(); }
  else if (event.key === 'ArrowRight' || event.key === 'ArrowLeft') { event.preventDefault(); step(event.key === 'ArrowRight' ? 1 : -1); }
});
reducedMotion.addEventListener('change', event => { if (event.matches) film.pause(); });

// Chapter rail and time label. Runs every frame but only writes on change.
let shownTime = -1;
let shownSecond = -1;
let activeChapter = -1;
function renderTime(now) {
  if (Math.abs(now - shownTime) < 0.008) return;
  shownTime = now;
  for (const chapter of chapters) {
    chapter.button.style.setProperty('--p', clamp((now - chapter.start) / (chapter.end - chapter.start), 0, 1).toFixed(4));
  }
  let index = chapters.findIndex(chapter => now < chapter.end);
  if (index < 0) index = chapters.length - 1;
  if (index !== activeChapter) {
    chapters[activeChapter]?.button.removeAttribute('aria-current');
    chapters[index].button.setAttribute('aria-current', 'step');
    chapterLabel.textContent = chapters[index].name;
    activeChapter = index;
  }
  const second = Math.floor(now);
  if (second !== shownSecond) { shownSecond = second; nowLabel.textContent = clock(now); }
}

// Click toggles playback. A sideways drag scrubs: the film's width spans its length.
let press = null;
let scrubTarget = null;
frame.addEventListener('pointerdown', event => {
  if (event.button !== 0) return;
  press = { id: event.pointerId, x: event.clientX, y: event.clientY, from: film.currentTime, resume: !film.paused, scrubbing: false };
  frame.setPointerCapture?.(event.pointerId);
  cursor?.press(true);
});
frame.addEventListener('pointermove', event => {
  cursor?.move(event);
  if (!press || press.id !== event.pointerId) return;
  const dx = event.clientX - press.x;
  if (!press.scrubbing) {
    if (Math.abs(dx) < 8 || Math.abs(dx) < Math.abs(event.clientY - press.y)) return;
    press.scrubbing = true;
    film.pause();
    root.classList.add('film-scrubbing');
  }
  scrubTarget = clamp(press.from + (dx / frame.getBoundingClientRect().width) * duration(), 0, duration() - 0.04);
});
function release(event, cancelled) {
  if (!press || press.id !== event.pointerId) return;
  if (press.scrubbing) {
    root.classList.remove('film-scrubbing');
    if (scrubTarget !== null) film.currentTime = scrubTarget;
    if (press.resume) play();
  } else if (!cancelled) {
    togglePlay();
    sheet?.ripple(event.clientX, event.clientY);
  }
  press = null;
  scrubTarget = null;
  cursor?.press(false);
}
frame.addEventListener('pointerup', event => release(event, false));
frame.addEventListener('pointercancel', event => release(event, true));

// The cursor shows what a click will do, and its ring shows how far the film has run.
function createCursor() {
  const element = document.createElement('div');
  element.className = 'film-cursor';
  element.setAttribute('aria-hidden', 'true');
  element.innerHTML = '<svg viewBox="0 0 100 100"><circle class="ring-track" cx="50" cy="50" r="49"/><circle class="ring-fill" cx="50" cy="50" r="49" pathLength="1"/></svg><span class="label"></span>';
  document.body.append(element);
  root.classList.add('film-cursor-on');
  const label = $('.label', element);
  const ring = $('.ring-fill', element);
  let x = 0, y = 0, targetX = 0, targetY = 0, scale = 0, inside = false, pressed = false, text = '', ringShown = -1;
  frame.addEventListener('pointerenter', event => {
    if (event.pointerType !== 'mouse') return;
    inside = true;
    targetX = event.clientX; targetY = event.clientY;
    if (scale < 0.05) { x = targetX; y = targetY; }
  });
  frame.addEventListener('pointerleave', () => { inside = false; });
  return {
    move(event) { if (event.pointerType === 'mouse') { inside = true; targetX = event.clientX; targetY = event.clientY; } },
    press(value) { pressed = value; },
    tick(now) {
      const goal = inside ? (pressed ? 0.86 : 1) : 0;
      if (Math.abs(goal - scale) < 0.002 && Math.abs(targetX - x) < 0.1 && Math.abs(targetY - y) < 0.1 && !inside) return;
      x += (targetX - x) * 0.24;
      y += (targetY - y) * 0.24;
      scale += (goal - scale) * 0.2;
      element.style.transform = `translate3d(${x.toFixed(1)}px,${y.toFixed(1)}px,0) scale(${scale.toFixed(3)})`;
      const next = root.classList.contains('film-scrubbing') ? clock(now) : film.paused ? 'Play' : 'Pause';
      if (next !== text) { text = next; label.textContent = next; }
      const progress = Math.round((now / duration()) * 1000) / 1000;
      if (progress !== ringShown) { ringShown = progress; ring.style.strokeDashoffset = String(1 - progress); }
    },
  };
}

// Section headings light up word by word as they reach reading height.
function prepareHeadings() {
  return $$('.beat h2').map(heading => {
    const words = heading.textContent.trim().split(/\s+/);
    heading.textContent = '';
    const spans = words.map((word, index) => {
      const span = document.createElement('span');
      span.className = 'w';
      span.textContent = word;
      heading.append(span, index < words.length - 1 ? ' ' : '');
      return span;
    });
    return { heading, spans, shown: -1 };
  });
}
function revealHeadings(headings) {
  const height = innerHeight;
  for (const item of headings) {
    const progress = clamp((height * 0.88 - item.heading.getBoundingClientRect().top) / (height * 0.42), 0, 1);
    if (Math.abs(progress - item.shown) < 0.002) continue;
    item.shown = progress;
    const count = item.spans.length;
    item.spans.forEach((span, index) => { span.style.opacity = (0.14 + 0.86 * clamp(progress * (count + 1) - index, 0, 1)).toFixed(3); });
  }
}

// The sheet: a canvas inside the frame, taller than it so the film can bow and cast
// a shadow. It scrolls with the page, so it never trails the layout. Scroll speed
// bows it and splits the color channels; at rest it is an exact copy of the video.
const VERTEX = `
attribute vec2 aUv;
uniform vec2 uSize;
uniform vec2 uPad;
uniform float uScale;
uniform float uBend;
varying vec2 vLocal;
void main() {
  vec2 view = uSize + 2.0 * uPad;
  vec2 local = aUv * view - uPad;
  vLocal = local;
  vec2 point = uPad + uSize * 0.5 + (local - uSize * 0.5) * uScale;
  point.y += sin(clamp(local.x / uSize.x, 0.0, 1.0) * 3.14159265) * uBend;
  vec2 clip = point / view * 2.0 - 1.0;
  gl_Position = vec4(clip.x, -clip.y, 0.0, 1.0);
}`;
const FRAGMENT = `
precision highp float;
uniform sampler2D uTexture;
uniform vec2 uSize;
uniform float uRadius;
uniform float uEdge;
uniform float uShift;
uniform vec3 uRipple;
uniform float uDim;
varying vec2 vLocal;
float box(vec2 p, vec2 b, float r) {
  vec2 q = abs(p) - b + r;
  return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
void main() {
  vec2 halfSize = uSize * 0.5;
  float shadowDistance = box(vLocal - halfSize - vec2(0.0, 26.0), halfSize - vec2(70.0, 30.0), uRadius + 20.0);
  float shadow = 0.34 * (1.0 - smoothstep(-26.0, 70.0, shadowDistance));
  vec4 color = vec4(vec3(0.086, 0.204, 0.333) * shadow, shadow);
  float coverage = clamp(0.5 - box(vLocal - halfSize, halfSize, uRadius) / uEdge, 0.0, 1.0);
  if (coverage > 0.0) {
    vec2 point = vLocal;
    if (uRipple.z >= 0.0) {
      vec2 away = point - uRipple.xy;
      float reach = length(away);
      float band = exp(-pow((reach - uRipple.z * length(uSize) * 0.8) / 48.0, 2.0));
      point += away / max(reach, 1.0) * band * (1.0 - uRipple.z) * 10.0;
    }
    vec2 uv = point / uSize;
    vec2 split = vec2(0.0, uShift / uSize.y);
    vec3 film = vec3(texture2D(uTexture, uv + split).r, texture2D(uTexture, uv).g, texture2D(uTexture, uv - split).b);
    float light = dot(film, vec3(0.299, 0.587, 0.114));
    film = mix(film, vec3(light), uDim * 0.4) * (1.0 - 0.1 * uDim);
    color = vec4(film, 1.0) * coverage + color * (1.0 - coverage);
  }
  gl_FragColor = color;
}`;

function createSheet() {
  const canvas = document.createElement('canvas');
  canvas.className = 'film-gl';
  canvas.setAttribute('aria-hidden', 'true');
  const gl = canvas.getContext('webgl', { alpha: true, premultipliedAlpha: true, antialias: true, depth: false, stencil: false });
  if (!gl) return null;
  const compile = (type, source) => {
    const shader = gl.createShader(type);
    gl.shaderSource(shader, source);
    gl.compileShader(shader);
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(shader));
    return shader;
  };
  const program = gl.createProgram();
  gl.attachShader(program, compile(gl.VERTEX_SHADER, VERTEX));
  gl.attachShader(program, compile(gl.FRAGMENT_SHADER, FRAGMENT));
  gl.linkProgram(program);
  if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(program));
  gl.useProgram(program);
  const uniform = name => gl.getUniformLocation(program, name);
  const u = Object.fromEntries(['uPad', 'uScale', 'uBend', 'uSize', 'uRadius', 'uEdge', 'uShift', 'uRipple', 'uDim', 'uTexture'].map(name => [name, uniform(name)]));

  // 64 columns keep the bow smooth. It only bends across, so 4 rows are plenty.
  const columns = 64, rows = 4, points = [], indices = [];
  for (let row = 0; row <= rows; row++) for (let column = 0; column <= columns; column++) points.push(column / columns, row / rows);
  for (let row = 0; row < rows; row++) for (let column = 0; column < columns; column++) {
    const a = row * (columns + 1) + column, b = a + 1, c = a + columns + 1, d = c + 1;
    indices.push(a, c, b, b, c, d);
  }
  gl.bindBuffer(gl.ARRAY_BUFFER, gl.createBuffer());
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(points), gl.STATIC_DRAW);
  const attribute = gl.getAttribLocation(program, 'aUv');
  gl.enableVertexAttribArray(attribute);
  gl.vertexAttribPointer(attribute, 2, gl.FLOAT, false, 0, 0);
  gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, gl.createBuffer());
  gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(indices), gl.STATIC_DRAW);

  const texture = gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, texture);
  for (const [key, value] of [[gl.TEXTURE_MIN_FILTER, gl.LINEAR], [gl.TEXTURE_MAG_FILTER, gl.LINEAR], [gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE], [gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE]]) gl.texParameteri(gl.TEXTURE_2D, key, value);
  gl.uniform1i(u.uTexture, 0);
  gl.enable(gl.BLEND);
  gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
  gl.clearColor(0, 0, 0, 0);
  frame.append(canvas);

  const pad = 96;
  let ratio = 1, width = 0, height = 0, radius = 16, ready = false, fresh = true, drawn = '', dim = film.paused ? 1 : 0, ripple = null, lost = false;
  const resize = () => {
    ratio = Math.min(devicePixelRatio || 1, 2);
    width = frame.clientWidth; height = frame.clientHeight;
    canvas.width = Math.round(width * ratio); canvas.height = Math.round((height + 2 * pad) * ratio);
    canvas.style.top = `${-pad}px`; canvas.style.width = `${width}px`; canvas.style.height = `${height + 2 * pad}px`;
    gl.viewport(0, 0, canvas.width, canvas.height);
    radius = parseFloat(getComputedStyle(frame).borderTopLeftRadius) || 16;
    drawn = '';
  };
  resize();
  new ResizeObserver(resize).observe(frame);
  addEventListener('resize', resize);
  // Upload a frame only when the video presents a new one.
  const markFresh = () => { fresh = true; };
  if ('requestVideoFrameCallback' in film) {
    const onFrame = () => { fresh = true; film.requestVideoFrameCallback(onFrame); };
    film.requestVideoFrameCallback(onFrame);
  }
  ['loadeddata', 'seeked', 'playing'].forEach(type => film.addEventListener(type, markFresh));
  canvas.addEventListener('webglcontextlost', event => {
    event.preventDefault();
    lost = true;
    root.classList.remove('gl-ready');
    canvas.remove();
  });

  return {
    get ready() { return ready && !lost; },
    refresh() { fresh = true; drawn = ''; },
    ripple(clientX, clientY) {
      if (!motion) return;
      const rect = frame.getBoundingClientRect();
      ripple = { x: clientX - rect.left, y: clientY - rect.top, start: performance.now() };
    },
    draw(now, velocity) {
      if (lost) return;
      const rect = frame.getBoundingClientRect();
      // Well off screen, skip the work but keep the last frame, so a fast scroll back never shows a blank sheet.
      const margin = innerHeight * 0.5;
      if (rect.bottom + margin < 0 || rect.top - margin > innerHeight) return;
      if (!('requestVideoFrameCallback' in film) && !film.paused) fresh = true;
      if (fresh && film.readyState >= 2) {
        gl.bindTexture(gl.TEXTURE_2D, texture);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, film);
        fresh = false;
        drawn = '';
        if (!ready) { ready = true; root.classList.add('gl-ready'); }
      }
      if (!ready) return;
      dim += ((film.paused ? 1 : 0) - dim) * 0.12;
      if (Math.abs(dim - (film.paused ? 1 : 0)) < 0.002) dim = film.paused ? 1 : 0;
      const rippleAge = ripple ? (now - ripple.start) / 900 : -1;
      if (rippleAge >= 1) ripple = null;
      // The film grows into place as it rises toward the middle of the screen.
      const rise = clamp((innerHeight - (rect.top + rect.height / 2)) / (innerHeight * 0.5), 0, 1);
      const scale = 0.92 + 0.08 * (1 - (1 - rise) ** 3);
      const bend = clamp(velocity * 1.35, -42, 42);
      const shift = clamp(velocity * 0.17, -7, 7);
      const key = [width, height, scale.toFixed(4), bend.toFixed(2), shift.toFixed(2), dim.toFixed(3), rippleAge.toFixed(3)].join();
      if (key === drawn) return;
      drawn = key;
      gl.clear(gl.COLOR_BUFFER_BIT);
      gl.uniform2f(u.uPad, 0, pad);
      gl.uniform1f(u.uScale, scale);
      gl.uniform1f(u.uBend, bend);
      gl.uniform2f(u.uSize, width, height);
      gl.uniform1f(u.uRadius, radius);
      gl.uniform1f(u.uEdge, 1 / (ratio * scale));
      gl.uniform1f(u.uShift, shift);
      gl.uniform3f(u.uRipple, ripple?.x ?? 0, ripple?.y ?? 0, ripple ? rippleAge : -1);
      gl.uniform1f(u.uDim, dim);
      gl.drawElements(gl.TRIANGLES, indices.length, gl.UNSIGNED_SHORT, 0);
    },
  };
}

let sheet = null;
let cursor = null;
let headings = [];
if (motion) {
  try { sheet = createSheet(); } catch { sheet = null; }
  if (finePointer.matches) cursor = createCursor();
  headings = prepareHeadings();
}

// One loop drives the sheet, the cursor, the rail, and the headings.
let lastScroll = scrollY;
let lastFrame = performance.now();
let velocity = 0;
let layoutChanged = true;
addEventListener('resize', () => { layoutChanged = true; });
function tick(now) {
  requestAnimationFrame(tick);
  const elapsed = clamp(now - lastFrame, 1, 64);
  lastFrame = now;
  const moved = scrollY - lastScroll;
  lastScroll = scrollY;
  // Scroll speed in pixels per 60 Hz frame, eased so the sheet settles in about half a second.
  velocity += (moved * (16.7 / elapsed) - velocity) * (1 - 0.84 ** (elapsed / 16.7));
  if (Math.abs(velocity) < 0.02) velocity = 0;
  if (scrubTarget !== null && !film.seeking && Math.abs(film.currentTime - scrubTarget) > 0.03) film.currentTime = scrubTarget;
  const time = scrubTarget ?? film.currentTime;
  renderTime(time);
  cursor?.tick(time);
  if (headings.length && (moved || layoutChanged)) revealHeadings(headings);
  layoutChanged = false;
  sheet?.draw(now, velocity);
}
requestAnimationFrame(tick);

// Hand the frame back to the <video> for page snapshots, so the transition to the
// landing page and the back/forward cache both show the current frame.
addEventListener('pageswap', () => root.classList.remove('gl-ready'));
addEventListener('pagehide', () => root.classList.remove('gl-ready'));
addEventListener('pageshow', () => {
  if (!sheet?.ready) return;
  sheet.refresh();
  root.classList.add('gl-ready');
});

syncState();
renderTime(0);
if (motion && !saveData) {
  film.preload = 'auto';
  play();
}
