import fs from 'node:fs';
import dotenv from 'dotenv';
import { z } from 'zod';

const allowed = z.object({
  PUBLIC_BASE_URL: z.string().url().refine(v => new URL(v).protocol === 'https:' || new URL(v).hostname === 'localhost').optional(),
  TWILIO_ACCOUNT_SID: z.string().regex(/^AC[0-9a-f]{32}$/i).optional(),
  TWILIO_AUTH_TOKEN: z.string().min(16).max(200).optional(),
  TWILIO_FROM_NUMBER: z.string().regex(/^\+[1-9]\d{7,14}$/).optional(),
  GOOGLE_CLIENT_ID: z.string().min(10).max(400).optional(),
  GOOGLE_CLIENT_SECRET: z.string().min(8).max(400).optional(),
  GOOGLE_REDIRECT_URI: z.string().url().refine(v => new URL(v).protocol === 'https:' || (new URL(v).protocol === 'http:' && new URL(v).hostname === 'localhost')).optional(),
  GOOGLE_MAPS_API_KEY: z.string().min(10).max(400).optional(),
}).strict();
export function config() {
  const disk = fs.existsSync('.env') ? dotenv.parse(fs.readFileSync('.env')) : {};
  const get = (key: string) => disk[key] || process.env[key] || '';
  return {
    port: Number(get('PORT') || 3333), baseUrl: get('PUBLIC_BASE_URL').replace(/\/$/, ''),
    phone: get('ENRO_TEST_PHONE'), accountSid: get('TWILIO_ACCOUNT_SID'), authToken: get('TWILIO_AUTH_TOKEN'),
    from: get('TWILIO_FROM_NUMBER'), clientId: get('GOOGLE_CLIENT_ID'), clientSecret: get('GOOGLE_CLIENT_SECRET'), mapsKey: get('GOOGLE_MAPS_API_KEY'),
    googleRedirectUri: get('GOOGLE_REDIRECT_URI') || `${get('PUBLIC_BASE_URL').replace(/\/$/, '')}/oauth/google/callback`,
  };
}
export function saveConfig(input: unknown) {
  const parsed = allowed.parse(input);
  const disk = fs.existsSync('.env') ? dotenv.parse(fs.readFileSync('.env')) : {};
  for (const [key, value] of Object.entries(parsed)) {
    if (value && /[\r\n\x00]/.test(value)) throw new Error('Configuration contains invalid characters.');
    if (value) disk[key] = value;
  }
  fs.writeFileSync('.env.tmp', Object.entries(disk).map(([key, value]) => `${key}=${JSON.stringify(value)}`).join('\n') + '\n', { mode: 0o600 });
  fs.renameSync('.env.tmp', '.env');
}
