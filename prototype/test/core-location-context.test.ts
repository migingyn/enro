import test from 'node:test';
import assert from 'node:assert/strict';
import {locationContext} from '../src/core/location-context.js';
import type {DepartureLocationFix} from '../src/core/departure.js';
const now=Date.now();const anchor={label:'Fixture place',latitude:0,longitude:0,radiusMeters:300};
const fix=(age:number,meters=0,extra:Partial<DepartureLocationFix>={}):DepartureLocationFix=>({deviceId:'phone',sessionId:'a',latitude:meters/111195,longitude:0,accuracyMeters:5,timestamp:now-age,...extra});
test('location off and stale GPS never expose a current coordinate or assume a place',()=>{
  assert.equal(locationContext(false,[fix(0)],anchor,now).current,null);
  assert.equal(locationContext(true,[fix(46000)],anchor,now).status,'waiting');
  assert.match(locationContext(true,[fix(0)],undefined,now).summary,/Set a place reference/);
});
test('repeated accurate inside fixes establish staying near a place independently of phone motion',()=>{
  const s=locationContext(true,[fix(20000,5),fix(10000,10),fix(0,8)],anchor,now);
  assert.equal(s.status,'staying-near-place');assert.equal(s.location,'inside-origin');
  assert.equal(s.movement,'unknown');assert.equal(s.trail.length,3);
});
test('GPS speed uses its accuracy; missing accuracy never proves movement',()=>{
  assert.equal(locationContext(true,[fix(0,0,{speedMetersPerSecond:3,speedAccuracyMetersPerSecond:0.2})],anchor,now).movement,'moving');
  assert.equal(locationContext(true,[fix(0,0,{speedMetersPerSecond:0,speedAccuracyMetersPerSecond:0.2})],anchor,now).movement,'stationary');
  assert.equal(locationContext(true,[fix(0,0,{speedMetersPerSecond:3})],anchor,now).movement,'unknown');
});
test('repeated outward GPS movement establishes moving away, not a destination',()=>{
  const s=locationContext(true,[fix(20000,500),fix(10000,650),fix(0,800)],anchor,now);
  assert.equal(s.status,'moving-away');assert.equal(s.movement,'moving');assert.equal(s.trend,'away');
  assert.doesNotMatch(s.summary,/arrived|on route/);
});
test('poor accuracy and boundary overlap remain uncertain',()=>{
  assert.equal(locationContext(true,[fix(0,0,{accuracyMeters:100})],anchor,now).status,'imprecise');
  assert.equal(locationContext(true,[fix(0,295,{accuracyMeters:10})],anchor,now).status,'uncertain');
});
