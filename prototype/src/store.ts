import fs from 'node:fs';
import path from 'node:path';
import { randomBytes } from 'node:crypto';
import type { AppState } from './types.js';

export const dataDir = path.resolve(process.env.ENRO_DATA_DIR || '.data');
fs.mkdirSync(dataDir, { recursive: true, mode: 0o700 });

export function readPrivate<T>(name: string): T | null {
  const file = path.join(dataDir, name);
  if (!fs.existsSync(file)) return null;
  return JSON.parse(fs.readFileSync(file, 'utf8')) as T;
}
export function writePrivate(name: string, value: unknown) {
  const file = path.join(dataDir, name);
  const temporary = `${file}.tmp`;
  fs.writeFileSync(temporary, JSON.stringify(value, null, 2), { mode: 0o600 });
  fs.renameSync(temporary, file);
}
export class Store {
  state: AppState;
  constructor() {
    this.state = readPrivate<AppState>('state.json') || { events: [], plans: [], calls: [], jobs: {}, audit: [] };
    // A process crash cannot safely establish whether the carrier accepted a call.
    for (const job of Object.values(this.state.jobs)) {
      if (job.state === 'dispatching') job.state = 'unknown';
    }
    for (const call of this.state.calls) {
      if (['starting', 'queued', 'ringing', 'in-progress'].includes(call.status)) {
        call.status = 'unknown';
        call.result = 'Server restarted; reconcile carrier status before another call.';
      }
    }
    this.save();
  }
  save() { writePrivate('state.json', this.state); }
  audit(type: string, message: string) {
    this.state.audit.unshift({ at: new Date().toISOString(), type, message });
    this.state.audit = this.state.audit.slice(0, 150);
    this.save();
  }
}
export function localKeys(): { admin: string; sensor: string } {
  let keys = readPrivate<{ admin: string; sensor: string }>('keys.json');
  if (!keys) {
    keys = { admin: randomBytes(32).toString('hex'), sensor: randomBytes(32).toString('hex') };
    writePrivate('keys.json', keys);
  }
  return keys;
}
