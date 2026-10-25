import fs from 'node:fs';
import path from 'node:path';
import twilio from 'twilio';
import { config } from '../src/config.js';
import { voiceReady } from '../src/providers/voice.js';
import { PARKING_DEMO, parkingDemoTwiml } from '../src/providers/parking-demo.js';

// Preview is the default. --reset ends this demo's previous call before replaying.
const args = process.argv.slice(2);
const resetting = args.includes('--reset');
if (args.some(arg => !['--call', '--preview', '--reset'].includes(arg) && !/^--delay=\d+$/.test(arg))
  || args.filter(a => a.startsWith('--delay=')).length > 1 || ((args.includes('--call') || resetting) && args.includes('--preview'))) {
  throw new Error('Usage: node --import tsx scripts/call-parking-demo.ts [--preview|--call|--reset] [--delay=5]');
}
const delay = Number(args.find(arg => arg.startsWith('--delay='))?.split('=')[1] ?? 5);
const twiml = parkingDemoTwiml(delay);
if (!args.includes('--call') && !resetting) {
  console.log(`SCRIPTED DEMO · Gilman Parking Structure · male voice · speech rate ${PARKING_DEMO.speed}x · no call placed`);
  console.log(twiml);
} else {
  const c = config();
  if (!voiceReady(c)) throw new Error('The existing local calling configuration is incomplete.');
  const base = `http://127.0.0.1:${c.port}`;
  const bootstrap = await fetch(base + '/api/bootstrap').then(r => {
    if (!r.ok) throw new Error('Open the local Enro server before running the demo.');
    return r.json() as Promise<{token:string}>;
  });
  const state = await fetch(base + '/api/state', {headers:{Authorization:`Bearer ${bootstrap.token}`}}).then(r => {
    if (!r.ok) throw new Error('Cannot check the main call scheduler.');
    return r.json() as Promise<{calls:{status:string}[];plans:{armed:boolean;event:{status:string};wakeAt:string;leaveAt:string;reminderKinds?:string[]}[]}>;
  });
  const terminal = new Set(['completed','canceled','busy','failed','no-answer']);
  if (state.calls.some(call => !terminal.has(call.status))) throw new Error('A main Enro call is active or unresolved. Wait for it to finish.');
  const now = Date.now();
  if (state.plans.some(plan => plan.armed && plan.event.status === 'confirmed'
    && (plan.reminderKinds ?? ['wake','leave']).some(kind => {
      const at=Date.parse(kind === 'wake' ? plan.wakeAt : plan.leaveAt);
      return at >= now-120000 && at <= now+90000;
    }))) throw new Error('A scheduled reminder is due soon. Wait until it finishes before placing this separate demo call.');
  const client = twilio(c.accountSid,c.authToken,{autoRetry:false,timeout:15000});
  const logFile = path.resolve(process.env.ENRO_DATA_DIR || '.data','parking-demo-call.json');
  if (fs.existsSync(logFile)) {
    const previous=JSON.parse(fs.readFileSync(logFile,'utf8')) as {callSid?:string;status:string;scripted?:boolean};
    if (!terminal.has(previous.status)) {
      if (!previous.callSid) throw new Error('The previous demo carrier outcome is unknown. Inspect Twilio before another attempt.');
      const prior=await client.calls(previous.callSid).fetch();
      if (!terminal.has(prior.status)) {
        if (!resetting) throw new Error('The previous demo call is still active. Use the explicit reset command to restart it.');
        if (previous.scripted !== true || prior.to !== c.phone || prior.from !== c.from) throw new Error('The previous call does not match this scripted demo. It was not stopped.');
        try {
          const ended=await client.calls(previous.callSid).update({status:['queued','ringing'].includes(prior.status)?'canceled':'completed'});
          if (!terminal.has(ended.status)) throw new Error('Stop still pending.');
          fs.writeFileSync(logFile,JSON.stringify({...previous,status:ended.status,finishedAt:new Date().toISOString()},null,2)+'\n',{mode:0o600});
        } catch {
          throw new Error('Could not confirm the old demo call ended. No fresh call was placed.');
        }
      }
    }
  }
  // Check the carrier too: this separate demo does not enter the main scheduler's state.
  for (const status of ['in-progress','ringing','queued'] as const) {
    const calls=await client.calls.list({to:c.phone,status,limit:1});
    if (calls.length) throw new Error('The configured phone already has an active carrier call.');
  }
  fs.mkdirSync(path.dirname(logFile),{recursive:true,mode:0o700});
  const record={scripted:true,reset:resetting,destination:PARKING_DEMO.destination,speed:PARKING_DEMO.speed,delaySeconds:delay,createdAt:new Date().toISOString(),status:'dispatching'};
  const save=(data:unknown)=>fs.writeFileSync(logFile,JSON.stringify(data,null,2)+'\n',{mode:0o600});
  save(record);
  try {
    const call=await client.calls.create({to:c.phone,from:c.from,twiml,timeout:25,timeLimit:180,record:false});
    save({...record,status:call.status,callSid:call.sid});
    console.log(JSON.stringify({scripted:true,destination:PARKING_DEMO.destination,speed:PARKING_DEMO.speed,status:call.status,callSid:call.sid}));
  } catch(error) {
    const rejected=typeof (error as {status?:number}).status==='number' && (error as {status:number}).status>=400 && (error as {status:number}).status<500;
    save({...record,status:rejected?'failed':'unknown'});
    throw new Error(rejected?'Carrier rejected the demo call. No retry made.':'Carrier outcome unknown. Inspect Twilio before retrying.');
  }
}
