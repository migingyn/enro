import assert from 'node:assert/strict';
import { spawn, type ChildProcess } from 'node:child_process';
import { existsSync } from 'node:fs';
import { mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import http from 'node:http';
import net from 'node:net';
import os from 'node:os';
import path from 'node:path';
import { test } from 'node:test';
import { fileURLToPath, pathToFileURL } from 'node:url';
import type { departureDemo } from '../src/core/departure-demo.js';
import type { MotionEvidenceSnapshot } from '../src/core/sensor-types.js';

const root = fileURLToPath(new URL('../', import.meta.url));

async function availablePort(): Promise<number> {
  const server = net.createServer();
  await new Promise<void>((resolve, reject) => {
    server.once('error', reject);
    server.listen(0, '127.0.0.1', resolve);
  });
  const address = server.address();
  assert.ok(address && typeof address !== 'string');
  await new Promise<void>((resolve, reject) => server.close(error => error ? reject(error) : resolve()));
  return address.port;
}

async function ready(child: ChildProcess): Promise<void> {
  return new Promise((resolve, reject) => {
    let output = '';
    const timer = setTimeout(() => finish(new Error(`Isolated server did not start: ${output}`)), 10_000);
    const onData = (chunk: Buffer) => {
      output = (output + chunk.toString()).slice(-20_000);
      if (output.includes('Enro console:')) finish();
    };
    const onExit = (code: number | null) => finish(new Error(`Isolated server exited (${code}): ${output}`));
    const onError = (error: Error) => finish(error);
    function finish(error?: Error) {
      clearTimeout(timer);
      child.stdout?.off('data', onData);
      child.stderr?.off('data', onData);
      child.off('exit', onExit);
      child.off('error', onError);
      if (error) reject(error); else resolve();
    }
    child.stdout?.on('data', onData);
    child.stderr?.on('data', onData);
    child.once('exit', onExit);
    child.once('error', onError);
  });
}

async function stop(child: ChildProcess | undefined): Promise<void> {
  if (!child?.pid) return;
  // tsx launches a child process: terminate the isolated process group as well.
  const signal = (name: NodeJS.Signals) => {
    try { process.kill(-child.pid!, name); }
    catch (error) { if ((error as NodeJS.ErrnoException).code !== 'ESRCH') throw error; }
  };
  signal('SIGTERM');
  if (child.exitCode !== null || child.signalCode !== null) return;
  await new Promise<void>(resolve => {
    const timer = setTimeout(() => { signal('SIGKILL'); resolve(); }, 2000);
    child.once('exit', () => { clearTimeout(timer); resolve(); });
  });
}

interface Reply { status: number; body: unknown }
function request(port: number, pathname: string, options: {
  method?: string;
  body?: unknown;
  headers?: Record<string, string>;
} = {}): Promise<Reply> {
  const body = options.body === undefined ? undefined : JSON.stringify(options.body);
  return new Promise((resolve, reject) => {
    const req = http.request({
      hostname: '127.0.0.1', port, path: pathname, method: options.method || 'GET',
      headers: {
        host: `localhost:${port}`,
        ...(body === undefined ? {} : { 'content-type': 'application/json', 'content-length': Buffer.byteLength(body).toString() }),
        ...options.headers,
      },
    }, res => {
      let text = '';
      res.setEncoding('utf8');
      res.on('data', chunk => { text += chunk; });
      res.on('end', () => {
        let parsed: unknown = text;
        try { parsed = JSON.parse(text); } catch { /* Some forbidden routes return plain text. */ }
        resolve({ status: res.statusCode || 0, body: parsed });
      });
      res.on('error', reject);
    });
    req.setTimeout(3000, () => req.destroy(new Error('Local HTTP request timed out.')));
    req.on('error', reject);
    req.end(body);
  });
}

test('isolated server HTTP boundaries and live sensor ingestion', { timeout: 25_000 }, async t => {
  const temporary = await mkdtemp(path.join(os.tmpdir(), 'enro-http-test-'));
  const data = path.join(temporary, 'data');
  let child: ChildProcess | undefined;
  t.after(async () => {
    try { await stop(child); }
    finally { await rm(temporary, { recursive: true, force: true }); }
  });
  const port = await availablePort();
  child = spawn(process.execPath, [path.join(root, 'node_modules/tsx/dist/cli.mjs'), path.join(root, 'src/server.ts')], {
    cwd: temporary,
    // Deliberately do not inherit credentials, NODE_OPTIONS, or workspace .env.
    env: { PATH: process.env.PATH, PORT: String(port), ENRO_DATA_DIR: data },
    detached: true, stdio: ['ignore', 'pipe', 'pipe'],
  });
  await ready(child);
  assert.equal((await request(port, '/health')).status, 200);
  const bootstrap = await request(port, '/api/bootstrap');
  assert.equal(bootstrap.status, 200);
  const token = (bootstrap.body as { token: string }).token;
  assert.match(token, /^[a-f\d]{64}$/);
  const headers = { authorization: `Bearer ${token}` };
  const state = async () => {
    const reply = await request(port, '/api/state', { headers });
    assert.equal(reply.status, 200);
    return reply.body as {
      config: { voiceReady: boolean; googleReady: boolean; routesReady: boolean };
      preferences: { locationEnabled: boolean };
      calls: unknown[];
      events: unknown[];
      plans: unknown[];
      motion: MotionEvidenceSnapshot;
    };
  };

  await t.test('protected API requires authentication', async () => {
    assert.equal((await request(port, '/api/state')).status, 401);
    assert.equal((await request(port, '/api/state', { headers: { authorization: 'Bearer incorrect' } })).status, 401);
    const initial = await state();
    assert.equal(initial.config.voiceReady, false);
    assert.equal(initial.config.googleReady, false);
    assert.equal(initial.config.routesReady, false);
    assert.equal(initial.preferences.locationEnabled, false);
    assert.deepEqual(initial.calls, []);
    assert.equal(initial.motion.status, 'unavailable');
  });

  await t.test('bootstrap rejects external Host and cross-site requests', async () => {
    assert.equal((await request(port, '/api/bootstrap', { headers: { host: 'external.example' } })).status, 403);
    assert.equal((await request(port, '/api/bootstrap', { headers: { 'sec-fetch-site': 'cross-site' } })).status, 403);
    assert.equal((await request(port, '/api/bootstrap', { headers: { origin: 'https://external.example' } })).status, 403);
  });

  await t.test('sensor token admits fresh native samples and ignores replay', async () => {
    const keys = JSON.parse(await readFile(path.join(data, 'keys.json'), 'utf8')) as { sensor: string };
    const now = Date.now();
    // Test fixture goes only to this child process and its temporary data directory.
    const sample = (at: number) => ({ name: 'accelerometer', time: at * 1_000_000, values: { x: 2, y: 0, z: 0 } });
    const payload = { messageId: 0, deviceId: 'isolated-test-phone', sessionId: 'isolated-test-recording', payload: [sample(now - 1500), sample(now - 100)] };
    assert.equal((await request(port, '/sensor/incorrect', { method: 'POST', body: payload })).status, 401);
    assert.equal((await state()).motion.sampleCount, 0);
    const accepted = await request(port, `/sensor/${keys.sensor}`, { method: 'POST', body: payload });
    assert.equal(accepted.status, 200);
    assert.deepEqual(accepted.body, { ok: true, accepted: 2, ignored: 0 });
    assert.equal((await state()).motion.source, 'sensor-logger');
    assert.equal((await state()).motion.status, 'moving');
    const replay = await request(port, `/sensor/${keys.sensor}`, { method: 'POST', body: payload });
    assert.deepEqual(replay.body, { ok: true, accepted: 0, ignored: 2 });
    assert.equal((await state()).motion.sampleCount, 2);
  });

  await t.test('missing call credentials fail without creating fictional call evidence', async () => {
    const reply = await request(port, '/api/calls/test', { method: 'POST', headers, body: {} });
    assert.equal(reply.status, 400);
    assert.match((reply.body as { error: string }).error, /not configured|credentials/i);
    assert.deepEqual((await state()).calls, []);
    const disk = JSON.parse(await readFile(path.join(data, 'state.json'), 'utf8')) as { calls: unknown[] };
    assert.deepEqual(disk.calls, []);
  });

  await t.test('unsigned voice callbacks are rejected', async () => {
    for (const endpoint of ['/voice/test-session', '/voice/test-session/status']) {
      assert.equal((await request(port, endpoint, { method: 'POST', body: { CallSid: 'test-call', CallStatus: 'completed' } })).status, 403);
    }
    assert.deepEqual((await state()).calls, []);
  });

  await t.test('manual events require future explicit-offset times', async () => {
    const future = new Date(Date.now() + 3_600_000).toISOString();
    const event = { title: 'Isolated fixture event', startAt: future, timeZone: 'America/Los_Angeles', location: 'Fixture location', requirements: [] };
    for (const startAt of [future.replace(/Z$/, ''), new Date(Date.now() - 60_000).toISOString()]) {
      assert.equal((await request(port, '/api/events', { method: 'POST', headers, body: { ...event, startAt } })).status, 400);
    }
    const reply = await request(port, '/api/events', { method: 'POST', headers, body: event });
    assert.equal(reply.status, 201);
    assert.equal((reply.body as { startAt: string }).startAt, future);
    assert.equal((await state()).events.length, 1);
  });

  await t.test('departure scenarios require auth and cannot alter live sensor or scheduling evidence', async () => {
    const before = await state();
    const diskBefore = await readFile(path.join(data, 'state.json'), 'utf8');
    const valid = { scenario: 'stationary', travelMinutes: 20, clockMinutes: 0 };
    for (const authorization of [undefined, 'Bearer incorrect']) {
      const reply = await request(port, '/api/demo/departure', {
        method: 'POST', body: valid, ...(authorization ? { headers: { authorization } } : {}),
      });
      assert.equal(reply.status, 401);
    }

    // Freshness can advance as wall time passes; the underlying observations
    // and counters must remain exactly the same after every synthetic exercise.
    const observations = (motion: MotionEvidenceSnapshot) => JSON.parse(JSON.stringify(motion,
      (key, value) => key === 'status' || key === 'scopedTo' ? undefined : value));
    const unchanged = async () => {
      const after = await state();
      assert.deepEqual(after.events, before.events);
      assert.deepEqual(after.plans, before.plans);
      assert.deepEqual(after.calls, before.calls);
      assert.deepEqual(observations(after.motion), observations(before.motion));
      assert.equal(after.preferences.locationEnabled, false);
      assert.equal(await readFile(path.join(data, 'state.json'), 'utf8'), diskBefore);
    };
    const scenario = async (name: ReturnType<typeof departureDemo>['scenario']) => {
      const reply = await request(port, '/api/demo/departure', {
        method: 'POST', headers, body: { ...valid, scenario: name },
      });
      assert.equal(reply.status, 200);
      const result = reply.body as ReturnType<typeof departureDemo>;
      assert.equal(result.source, 'synthetic-demo');
      assert.equal(result.routeSource, 'manual-assumption');
      assert.equal(result.callsEnabled, false);
      assert.equal(result.scenario, name);
      assert.equal(result.leaveClockMinutes, 0);
      assert.equal(result.wakeClockMinutes, -45);
      assert.equal(result.assessment.timeToLeave, true);
      await unchanged();
      return result;
    };

    const stationary = await scenario('stationary');
    assert.equal(stationary.assessment.status, 'still-inactive');
    assert.equal(stationary.assessment.activity, 'inactive');
    assert.equal(stationary.assessment.evidence.walkingObserved, false);
    assert.equal(stationary.assessment.location, 'unknown');

    const pickup = await scenario('pickup');
    assert.equal(pickup.pickupPossible, true);
    assert.equal(pickup.walkingSteps, 0);
    assert.equal(pickup.assessment.evidence.walkingObserved, false);
    assert.equal(pickup.assessment.location, 'unknown');

    const shaking = await scenario('shaking');
    assert.equal(shaking.assessment.status, 'activity-departure-unconfirmed');
    assert.equal(shaking.assessment.activity, 'active');
    assert.equal(shaking.assessment.evidence.walkingObserved, false);
    assert.equal(shaking.assessment.evidence.stepDelta, 0);
    assert.equal(shaking.assessment.location, 'unknown');

    const walking = await scenario('walking');
    assert.equal(walking.assessment.status, 'activity-departure-unconfirmed');
    assert.equal(walking.assessment.evidence.walkingObserved, true);
    assert.equal(walking.assessment.evidence.stepDelta, 6);
    assert.equal(walking.assessment.location, 'unknown');
    assert.equal(walking.assessment.evidence.gps.usableFixCount, 0);

    const departed = await scenario('departed-gps');
    assert.equal(departed.assessment.status, 'outside-origin-confirmed');
    assert.equal(departed.assessment.location, 'outside-origin-confirmed');
    assert.equal(departed.assessment.evidence.gps.outsideFixCount, 3);
    assert.equal(departed.assessment.evidence.walkingObserved, false);

    for (const invalid of [
      { ...valid, scenario: 'awake' },
      { ...valid, travelMinutes: 0 },
      { ...valid, travelMinutes: 181 },
      { ...valid, travelMinutes: '20' },
      { ...valid, clockMinutes: -121 },
      { ...valid, clockMinutes: 121 },
      { scenario: valid.scenario, travelMinutes: 20 },
      { ...valid, createCall: true },
    ]) {
      const reply = await request(port, '/api/demo/departure', { method: 'POST', headers, body: invalid });
      assert.equal(reply.status, 400);
    }
    await unchanged();
  });

  await t.test('remote Host cannot save credentials even with admin authorization', async () => {
    const reply = await request(port, '/api/config', {
      method: 'POST', headers: { ...headers, host: 'external.example' },
      body: { PUBLIC_BASE_URL: 'https://example.invalid' },
    });
    assert.equal(reply.status, 403);
    assert.equal(existsSync(path.join(temporary, '.env')), false);
  });

  await t.test('GPS and pose reach authenticated context without entering persisted call evidence', async () => {
    const placeAnchor = {label:'Isolated place',latitude:0,longitude:0,radiusMeters:300};
    assert.equal((await request(port, '/api/preferences', {method:'POST',headers,body:{locationEnabled:true,placeAnchor}})).status,200);
    const keys = JSON.parse(await readFile(path.join(data,'keys.json'),'utf8')) as {sensor:string};
    const now=Date.now();
    const sample=(name:string,values:Record<string,number>)=>({name,values,time:now*1_000_000});
    await request(port, `/sensor/${keys.sensor}`, {method:'POST',body:{deviceId:'pose-fixture',sessionId:'pose-fixture',messageId:0,payload:[
      sample('gravity',{x:0,y:0,z:9.8}), sample('orientation',{qw:1,qx:0,qy:0,qz:0}),
      sample('gyroscope',{x:0,y:0,z:1}), sample('location',{latitude:0,longitude:0,horizontalAccuracy:5,speed:0,speedAccuracy:0.1})
    ]}});
    const reply=await request(port,'/api/state',{headers});
    const body=reply.body as {pose:{phonePosition:{position:string};orientation:{status:string}},location:{status:string;movement:string;current:unknown},agentContext:{phonePosition:string}};
    assert.equal(body.pose.phonePosition.position,'horizontal');
    assert.equal(body.pose.orientation.status,'live');
    assert.equal(body.location.status,'near-place');
    assert.equal(body.location.movement,'stationary');
    assert.equal(body.agentContext.phonePosition,'horizontal');
    const disk=await readFile(path.join(data,'state.json'),'utf8');
    assert.doesNotMatch(disk,/"latitude"|"speedMetersPerSecond"/);
    await request(port,'/api/preferences',{method:'POST',headers,body:{locationEnabled:false,placeAnchor}});
    const disabled=(await request(port,'/api/state',{headers})).body as {location:{current:unknown;status:string}};
    assert.equal(disabled.location.current,null); assert.equal(disabled.location.status,'off');
  });
});

test('console stop preserves terminal call evidence and provider reconciliation', { timeout: 25_000 }, async t => {
  const temporary = await mkdtemp(path.join(os.tmpdir(), 'enro-stop-test-'));
  const data = path.join(temporary, 'data');
  let child: ChildProcess | undefined;
  t.after(async () => {
    try { await stop(child); }
    finally { await rm(temporary, { recursive: true, force: true }); }
  });
  await mkdir(data);
  const finishedAt = '2026-10-24T23:05:00.000Z';
  const makeCall = (id: string, extra: Record<string, unknown> = {}) => ({
    id, kind: 'test', callSid: id, status: 'unknown', createdAt: '2026-10-24T23:04:00.000Z',
    verbalConfirmed: false, movementObserved: false, rounds: 0, greeting: 'Isolated fixture', ...extra,
  });
  const ended = makeCall('already-ended', { status: 'completed', finishedAt, result: 'time-limit' });
  const failed = makeCall('failed-without-sid', { callSid: undefined, status: 'failed', finishedAt, result: 'carrier-rejected' });
  await writeFile(path.join(data, 'state.json'), JSON.stringify({
    events: [], plans: [{id:'inactivity-fixture',event:{id:'fixture-event',title:'Fixture',startAt:new Date(Date.now()+900000).toISOString(),timeZone:'UTC',location:'End',status:'confirmed',requirements:[],source:{type:'manual',id:'fixture-event'}},origin:'Start',originCoordinates:{latitude:0,longitude:0},destinationCoordinates:{latitude:0,longitude:0.05},travelMode:'DRIVE',route:{provider:'osrm',durationSeconds:480,fetchedAt:new Date().toISOString()},preparationMinutes:0,bufferMinutes:10,wakeAt:new Date(Date.now()-600000).toISOString(),leaveAt:new Date(Date.now()-600000).toISOString(),createdAt:new Date().toISOString(),armed:false}], jobs: {}, audit: [], calls: [ended, failed,
      makeCall('reconcile-ended', { result: 'time-limit' }), makeCall('reconcile-busy'),
      makeCall('cancel-active'), makeCall('still-active'), makeCall('callback-race', { result: 'time-limit' })],
  }));
  // Stub only this child process's voice provider. No provider credentials,
  // external network requests, or workspace runtime state are used.
  const stub = path.join(temporary, 'voice-fixture.mjs');
  await writeFile(stub, `
    import fs from 'node:fs';
    import path from 'node:path';
    export const voiceReady = () => false;
    export const hasActivePhoneCall = () => false;
    export const validSignature = () => true;
    export const greeting = () => 'Isolated fixture';
    export const voiceResponse = () => '<Response><Hangup/></Response>';
    export const placeCall = () => { throw new Error('Unexpected dial in isolated test'); };
    export const getCall = () => { throw new Error('Unexpected reconcile in isolated test'); };
    export async function stopCall(_config, sid) {
      const endTime = new Date('2026-10-24T23:06:00.000Z');
      if (sid === 'reconcile-ended') return { status: 'completed', endTime };
      if (sid === 'reconcile-busy') return { status: 'busy', endTime };
      if (sid === 'cancel-active') return { status: 'canceled', endTime };
      if (sid === 'still-active') return { status: 'in-progress', endTime: null };
      if (sid === 'callback-race') {
        fs.writeFileSync(path.join(process.env.ENRO_DATA_DIR, 'stop-waiting'), '');
        await new Promise(resolve => setTimeout(resolve, 150));
        return { status: 'completed', endTime };
      }
      throw new Error('Terminal calls must not reach the provider');
    }
  `);
  const preload = path.join(temporary, 'load-voice-fixture.mjs');
  await writeFile(preload, `
    import { registerHooks } from 'node:module';
    registerHooks({ resolve(specifier, context, nextResolve) {
      if (specifier.includes('/providers/voice.')) {
        return { url: ${JSON.stringify(pathToFileURL(stub).href)}, shortCircuit: true };
      }
      return nextResolve(specifier, context);
    }});
  `);
  const port = await availablePort();
  child = spawn(process.execPath, [path.join(root, 'node_modules/tsx/dist/cli.mjs'), path.join(root, 'src/server.ts')], {
    cwd: temporary,
    env: { PATH: process.env.PATH, PORT: String(port), ENRO_DATA_DIR: data, NODE_OPTIONS: `--import=${pathToFileURL(preload).href}` },
    detached: true, stdio: ['ignore', 'pipe', 'pipe'],
  });
  await ready(child);
  const bootstrap = await request(port, '/api/bootstrap');
  assert.equal(bootstrap.status, 200);
  const headers = { authorization: `Bearer ${(bootstrap.body as { token: string }).token}` };
  const stopFixture = (id: string) => request(port, `/api/calls/${id}/stop`, { method: 'POST', headers, body: {} });

  await t.test('inactivity configuration requires auth, supports 60/300 seconds, and cannot arm without calling',async()=>{
    const endpoint='/api/plans/inactivity-fixture/inactivity-rule';
    assert.equal((await request(port,endpoint,{method:'POST',body:{enabled:true,durationSeconds:60,armed:true}})).status,401);
    assert.equal((await request(port,endpoint,{method:'POST',headers,body:{enabled:true,durationSeconds:1}})).status,400);
    assert.equal((await request(port,endpoint,{method:'POST',headers,body:{enabled:true,durationSeconds:60,armed:true}})).status,409);
    const configured=await request(port,endpoint,{method:'POST',headers,body:{enabled:true}});
    assert.equal(configured.status,200);
    const p=configured.body as {armed:boolean;flatInactivity:{durationSeconds:number;armedAt:string}};
    assert.equal(p.armed,false);assert.equal(p.flatInactivity.durationSeconds,300);assert.ok(Number.isFinite(Date.parse(p.flatInactivity.armedAt)));
    const demo=await request(port,endpoint,{method:'POST',headers,body:{enabled:true,durationSeconds:60}});
    assert.equal((demo.body as {flatInactivity:{durationSeconds:number}}).flatInactivity.durationSeconds,60);
    const disabled=await request(port,endpoint,{method:'POST',headers,body:{enabled:false}});
    assert.equal((disabled.body as {armed:boolean;flatInactivity?:unknown}).armed,false);
    assert.equal((disabled.body as {flatInactivity?:unknown}).flatInactivity,undefined);
  });

  await t.test('already terminal sessions return unchanged without a provider request', async () => {
    for (const original of [ended, failed]) {
      const reply = await stopFixture(original.id);
      assert.equal(reply.status, 200);
      assert.deepEqual(reply.body, JSON.parse(JSON.stringify(original)));
    }
  });

  await t.test('reconciled carrier completion preserves the existing outcome and carrier end time', async () => {
    const reply = await stopFixture('reconcile-ended');
    assert.equal(reply.status, 200);
    assert.equal((reply.body as { status: string }).status, 'completed');
    assert.equal((reply.body as { result: string }).result, 'time-limit');
    assert.equal((reply.body as { finishedAt: string }).finishedAt, '2026-10-24T23:06:00.000Z');
  });

  await t.test('busy and canceled provider statuses are not rewritten as completed', async () => {
    for (const [id, status] of [['reconcile-busy', 'busy'], ['cancel-active', 'canceled']]) {
      const reply = await stopFixture(id!);
      assert.equal(reply.status, 200);
      assert.equal((reply.body as { status: string }).status, status);
      assert.equal((reply.body as { result: string }).result, `carrier-${status}`);
      assert.equal((reply.body as { finishedAt: string }).finishedAt, '2026-10-24T23:06:00.000Z');
    }
  });

  await t.test('a nonterminal provider result does not claim the call ended', async () => {
    const reply = await stopFixture('still-active');
    assert.equal(reply.status, 202);
    assert.equal((reply.body as { status: string }).status, 'in-progress');
    assert.equal((reply.body as { finishedAt?: string }).finishedAt, undefined);
    assert.equal((reply.body as { result?: string }).result, undefined);
  });

  await t.test('a terminal callback arriving during stop retains its timestamp and result', async () => {
    const pendingStop = stopFixture('callback-race');
    const deadline = Date.now() + 1000;
    while (!existsSync(path.join(data, 'stop-waiting')) && Date.now() < deadline) {
      await new Promise(resolve => setTimeout(resolve, 5));
    }
    assert.equal(existsSync(path.join(data, 'stop-waiting')), true);
    const callback = await request(port, '/voice/callback-race/status', {
      method: 'POST', body: { AccountSid: '', CallSid: 'callback-race', CallStatus: 'completed', SequenceNumber: '1' },
    });
    assert.equal(callback.status, 204);
    const state = await request(port, '/api/state', { headers });
    const callbackCall = (state.body as { calls: Array<{ id: string; finishedAt?: string }> }).calls.find(call => call.id === 'callback-race');
    assert.ok(callbackCall?.finishedAt);
    const stopped = await pendingStop;
    assert.equal(stopped.status, 200);
    assert.equal((stopped.body as { finishedAt: string }).finishedAt, callbackCall.finishedAt);
    assert.equal((stopped.body as { result: string }).result, 'time-limit');
    const disk = JSON.parse(await readFile(path.join(data, 'state.json'), 'utf8')) as { calls: Array<{ id: string; finishedAt?: string }> };
    assert.equal(disk.calls.find(call => call.id === 'callback-race')?.finishedAt, callbackCall.finishedAt);
  });
});
