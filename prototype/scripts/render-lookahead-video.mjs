// Offline vector rendering of the standalone, explicitly staged Lookahead demo.
// CANVAS_MODULES points to a node_modules directory containing @napi-rs/canvas.
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { mkdir, writeFile } from 'node:fs/promises';
import { spawn } from 'node:child_process';
import { once } from 'node:events';
import { simulate } from '../public/demo/lookahead/model.js';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const require = createRequire(process.env.CANVAS_MODULES ? path.join(process.env.CANVAS_MODULES, '_render.cjs') : import.meta.url);
const { createCanvas, GlobalFonts } = require('@napi-rs/canvas');
for (const suffix of [3, 4, 5, 6, 7]) GlobalFonts.registerFromPath(path.join(root, 'public/fonts', `inter-${suffix}.ttf`), 'Inter');

const WIDTH = 900, HEIGHT = 1020, FPS = 60, DURATION = 21;
const out = path.resolve(process.env.VIDEO_OUTPUT || path.join(root, 'output/lookahead-video'));
await mkdir(out, { recursive: true });
const canvas = createCanvas(WIDTH, HEIGHT);
const ctx = canvas.getContext('2d');
const colors = { ink: '#243733', muted: '#7c877f', blue: '#315feb', coral: '#ed805d', teal: '#139789' };
const scenes = [
  { id: 'resting', title: 'Still lying down', signals: ['ACCEL  still', 'GYRO  steady', 'STEPS  0', 'GPS  home'], active: 0 },
  { id: 'preparing', title: 'Getting ready', signals: ['ACCEL  active', 'GYRO  turning', 'STEPS  12', 'GPS  home'], active: 1 },
  { id: 'departing', title: 'Heading out', signals: ['ACCEL  moving', 'GYRO  upright', 'STEPS  48', 'GPS  en route'], active: 2 },
].map((scene) => ({ ...scene, result: simulate({ stage: scene.id }) }));

let seed = 1241;
const random = () => { seed = Math.imul(seed, 1664525) + 1013904223 | 0; return (seed >>> 0) / 4294967296; };
const groups = [
  { label: 'Start delay', x: 169, y: 113, color: '#8958be', lx: 78, ly: 60 },
  { label: 'Get ready', x: 321, y: 85, color: '#315feb', lx: 382, ly: 32 },
  { label: 'Travel', x: 401, y: 198, color: '#d47930', lx: 479, ly: 165 },
  { label: 'Arrival', x: 292, y: 253, color: '#139789', lx: 354, ly: 317 },
  { label: 'Commitment', x: 145, y: 234, color: '#c45b80', lx: 67, ly: 296 },
];
const nodes = [], links = [];
const hubs = groups.map((group, index) => {
  const node = { ...group, group: index, radius: 6.8, phase: random() * 6, hub: true };
  nodes.push(node); return node;
});
const center = { x: 268, y: 174, radius: 11, color: '#223856', phase: 0, group: -1, hub: true };
nodes.push(center);
function connect(a, b, group, trunk = false) {
  links.push({ a, b, group, bend: (random() - .5) * 28, phase: random(), trunk });
}
groups.forEach((group, index) => {
  const leaves = [];
  for (let i = 0; i < 20; i += 1) {
    const angle = i * 2.399963 + index * .75;
    const radius = 19 + Math.sqrt(i / 19) * (47 + random() * 9);
    const node = { x: group.x + Math.cos(angle) * radius, y: group.y + Math.sin(angle) * radius * .79, group: index, color: group.color, radius: 2.8 + random() * .9, phase: random() * Math.PI * 2 };
    nodes.push(node); leaves.push(node); connect(node, hubs[index], index);
    if (i > 1 && i % 2 === 0) connect(leaves[i - 2], node, index);
    if (i % 5 === 0) connect(node, center, index);
    if (i % 4 === 0) connect(node, hubs[(index + 2) % hubs.length], index);
  }
  connect(hubs[index], center, index, true);
  connect(hubs[index], hubs[(index + 1) % hubs.length], index, true);
});

const circle = (x, y, radius, fill) => { ctx.beginPath(); ctx.arc(x, y, radius, 0, Math.PI * 2); ctx.fillStyle = fill; ctx.fill(); };
function rounded(x, y, w, h, radius, fill) { ctx.beginPath(); ctx.roundRect(x, y, w, h, radius); ctx.fillStyle = fill; ctx.fill(); }
function text(value, x, y, size = 12, color = colors.ink, weight = 400, align = 'left') {
  ctx.font = `${weight} ${size}px Inter`; ctx.textAlign = align; ctx.fillStyle = color; ctx.fillText(value, x, y);
}
function tracked(value, x, y, size, tracking, color = colors.muted) {
  ctx.font = `500 ${size}px Inter`; ctx.fillStyle = color; ctx.textAlign = 'left';
  for (const letter of value) { ctx.fillText(letter, x, y); x += ctx.measureText(letter).width + tracking; }
}
function rule(y, color = '#edf0eb') { ctx.beginPath(); ctx.moveTo(9, y); ctx.lineTo(591, y); ctx.lineWidth = 1; ctx.strokeStyle = color; ctx.stroke(); }
function time(minutes) { const n = Math.round(minutes); return `${Math.floor(n / 60) % 12 || 12}:${String(n % 60).padStart(2, '0')}`; }
function point(a, b, c, u) { return { x: (1-u)**2*a.x + 2*(1-u)*u*c.x + u*u*b.x, y: (1-u)**2*a.y + 2*(1-u)*u*c.y + u*u*b.y }; }

function graph(seconds, scene) {
  ctx.save(); ctx.translate(20, 133);
  // Fixed layout makes the moving impulses readable at corner-video scale.
  const positions = new Map(nodes.map((node) => [node, {
    x: node.x + (node.hub ? 0 : Math.sin(seconds * .45 + node.phase) * 1.4),
    y: node.y + (node.hub ? 0 : Math.cos(seconds * .4 + node.phase) * 1.4),
  }]));
  const wave = (scene.active + Math.floor(seconds / 1.4)) % 5;
  const impacts = [];
  const paths = [];
  links.forEach((link, index) => {
    const a = positions.get(link.a), b = positions.get(link.b);
    const c = { x: (a.x + b.x) / 2 + link.bend, y: (a.y + b.y) / 2 - link.bend };
    const active = link.group === wave || link.trunk;
    ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.quadraticCurveTo(c.x, c.y, b.x, b.y);
    ctx.lineWidth = active ? 1.2 : .9; ctx.strokeStyle = active ? '#315feb62' : '#8995a35c'; ctx.stroke();
    if (!active || (!link.trunk && index % 3 !== 0)) return;
    const period = link.trunk ? .9 : 1.15;
    const clock = (seconds + link.phase * period) % period;
    const duration = link.trunk ? .32 : .23;
    if (clock < duration) paths.push({ a, b, c, u: clock / duration, trunk: link.trunk });
    else if (clock < duration + .28) impacts.push({ ...b, age: (clock - duration) / .28 });
  });
  // A bullet has a bright head and a short tapered tail, not a glowing edge.
  for (const pulse of paths) {
    for (let trail = 8; trail >= 1; trail--) {
      const u = Math.max(0, pulse.u - trail * .018);
      const p = point(pulse.a, pulse.b, pulse.c, u);
      circle(p.x, p.y, (pulse.trunk ? 2.4 : 1.9) * (1 - trail / 13), `rgba(49,95,235,${(1 - trail / 10) * .75})`);
    }
    const p = point(pulse.a, pulse.b, pulse.c, pulse.u);
    circle(p.x, p.y, pulse.trunk ? 5.2 : 4, '#315feb14');
    circle(p.x, p.y, pulse.trunk ? 2.9 : 2.4, '#315feb');
    circle(p.x-.25, p.y-.25, .9, '#eaf6ff');
  }
  for (const impact of impacts) {
    ctx.beginPath(); ctx.arc(impact.x, impact.y, 4 + impact.age * 10, 0, Math.PI * 2);
    ctx.lineWidth = 1.2; ctx.strokeStyle = `rgba(49,95,235,${(1 - impact.age) * .5})`; ctx.stroke();
  }
  for (const node of nodes) {
    const p = positions.get(node);
    if (node.hub && node.group === wave) {
      const radius = 13 + (Math.sin(seconds * 3) + 1) * 2;
      circle(p.x, p.y, radius, '#315feb0b');
      ctx.strokeStyle = '#315feb30'; ctx.lineWidth = 1; ctx.stroke();
    }
    circle(p.x, p.y, node.radius, node.hub && node !== center ? colors.coral : node.color);
    ctx.lineWidth = node.hub ? 1.7 : 1.2; ctx.strokeStyle = '#fff'; ctx.stroke();
  }
  for (const group of groups) {
    ctx.font = '12px Inter'; const width = ctx.measureText(group.label).width;
    rounded(group.lx - width/2 - 4, group.ly - 11, width + 8, 17, 4, '#fffffff0');
    text(group.label, group.lx, group.ly+1, 12, '#6f7a7c', 400, 'center');
  }
  text('you', center.x, center.y + 3, 8.5, '#fff', 500, 'center');
  ctx.restore();
}

function draw(seconds) {
  ctx.resetTransform(); ctx.clearRect(0, 0, WIDTH, HEIGHT); ctx.scale(1.5, 1.5);
  const index = Math.min(2, Math.floor(seconds / 7));
  const scene = scenes[index], result = scene.result;
  rounded(8, 8, 584, 664, 17, '#fff');
  ctx.save(); ctx.beginPath(); ctx.roundRect(8, 8, 584, 664, 17); ctx.clip();
  text('enro', 36, 53, 34, colors.ink, 700);
  ctx.beginPath(); ctx.moveTo(116, 32); ctx.lineTo(116, 52); ctx.strokeStyle = '#dbe1d8'; ctx.lineWidth=1; ctx.stroke();
  tracked('LOOKAHEAD', 133, 47, 10, 2.6, '#7b877f');
  circle(460, 43, 2.4, '#97a680'); text('Demo scenario', 471, 47, 10, '#788478');
  rule(76);
  text(scene.title, 36, 111, 17, '#34453d', 600);
  tracked('COMMITMENT', 479, 100, 7.4, .8);
  text('9:00 AM', 564, 115, 11, '#58665e', 500, 'right');
  // Sensor values are scripted scene cues, never read from private/live state.
  scene.signals.forEach((signal, i) => {
    circle(39 + i * 136, 132, 2, groups[i].color);
    text(signal, 46 + i * 136, 135, 8.6, '#849087', 500);
  });
  for (let x=22;x<582;x+=15) for(let y=154;y<465;y+=15) circle(x,y,.65,'#e0e8ee');
  graph(seconds, scene);
  text('Simulated signals  ·  Reference time 8:05 AM', 300, 477, 9.5, '#8a958d', 400, 'center');

  tracked('PREDICTED ARRIVAL', 36, 512, 9.3, 1.4, '#7f8b81');
  text(time(result.medianArrival), 35, 566, 58, colors.ink, 400);
  ctx.font='400 58px Inter'; const etaWidth=ctx.measureText(time(result.medianArrival)).width;
  text('AM', 42+etaWidth, 565, 12, '#859083');
  const delta = Math.round(result.medianArrival) - result.eventAt;
  const timing = delta > 0 ? `${delta} min late` : `${-delta} min early`;
  const tone = delta > 0 ? '#b46f50' : '#3f7b65';
  ctx.font='500 14px Inter'; const badgeWidth=ctx.measureText(timing).width + 19;
  rounded(564-badgeWidth, 512, badgeWidth, 29, 5, delta>0?'#fbefe5':'#edf5ee');
  text(timing, 554, 531, 14, tone, 500, 'right');
  const risk=result.pLate>.99?'>99%':result.pLate<.01?'<1%':`${Math.round(result.pLate*100)}%`;
  text(`${risk} chance of being late`, 564, 561, 10, '#7d8981', 400, 'right');

  const histX=36,histW=528,histY=611,histH=29;
  const largest=Math.max(...result.bins.map(b=>b.count));
  result.bins.forEach((bin,i)=>{
    const height=bin.count?Math.max(1,bin.count/largest*histH):.8;
    const fill=bin.count?(bin.from>=result.eventAt?'#d3997f':'#99b7a5'):'#edf0eb';
    rounded(histX+i*histW/result.bins.length+1,histY-height,histW/result.bins.length-3,height,1,fill);
  });
  const deadlineX=histX+(540-500)/(615-500)*histW;
  ctx.beginPath();ctx.setLineDash([1.5,2]);ctx.moveTo(deadlineX,578);ctx.lineTo(deadlineX,615);ctx.strokeStyle='#8b9b8c';ctx.lineWidth=1;ctx.stroke();ctx.setLineDash([]);
  text('8:20',histX,625,8.4,'#929e94');
  text('9:00 commitment',deadlineX,625,8.4,'#89988b',400,'center');
  text('10:15',histX+histW,625,8.4,'#929e94',400,'right');
  ctx.fillStyle='#fbfcf9';ctx.fillRect(8,638,584,34);rule(638);
  circle(38,655,2.4,'#9dab86');text('2,048 sampled mornings',47,658,9,'#7d8c7c');
  text(`Scenario 0${index+1} / 03`,564,658,9,'#7d8c7c',400,'right');
  ctx.restore();
}

// Preview without spinning up an encoder allows visual review before a long run.
if (process.argv.includes('--preview')) {
  for (const [name, seconds] of [['resting', 2.15], ['preparing', 9.15], ['departing', 16.15]]) {
    draw(seconds); await writeFile(path.join(out, `preview-${name}.png`), await canvas.encode('png'));
  }
  console.log(`Preview frames saved to ${out}`);
} else {
  const cardFile=path.join(out,'enro-lookahead-card.mp4');
  const rightFile=path.join(out,'enro-lookahead-right-half-green.mp4');
  const filters='[0:v]split=2[card][plate];color=c=0xeef0e7:s=900x1020:r=60[paper];[paper][card]overlay=0:0:shortest=1:format=auto,format=yuv420p[main];color=c=0x00ff00:s=1920x1080:r=60[green];[green][plate]overlay=990:30:shortest=1:format=auto,format=yuv420p[right]';
  const encode=['-an','-c:v','libx264','-preset','fast','-crf','17','-pix_fmt','yuv420p','-movflags','+faststart','-color_primaries','bt709','-color_trc','bt709','-colorspace','bt709'];
  const ffmpeg=spawn('ffmpeg',['-hide_banner','-loglevel','error','-y','-f','rawvideo','-pixel_format','rgba','-video_size',`${WIDTH}x${HEIGHT}`,'-framerate',String(FPS),'-i','pipe:0','-filter_complex',filters,'-map','[main]',...encode,cardFile,'-map','[right]',...encode,rightFile],{stdio:['pipe','ignore','pipe']});
  let errors='';ffmpeg.stderr.on('data',chunk=>{errors+=chunk;});
  const finished=once(ffmpeg,'close');
  // Forward broken-pipe errors with encoder diagnostics rather than hanging.
  ffmpeg.stdin.on('error',error=>console.error(`Encoder input: ${error.message}`));
  for(let frame=0;frame<FPS*DURATION;frame++) {
    draw(frame/FPS);
    const rgba=ctx.getImageData(0,0,WIDTH,HEIGHT).data;
    if(!ffmpeg.stdin.write(Buffer.from(rgba.buffer,rgba.byteOffset,rgba.byteLength))) await once(ffmpeg.stdin,'drain');
    if(frame%(FPS*3)===0)console.log(`Rendered ${frame/FPS}s / ${DURATION}s`);
  }
  ffmpeg.stdin.end();
  const [code]=await finished;if(code!==0)throw new Error(`ffmpeg exited ${code}: ${errors}`);
  const manifest={durationSeconds:DURATION,fps:FPS,card:{path:cardFile,width:WIDTH,height:HEIGHT},rightHalf:{path:rightFile,width:1920,height:1080,x:990,y:30,background:'#00ff00'},scenarios:scenes.map(s=>({name:s.title,medianArrival:time(s.result.medianArrival),pLate:s.result.pLate})),inputs:'Staged sensor scenarios; local 2,048-sample model, no live sensor access.'};
  await writeFile(path.join(out,'export.json'),JSON.stringify(manifest,null,2)+'\n');
  console.log(JSON.stringify(manifest,null,2));
}
