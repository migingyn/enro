import test from 'node:test';
import assert from 'node:assert/strict';
import { assessTrip, tripSatisfiesReminder } from '../src/core/trip.js';
import { createPlan, dueJobs } from '../src/core/planning.js';
import { estimateOsrmDrive } from '../src/providers/routes.js';
import { preparationRequirements } from '../src/core/requirements.js';
import type { DepartureLocationFix } from '../src/core/departure.js';
import type { MorningPlan } from '../src/types.js';
const now=Date.parse('2026-10-25T00:00:00Z');
const plan: MorningPlan={id:'trip',origin:'Start',originCoordinates:{latitude:0,longitude:0},destinationCoordinates:{latitude:0,longitude:0.05},originRadiusMeters:200,destinationRadiusMeters:150,travelMode:'DRIVE',preparationMinutes:0,bufferMinutes:10,route:{provider:'osrm',durationSeconds:480,fetchedAt:new Date(now).toISOString()},wakeAt:new Date(now).toISOString(),leaveAt:new Date(now).toISOString(),createdAt:new Date(now).toISOString(),armed:true,event:{id:'event',title:'Appointment',startAt:new Date(now+3600000).toISOString(),timeZone:'America/Los_Angeles',location:'End',requirements:['Business professional: full suit'],status:'confirmed',source:{type:'manual',id:'event'}}};
const fix=(age:number,meters:number,extra:Partial<DepartureLocationFix>={}):DepartureLocationFix=>({deviceId:'phone',sessionId:'current',latitude:0,longitude:meters/111195,accuracyMeters:5,timestamp:now-age,...extra});
test('an explicit invitation dress code becomes a reminder, optional or negated codes do not',()=>{
 assert.deepEqual(preparationRequirements('Meet at 6:30 PM.\nDress code: Business professional (full suit).'),['Business professional: full suit']);
 assert.deepEqual(preparationRequirements('Dress code: Business professional not required.'),[]);
 assert.deepEqual(preparationRequirements('Full suit discussed in an unrelated sentence.'),[]);
});
test('stationary horizontal phone stays at origin; pose never proves body posture',()=>{
 const s=assessTrip(plan,[fix(20000,0),fix(10000,1),fix(0,0,{speedMetersPerSecond:0,speedAccuracyMetersPerSecond:0.2})],now,'horizontal','stationary');
 assert.equal(s.status,'still-at-origin');assert.equal(s.destinationConfirmed,false);assert.doesNotMatch(s.summary,/sitting|sleeping/);
 const coarse=assessTrip(plan,[fix(0,0,{accuracyMeters:33})],now);
 assert.equal(coarse.status,'still-at-origin');assert.match(coarse.summary,/33 meters/);assert.equal(tripSatisfiesReminder(coarse,'leave'),false);
 assert.equal(assessTrip(plan,[fix(0,180,{accuracyMeters:33})],now).status,'unknown');
});
test('moving toward the car within the origin remains distinct from destination progress',()=>{
 const s=assessTrip(plan,[fix(20000,0),fix(10000,60),fix(0,120)],now,'upright','walking');
 assert.equal(s.status,'moving-near-origin');assert.equal(s.destinationConfirmed,false);
});
test('leaving origin while approaching versus moving away from the destination',()=>{
 const progress=assessTrip(plan,[fix(20000,500),fix(10000,600),fix(0,700)],now);
 assert.equal(progress.status,'approaching-destination');assert.equal(tripSatisfiesReminder(progress,'leave'),true);assert.equal(tripSatisfiesReminder(progress,'wake'),false);
 const slight=assessTrip(plan,[fix(20000,500),fix(10000,525),fix(0,550)],now);
 assert.equal(slight.status,'approaching-destination');assert.equal(tripSatisfiesReminder(slight,'leave'),false);
 assert.equal(assessTrip(plan,[fix(20000,700),fix(10000,600),fix(0,500)],now).status,'moving-away-from-destination');
 assert.equal(assessTrip(plan,[fix(20000,700),fix(10000,701),fix(0,700)],now).status,'departed-origin');
 assert.equal(tripSatisfiesReminder(assessTrip(plan,[fix(20000,700),fix(10000,701),fix(0,700)],now),'leave'),false);
});
test('arrival requires repeated precise observations in the current session and interval',()=>{
 const at=[fix(20000,5560),fix(10000,5561),fix(0,5560)];
 assert.equal(assessTrip(plan,at,now).destinationConfirmed,true);
 assert.equal(assessTrip(plan,[at[2]!],now).destinationConfirmed,false);
 assert.equal(assessTrip(plan,[at[2]!,at[2]!,at[2]!],now).destinationConfirmed,false);
 assert.equal(assessTrip(plan,at.map(f=>({...f,accuracyMeters:100})),now).destinationConfirmed,false);
 assert.equal(assessTrip(plan,at.map(f=>({...f,timestamp:f.timestamp-46000})),now).destinationConfirmed,false);
 assert.equal(assessTrip(plan,[...at.slice(0,2).map(f=>({...f,sessionId:'old'})),at[2]!],now).destinationConfirmed,false);
 assert.equal(assessTrip(plan,[fix(20000,5200),fix(10000,5561),fix(0,5560)],now).destinationConfirmed,false);
});
test('driving plan includes time to the car and departure-only scheduling',()=>{
 const p=createPlan({event:plan.event,origin:plan.origin,travelMode:'DRIVE',preparationMinutes:0,bufferMinutes:10,accessMinutes:10,route:plan.route},now);
 assert.equal(Date.parse(p.leaveAt),Date.parse(p.event.startAt)-28*60000);
 p.armed=true;p.reminderKinds=['leave'];
 const jobs=dueJobs([p],{},Date.parse(p.leaveAt));assert.equal(jobs.length,1);assert.equal(jobs[0]?.kind,'leave');
 assert.equal(dueJobs([p],{[jobs[0]!.key]:{state:'satisfied',at:p.leaveAt}},Date.parse(p.leaveAt)).length,0);
 assert.throws(()=>createPlan({event:plan.event,origin:plan.origin,travelMode:'WALK',preparationMinutes:0,bufferMinutes:10,route:plan.route},now),/driving only/);
});
test('OSRM adapter uses driving, validates the result, and labels the provider',async()=>{
 const fetcher=(async(url:string|URL|Request)=>{assert.match(String(url),/route\/v1\/driving/);return new Response(JSON.stringify({code:'Ok',routes:[{duration:458.9,distance:4665.5}],waypoints:[{distance:97},{distance:3}]}));}) as typeof fetch;
 const r=await estimateOsrmDrive({latitude:0,longitude:0},{latitude:0,longitude:0.05},fetcher);
 assert.equal(r.provider,'osrm');assert.equal(r.durationSeconds,459);
 await assert.rejects(estimateOsrmDrive({latitude:0,longitude:0},{latitude:0,longitude:0.05},(async()=>new Response(JSON.stringify({code:'NoRoute'}))) as typeof fetch),/No usable/);
});
