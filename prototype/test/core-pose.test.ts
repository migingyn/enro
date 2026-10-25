import test from 'node:test';
import assert from 'node:assert/strict';
import {PoseTracker} from '../src/core/pose.js';
const now=Date.now();
const sample=(name:string,values:Record<string,number>,at=now)=>({name,values,time:at*1e6});
const batch=(payload:unknown[],messageId=1,sessionId='a')=>({deviceId:'phone',sessionId,messageId,payload});
test('actual quaternion, gravity, and gyroscope schema distinguish device pose and rotation',()=>{
  const p=new PoseTracker();
  p.ingest(batch([sample('orientation',{qw:1,qx:0,qy:0,qz:0}),sample('gravity',{x:0,y:0,z:9.8}),sample('gyroscope',{x:0,y:0,z:2})]),now);
  let s=p.snapshot(now);
  assert.equal(s.phonePosition.position,'horizontal'); assert.equal(s.orientation.status,'live');
  assert.equal(s.gyroscope.rotationRate,2);
  p.ingest(batch([sample('orientation',{qw:Math.SQRT1_2,qx:Math.SQRT1_2,qy:0,qz:0},now+1000),sample('gravity',{x:9.8,y:0,z:0},now+1000)],2),now+1000);
  s=p.snapshot(now+1000);assert.equal(s.phonePosition.position,'upright');
  assert.ok(Math.abs(s.orientation.rotationFromBaselineDegrees!-90)<0.001);
  assert.equal(p.snapshot(now+7000).phonePosition.position,'unknown');
  assert.equal(p.snapshot(now+7000).orientation.status,'stale');
});
test('pose rejects replay, invalid quaternions, and retired recording streams',()=>{
  const p=new PoseTracker();
  p.ingest(batch([sample('orientation',{qw:0,qx:0,qy:0,qz:0})]),now);
  assert.equal(p.snapshot(now).orientation.status,'unavailable');
  p.ingest(batch([sample('gravity',{x:0,y:0,z:9.8},now+1000)],2),now+1000);
  p.ingest(batch([sample('gravity',{x:9.8,y:0,z:0},now+1500)],2),now+1500);
  assert.equal(p.snapshot(now+1500).phonePosition.position,'horizontal');
  p.ingest(batch([sample('gravity',{x:9.8,y:0,z:0},now+2000)],1,'b'),now+2000);
  p.ingest(batch([sample('gravity',{x:0,y:0,z:9.8},now+3000)],3,'a'),now+3000);
  assert.equal(p.snapshot(now+3000).phonePosition.position,'upright');
});
