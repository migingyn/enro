import test from 'node:test';
import assert from 'node:assert/strict';
import { FlatInactivityTracker, inactivityDecision, inactivityWatchKey } from '../src/core/inactivity.js';
import { dueJobs } from '../src/core/planning.js';
import type { MorningPlan } from '../src/types.js';
import type { TripContext } from '../src/core/trip.js';
const base=Date.parse('2026-10-25T01:00:00Z');
function body(second:number,patch:{movement?:boolean;tilt?:boolean;rotation?:boolean;missing?:string;session?:string}={}) {
 const at=base+second*1000;
 return {deviceId:'fixture-phone',sessionId:patch.session??'one',messageId:second,payload:[
  {name:'gravity',time:at*1e6,values:patch.tilt?{x:9.8,y:0,z:0}:{x:0,y:0,z:9.8}},
  {name:'accelerometer',time:at*1e6,values:{x:patch.movement?2:0.01,y:0,z:0}},
  {name:'gyroscope',time:at*1e6,values:{x:patch.rotation?1:0.01,y:0,z:0}},
 ].filter(s=>s.name!==patch.missing)};
}
function feed(tracker:FlatInactivityTracker,from:number,to:number) {for(let n=from;n<=to;n++)tracker.ingest(body(n),base+n*1000);}
const plan:MorningPlan={id:'watch',event:{id:'event',title:'Fixture',startAt:new Date(base+3600000).toISOString(),timeZone:'America/Los_Angeles',location:'End',requirements:[],status:'confirmed',source:{type:'manual',id:'event'}},origin:'Start',travelMode:'DRIVE',preparationMinutes:0,bufferMinutes:10,route:{provider:'osrm',fetchedAt:new Date(base).toISOString(),durationSeconds:480},createdAt:new Date(base).toISOString(),wakeAt:new Date(base).toISOString(),leaveAt:new Date(base).toISOString(),armed:true,reminderKinds:['leave'],flatInactivity:{durationSeconds:60,armedAt:new Date(base).toISOString()}};
const trip:TripContext={status:'still-at-origin',summary:'Fixture phone at origin',distanceToDestinationMeters:3000,gpsMovement:'stationary',destinationConfirmed:false,phonePosition:'horizontal',activity:'stationary',observedAt:new Date(base+60000).toISOString(),lastGpsSampleAt:base+60000};
test('requires the complete 60 seconds; default 5-minute mode does not finish early',()=>{
 const t=new FlatInactivityTracker();feed(t,0,59);assert.equal(t.snapshot(base+59000,60).status,'counting');feed(t,60,60);
 assert.equal(t.snapshot(base+60000,60).status,'qualified');assert.equal(t.snapshot(base+60000).status,'counting');
 feed(t,61,300);assert.equal(t.snapshot(base+300000).status,'qualified');
});
test('single short shakes, tilts, or rotations reset an otherwise almost-complete minute',()=>{
 for(const patch of [{movement:true},{tilt:true},{rotation:true}]) {
  const t=new FlatInactivityTracker();feed(t,0,59);t.ingest(body(60,patch),base+60000);assert.equal(t.snapshot(base+60000,60).observedSeconds,0);
  feed(t,61,120);assert.equal(t.snapshot(base+120000,60).status,'counting');feed(t,121,121);assert.equal(t.snapshot(base+121000,60).status,'qualified');
 }
});
test('missing sensors, feed gaps, stale samples, and restarts never manufacture an idle minute',()=>{
 const t=new FlatInactivityTracker();feed(t,0,40);assert.equal(t.snapshot(base+44000,60).status,'unknown');feed(t,45,65);assert.equal(t.snapshot(base+65000,60).observedSeconds,20);
 for(let n=66;n<=130;n++)t.ingest(body(n,{missing:'gyroscope'}),base+n*1000);
 assert.equal(t.snapshot(base+130000,60).status,'unknown');
 assert.equal(new FlatInactivityTracker().snapshot(base+130000,60).status,'unknown');
});
test('duplicate batches, out-of-order samples, and old recordings cannot extend a streak',()=>{
 const t=new FlatInactivityTracker();feed(t,0,40);t.ingest(body(40),base+41000);assert.equal(t.snapshot(base+41000,60).observedSeconds,40);
 t.ingest(body(39),base+41000);assert.equal(t.snapshot(base+41000,60).observedSeconds,40);
 t.ingest(body(41,{session:'two'}),base+41000);assert.equal(t.snapshot(base+41000,60).observedSeconds,0);
 t.ingest(body(42,{session:'one'}),base+42000);assert.equal(t.snapshot(base+42000,60).observedSeconds,0);
});
test('explicit arming starts fresh and can monitor an already-passed leave time until the event',()=>{
 const t=new FlatInactivityTracker();feed(t,0,120);
 const armed=base+90000;const p={...plan,flatInactivity:{durationSeconds:60 as const,armedAt:new Date(armed).toISOString()}};
 let snapshot=t.snapshot(base+120000,60,armed);assert.equal(snapshot.observedSeconds,30);assert.equal(inactivityDecision(p,snapshot,trip,base+120000).shouldCall,false);
 feed(t,121,150);snapshot=t.snapshot(base+150000,60,armed);assert.equal(inactivityDecision(p,snapshot,{...trip,lastGpsSampleAt:base+150000},base+150000).shouldCall,true);
 assert.equal(dueJobs([p],{},base+150000).length,0);assert.match(inactivityWatchKey(p),/flat-inactive/);
});
test('calling requires all gates: leave time, armed future event, idle duration, and origin location',()=>{
 const t=new FlatInactivityTracker();feed(t,0,60);const s=t.snapshot(base+60000,60,base);
 assert.equal(inactivityDecision(plan,s,trip,base+60000).shouldCall,true);
 assert.equal(inactivityDecision({...plan,armed:false},s,trip,base+60000).shouldCall,false);
 assert.equal(inactivityDecision({...plan,leaveAt:new Date(base+120000).toISOString()},s,trip,base+60000).shouldCall,false);
 assert.equal(inactivityDecision(plan,s,{...trip,status:'unknown'},base+60000).shouldCall,false);
 assert.equal(inactivityDecision(plan,s,{...trip,status:'moving-near-origin'},base+60000).shouldCall,false);
 assert.equal(inactivityDecision(plan,s,{...trip,status:'departed-origin'},base+60000).shouldCall,false);
 assert.equal(inactivityDecision(plan,s,{...trip,lastGpsSampleAt:base+49000},base+60000).shouldCall,false);
 assert.equal(inactivityDecision(plan,s,{...trip,status:'approaching-destination',travelProgressConfirmed:true},base+60000).satisfied,true);
 assert.equal(inactivityDecision(plan,s,trip,base+3600000).status,'expired');
});
