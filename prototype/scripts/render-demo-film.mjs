// Original vector film with explicit sample inputs. No live account or sensor data.
// CANVAS_MODULES may point to a node_modules directory with @napi-rs/canvas 0.1.100.
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { mkdir, writeFile } from 'node:fs/promises';
import { spawn } from 'node:child_process';
import { once } from 'node:events';
import { evaluateScene, formatTime } from '../public/demo/stage-model.js';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const require = createRequire(process.env.CANVAS_MODULES ? path.join(process.env.CANVAS_MODULES, '_render.cjs') : import.meta.url);
const { createCanvas, GlobalFonts } = require('@napi-rs/canvas');
for (const n of [3,4,5,6,7]) GlobalFonts.registerFromPath(path.join(root, `public/fonts/inter-${n}.ttf`), 'Inter');
GlobalFonts.registerFromPath(path.join(root, 'public/fonts/archivo-0.ttf'), 'Archivo');
const W=1600,H=900,FPS=30,DURATION=56;
const out=path.join(root,'public/demo/film');
const preview=path.join(root,'../.data/film-review');
await mkdir(out,{recursive:true}); await mkdir(preview,{recursive:true});
const canvas=createCanvas(W,H),c=canvas.getContext('2d');
const C={ink:'#edf6ff',muted:'#9bb1c8',blue:'#96c9ff',green:'#a9d9bc',amber:'#edc394',line:'#b8d5f025',panel:'#d5e7fc0a'};
const starts=[0,6,14,23,32,40,48], ends=[6,14,23,32,40,48,56];
const titles=['THE MOMENT BEFORE','01 / THE COMMITMENT','02 / THE PLAN','03 / THE CALL','04 / THE CHECK','05 / THE RESPONSE','06 / THE DEPARTURE'];
const captions=[
  'Your calendar knows when. Enro works out when to go.',
  'A changed appointment should change the plan around it.',
  'Travel, a little breathing room, and time to get ready.',
  'When it is time to leave, the call explains why.',
  'A pickup, seven counts, and a reply do not establish walking.',
  'Fresh counts, walking Activity, and a spoken check in.',
  'Leaving the starting area is a separate signal. Arrival is still unconfirmed.',
];
const plan=(eventAt)=>({eventAt,travel:20,buffer:10,prep:45,arriveAt:eventAt-10,leaveAt:eventAt-30,wakeAt:eventAt-75});
const original=plan(18*60+30),updated=plan(18*60+20);
const shaken=evaluateScene({scene:'shaking',checkedIn:true});
const walked=evaluateScene({scene:'walking',checkedIn:true});
if(shaken.confirmed || !walked.confirmed)throw new Error('Film scenes disagree with the demo evidence rule.');
const clamp=x=>Math.max(0,Math.min(1,x));
const ease=x=>1-(1-clamp(x))**3;
const mix=(a,b,t)=>a+(b-a)*clamp(t);
function rr(x,y,w,h,r,fill,stroke){c.beginPath();c.roundRect(x,y,w,h,r);if(fill){c.fillStyle=fill;c.fill()}if(stroke){c.strokeStyle=stroke;c.lineWidth=1;c.stroke()}}
function dot(x,y,r,fill){c.beginPath();c.arc(x,y,r,0,Math.PI*2);c.fillStyle=fill;c.fill()}
function line(x,y,x2,y2,color=C.line,width=1){c.beginPath();c.moveTo(x,y);c.lineTo(x2,y2);c.strokeStyle=color;c.lineWidth=width;c.stroke()}
function text(s,x,y,size=24,color=C.ink,weight=400,align='left',family='Inter'){c.font=`${weight} ${size}px ${family}`;c.textAlign=align;c.fillStyle=color;c.fillText(s,x,y)}
function lines(arr,x,y,size=24,gap=35,color=C.ink,weight=400){arr.forEach((s,i)=>text(s,x,y+i*gap,size,color,weight))}
function label(s,x,y,color=C.muted){c.font='600 13px Inter';c.fillStyle=color;c.textAlign='left';for(const ch of s){c.fillText(ch,x,y);x+=c.measureText(ch).width+1.9}}
function chip(s,x,y,color=C.blue){c.font='500 16px Inter';let w=c.measureText(s).width+28;rr(x,y-24,w,36,18,color+'18');text(s,x+14,y,16,color,500);return w}
function enter(u,delay,fn){let k=ease((u-delay)/.8);if(k<=0)return;c.save();c.globalAlpha*=k;c.translate(0,(1-k)*22);fn();c.restore()}
function route(x,y,w,h,progress=0){
  c.save();c.translate(x,y);
  for(let a=0;a<w;a+=60)line(a,0,a,h,'#c8def50c');
  for(let b=0;b<h;b+=60)line(0,b,w,b,'#c8def50c');
  c.beginPath();c.moveTo(35,h-45);c.bezierCurveTo(w*.3,h*.78,w*.18,h*.21,w*.57,h*.35);c.bezierCurveTo(w*.81,h*.45,w*.73,42,w-38,42);c.strokeStyle='#83b8eb40';c.lineWidth=3;c.stroke();
  // Fixed curve; these coordinates are illustrative, not a street map.
  const samples=[];for(let j=0;j<=120;j++){let t=j/120;let px=35+(w-73)*t,py=h-45-(h-87)*(t*.8+.2*Math.sin(t*Math.PI/2));samples.push([px,py])}
  if(progress>0){c.beginPath();c.moveTo(...samples[0]);samples.slice(1,Math.ceil(clamp(progress)*120)).forEach(p=>c.lineTo(...p));c.strokeStyle=C.green;c.lineWidth=3;c.stroke();let p=samples[Math.floor(clamp(progress)*119)];dot(...p,13,'#a9d9bc20');dot(...p,5,C.green)}
  dot(35,h-45,8,C.blue);dot(w-38,42,8,C.green);c.restore();
}
function phone(x,y,scale=1,angle=0,body){
  c.save();c.translate(x,y);c.rotate(angle);c.scale(scale,scale);
  c.shadowColor='#00000050';c.shadowBlur=42;c.shadowOffsetY=20;
  rr(-156,-292,312,584,48,'#071421','#88a6c050');c.shadowBlur=0;c.shadowOffsetY=0;
  rr(-146,-282,292,564,40,'#1b3148');rr(-45,-266,90,18,10,'#0b1929');
  text('5:50',-122,-247,12,C.ink,600);dot(114,-251,3,C.ink);dot(102,-251,3,C.ink);
  if(body)body();rr(-48,266,96,4,2,'#d4e8fa70');c.restore();
}
function voiceWave(x,y,t){for(let i=0;i<31;i++){let h=5+22*Math.abs(Math.sin(t*4+i*.75))*Math.sin((i+1)/32*Math.PI);rr(x+i*6,y-h/2,3,h,2,C.blue)}}
function panel(x,y,w,h){rr(x,y,w,h,20,C.panel,C.line)}
function bg(t){
  const g=c.createLinearGradient(0,0,W,H);g.addColorStop(0,'#182d45');g.addColorStop(.55,'#102237');g.addColorStop(1,'#0b1a2d');c.fillStyle=g;c.fillRect(0,0,W,H);
  const g2=c.createRadialGradient(1180,280,0,1180,280,680);g2.addColorStop(0,'#739bc91e');g2.addColorStop(1,'#739bc900');c.fillStyle=g2;c.fillRect(0,0,W,H);
  c.save();c.translate(1200,450);c.rotate(t*.013);for(let r of [260,400,560]){c.beginPath();c.ellipse(0,0,r,r*.65,-.6,0,Math.PI*2);c.strokeStyle='#c8def50c';c.lineWidth=1;c.stroke()}c.restore();
  text('enro',64,65,36,C.ink,900,'left','Archivo');dot(1220,53,4,C.blue);label('SCRIPTED WALKTHROUGH',1234,58);line(64,97,1536,97);
  line(64,833,1536,833);c.fillStyle=C.blue;c.fillRect(64,832,(1536-64)*t/DURATION,2);
  text('Sample data · No live calls',64,871,15,C.muted);text('SIC / Invisible interfaces',1536,871,15,C.muted,400,'right');
}
function scene0(u,t){
  enter(u,0,()=>{label('A LITTLE AHEAD',90,212);lines(['En route.','On time.'],86,324,96,105,C.ink,900);});
  enter(u,.45,()=>lines(['The right nudge,','before the rush.'],92,590,28,40,C.muted));
  enter(u,.35,()=>{
    route(860,225,600,420,clamp(u/7)*.4);
    chip('Price Center · 6:20 PM',1140,235,C.green);chip('La Jolla Crossroads',852,680);
    phone(1090,452,.64,Math.sin(t*.5)*.03,()=>{text('enro',0,-100,43,C.ink,900,'center','Archivo');text('Time to head out.',0,-56,18,C.muted,400,'center');voiceWave(-91,15,t);rr(-118,67,236,93,14,'#ffffff08');lines(['Your next commitment,','with a little time to spare.'],-98,102,15,24,C.ink);});
  });
}
function scene1(u){
  label(titles[1],90,165);lines(['Plans change.','Enro follows.'],88,250,66,74,C.ink,700);
  enter(u,.1,()=>{panel(90,435,620,260);label('CALENDAR / EVENT',122,480);text('Price Center appointment',122,535,29,C.ink,600);text('6:30 PM',122,615,55,C.blue,500);text('UC San Diego',670,653,16,C.muted,400,'right');});
  enter(u,1.1,()=>{panel(845,222,655,306);label('EMAIL / SCHEDULE CHANGE',880,270);text('A small change for tonight',880,321,28,C.ink,600);lines(['“Can we meet at 6:20 instead?”','Same place. Ten minutes earlier.'],880,380,24,40,C.muted);chip('Matched to the appointment',880,482,C.green);});
  enter(u,3,()=>{line(710,565,845,565,C.blue,2);dot(710+135*((u-3)%2)/2,565,5,C.blue);panel(845,558,655,137);label('UPDATED EVENT TIME',880,604);text('6:20 PM',1464,653,43,C.green,500,'right');});
}
function scene2(u){
  label(titles[2],90,165);text('Work backward from being there.',88,249,55,C.ink,600);
  const k=ease((u-2)/1.4),p=updated;
  const xs=[115,490,865,1240],labs=['GET READY','LEAVE','ARRIVE','APPOINTMENT'];
  const old=[original.wakeAt,original.leaveAt,original.arriveAt,original.eventAt],next=[p.wakeAt,p.leaveAt,p.arriveAt,p.eventAt];
  line(140,459,1400,459,'#7fadd650',3);
  for(let i=0;i<4;i++)enter(u,.3+i*.25,()=>{dot(xs[i]+30,459,12,i===3?C.green:C.blue);label(labs[i],xs[i],365);text(formatTime(mix(old[i],next[i],k)).replace(' PM',''),xs[i],426,59,i===3?C.green:C.ink,500);text('PM',xs[i]+160,422,17,C.muted);});
  enter(u,1.5,()=>{chip('45 min to get ready',210,531);chip('20 min travel',612,531);chip('10 min buffer',1000,531);});
  enter(u,3.5,()=>{panel(90,606,1420,107);text('Appointment moved 10 minutes earlier',123,651,23,C.muted);text('Your preparation and departure move with it.',123,687,25,C.ink,500);chip('Plan recalculated',1265,668,C.green);});
  text('Travel time and preparation are sample assumptions.',90,762,17,C.muted);
}
function scene3(u,t){
  label(titles[3],90,165);text('5:50 PM.',88,266,78,C.ink,600);lines(['Time to leave.','No departure confirmed.'],90,339,35,48,C.muted);
  enter(u,.5,()=>{panel(90,477,760,207);label('PHONE SIGNALS',124,523);chip('Phone flat',124,575);chip('0 new counts',284,575);chip('Stationary',475,575);text('Device position does not tell us your body position.',124,644,21,C.muted);});
  phone(1207,432,.89,0,()=>{label('ENRO IS CALLING',-91,-196);text('enro',0,-105,50,C.ink,900,'center','Archivo');text('Time to head out',0,-65,18,C.muted,400,'center');voiceWave(-91,-9,t);rr(-124,43,248,152,16,'#ffffff08');lines(['“Your Price Center','appointment moved','to 6:20. It is time','to head out.”'],-105,76,19,29,C.ink);});
  for(let j=0;j<2;j++){let r=(((u*.55)+j/2)%1);c.beginPath();c.ellipse(1207,432,155+r*76,275+r*76,0,0,Math.PI*2);c.strokeStyle=`rgba(150,201,255,${(1-r)*.15})`;c.lineWidth=1;c.stroke()}
}
function scene4(u,t){
  label(titles[4],90,165);lines(['Moving the phone','is only one clue.'],88,251,62,72,C.ink,600);
  enter(u,.3,()=>{panel(90,418,830,288);const count=Math.min(7,Math.floor(u*2));label('PICKUP + COUNTER + VOICE',122,462);text(String(count).padStart(2,'0'),121,563,86,C.blue,500);text('new counts',252,543,26,C.muted);text('“I’m up.”',121,628,32,C.ink);chip('Activity · stationary',525,547,C.amber);});
  phone(1227,438,.79,Math.sin(t*8)*.09,()=>{text('Phone picked up',0,-144,22,C.ink,500,'center');voiceWave(-91,-53,t);text('Walking unconfirmed',0,36,19,C.amber,500,'center');lines(['The revised rule','keeps checking.'],-104,114,20,30,C.muted);});
  enter(u,3.8,()=>chip('Do not confirm walking',492,654,C.amber));
  text('Based on the seven shakes false positive from our phone trial.',90,762,19,C.muted);
}
function scene5(u,t){
  label(titles[5],90,165);text('Let the evidence change the response.',88,251,52,C.ink,600);
  const nodes=[{x:320,y:410,title:'8',sub:'fresh counts'},{x:780,y:410,title:'Walking',sub:'fresh Activity'},{x:1250,y:410,title:'“I’m up”',sub:'spoken check in'}];
  nodes.forEach((n,i)=>enter(u,i*.4,()=>{panel(n.x-200,324,400,198);text(n.title,n.x,416,i===0?67:39,C.blue,500,'center');text(n.sub,n.x,474,23,C.muted,400,'center');}));
  for(let i=0;i<3;i++)enter(u,1.6,()=>{line(nodes[i].x,524,780,620,C.line,2);let q=((t*.5+i*.2)%1);dot(mix(nodes[i].x,780,q),mix(524,620,q),4,C.green)});
  enter(u,2.3,()=>{rr(322,605,916,103,24,'#a9d9bc12','#a9d9bc30');text('“You’re moving. Leave now to keep your buffer.”',780,665,28,C.green,500,'center');});
  text('Experimental rule · Physical walking validation still needed',800,763,19,C.muted,400,'center');
}
function scene6(u){
  if(u<4.5){
    label(titles[6],90,165);lines(['A separate check','for heading out.'],88,252,64,72,C.ink,600);
    enter(u,.3,()=>{chip('Location · optional',92,428,C.green);lines(['Phone outside the starting area.','Route and arrival still unconfirmed.'],92,505,26,40,C.muted);});
    route(820,225,660,420,clamp(u/5)*.55);text('Crossroads',855,702,21,C.blue);text('Price Center',1445,218,21,C.green,400,'right');
  }else{
    const k=ease((u-4.5)/.5);c.save();c.globalAlpha=k;label('LESS CHECKING THE TIME',90,206);text('enro',800,420,156,C.ink,900,'center','Archivo');text('En route. On time.',800,513,43,C.blue,500,'center');text('A call when it counts. A little time back.',800,617,28,C.muted,400,'center');c.restore();
  }
}
const renderers=[scene0,scene1,scene2,scene3,scene4,scene5,scene6];
function draw(t){
  c.resetTransform();c.globalAlpha=1;bg(t);
  const idx=starts.findLastIndex(v=>t>=v),u=t-starts[idx];
  c.save();c.globalAlpha=Math.min(ease(u/.45),ease((ends[idx]-t)/.4));renderers[idx](u,t);c.restore();
  if(!(idx===6&&u>=4.5))text(captions[idx],800,804,21,C.ink,400,'center');
}
for(const [name,time] of [['poster',3.2],['event',11],['plan',20],['call',28],['check',37],['response',46],['departure',50]]){
  draw(time);await writeFile(path.join(preview,`${name}.png`),await canvas.encode('png'));
  if(name==='poster')await writeFile(path.join(out,'poster.jpg'),await canvas.encode('jpeg',90));
}
const vttTime=n=>`00:${String(Math.floor(n/60)).padStart(2,'0')}:${String(n%60).padStart(2,'0')}.000`;
await writeFile(path.join(out,'descriptions.vtt'),'WEBVTT\n\n'+captions.map((caption,i)=>`${vttTime(starts[i])} --> ${vttTime(ends[i])}\n${caption}\n`).join('\n'));
await writeFile(path.join(out,'manifest.json'),JSON.stringify({durationSeconds:DURATION,width:W,height:H,fps:FPS,audio:false,mode:'Scripted walkthrough using sample data',original,updated,checks:{shakingConfirmed:shaken.confirmed,walkingConfirmed:walked.confirmed},physicalValidation:'Revised walking rule is not yet physically validated'},null,2)+'\n');
if(process.argv.includes('--preview')){console.log(`Preview frames: ${preview}`);process.exit(0)}
const ff=spawn('ffmpeg',['-hide_banner','-loglevel','error','-y','-f','rawvideo','-pixel_format','rgba','-video_size',`${W}x${H}`,'-framerate',String(FPS),'-i','pipe:0','-an','-c:v','libx264','-preset','fast','-crf','21','-pix_fmt','yuv420p','-movflags','+faststart','-color_primaries','bt709','-color_trc','bt709','-colorspace','bt709',path.join(out,'enro.mp4')],{stdio:['pipe','ignore','pipe']});
let errors='';ff.stderr.on('data',chunk=>errors+=chunk);const done=once(ff,'close');ff.stdin.on('error',error=>console.error(error.message));
for(let frame=0;frame<FPS*DURATION;frame++){
  draw(frame/FPS);const rgba=c.getImageData(0,0,W,H).data;
  if(!ff.stdin.write(Buffer.from(rgba.buffer,rgba.byteOffset,rgba.byteLength)))await once(ff.stdin,'drain');
  if(frame%(FPS*8)===0)console.log(`Rendered ${frame/FPS}s / ${DURATION}s`);
}
ff.stdin.end();const [code]=await done;if(code!==0)throw new Error(errors);console.log(`Film saved: ${out}/enro.mp4`);
