import type { EnroEvent } from '../types.js';
import { preparationRequirements } from '../core/requirements.js';

export interface GoogleTokens {
  accessToken: string;
  refreshToken?: string;
  expiresAt: number;
  scope?: string;
}

export interface GoogleConfig { clientId: string; clientSecret: string; redirectUri: string }
interface TokenStore { read(): GoogleTokens | null; write(tokens: GoogleTokens): void }
interface CalendarEvent {
  id?: string; summary?: string; status?: string; location?: string; htmlLink?: string; description?: string;
  start?: { date?: string; dateTime?: string; timeZone?: string };
  attendees?: Array<{ self?: boolean; responseStatus?: string }>;
}
interface MessagePart {
  mimeType?: string; filename?: string;
  headers?: Array<{ name?: string; value?: string }>;
  body?: { data?: string; attachmentId?: string; size?: number };
  parts?: MessagePart[];
}
export interface EmailCandidate {
  id: string; subject: string; body: string; receivedAt: string; sourceUrl: string;
  account?: string;
}

const SCOPES = [
  'https://www.googleapis.com/auth/calendar.events.readonly',
  'https://www.googleapis.com/auth/gmail.readonly',
];
const DAY = 86_400_000;
const TOKEN_ENDPOINT = 'https://oauth2.googleapis.com/token';
const GMAIL = 'https://gmail.googleapis.com/gmail/v1/users/me';

// OAuth: https://developers.google.com/identity/protocols/oauth2/web-server
// The HTTP callback owner MUST validate a random, single-use state value before exchangeCode.
export class GoogleProvider {
  private refreshing?: Promise<GoogleTokens>;

  constructor(
    private config: GoogleConfig,
    private tokenStore: TokenStore,
    private fetcher: typeof fetch = fetch,
  ) {}

  authorizationUrl(state: string, email?: string): string {
    this.requireConfig();
    if (!state) throw new Error('OAuth state is required.');
    const url = new URL('https://accounts.google.com/o/oauth2/v2/auth');
    url.search = new URLSearchParams({
      client_id: this.config.clientId, redirect_uri: this.config.redirectUri,
      response_type: 'code', scope: SCOPES.join(' '), state,
      access_type: 'offline', prompt: 'consent', include_granted_scopes: 'true',
      ...(email ? { login_hint: email } : {}),
    }).toString();
    return url.toString();
  }

  connected(): boolean {
    const tokens = this.tokenStore.read();
    return Boolean(tokens?.accessToken && (tokens.refreshToken || tokens.expiresAt > Date.now()));
  }

  async exchangeCode(code: string): Promise<void> {
    this.requireConfig();
    if (!code) throw new Error('Missing Google authorization code.');
    // New authorization must not retain a refresh token belonging to a different account.
    await this.tokenRequest({ code, grant_type: 'authorization_code', redirect_uri: this.config.redirectUri });
  }

  async accountEmail(): Promise<string> {
    const profile = await this.get<{ emailAddress?: string }>(new URL(`${GMAIL}/profile`));
    const email = profile.emailAddress?.trim().toLowerCase();
    if (!email || !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email)) throw new Error('Google did not return an account email.');
    return email;
  }

  // https://developers.google.com/workspace/calendar/api/v3/reference/events/list
  async listEvents(now = Date.now()): Promise<EnroEvent[]> {
    const result: EnroEvent[] = [];
    let pageToken: string | undefined;
    for (let page = 0; page < 10; page++) {
      const url = new URL('https://www.googleapis.com/calendar/v3/calendars/primary/events');
      url.search = new URLSearchParams({
        timeMin: new Date(now).toISOString(), timeMax: new Date(now + 7 * DAY).toISOString(),
        singleEvents: 'true', orderBy: 'startTime', showDeleted: 'false', maxResults: '250',
        ...(pageToken ? { pageToken } : {}),
      }).toString();
      const data = await this.get<{ items?: CalendarEvent[]; timeZone?: string; nextPageToken?: string }>(url);
      for (const item of data.items ?? []) {
        const start = item.start?.dateTime;
        const startMs = start ? Date.parse(start) : NaN;
        const attendance = item.attendees?.find(person => person.self)?.responseStatus;
        if (!item.id || !start || !Number.isFinite(startMs) || startMs < now || startMs >= now + 7 * DAY
          || item.status === 'cancelled' || !item.location?.trim()
          || attendance === 'declined') continue;
        result.push({
          id: `google:${item.id}`, title: item.summary?.trim() || 'Untitled event',
          startAt: start, timeZone: item.start?.timeZone || data.timeZone || 'UTC',
          location: item.location.trim(), requirements: preparationRequirements(item.description ?? ''),
          // The organizer confirming an event does not confirm this user's attendance.
          // Accept unanswered/tentative invitations in Google Calendar, then resync.
          status: item.status === 'tentative' || attendance === 'needsAction' || attendance === 'tentative'
            ? 'tentative' : 'confirmed',
          source: { type: 'calendar', id: item.id, ...(item.htmlLink ? { url: item.htmlLink } : {}) },
        });
      }
      pageToken = data.nextPageToken;
      if (!pageToken) return result;
    }
    throw new Error('Calendar result exceeds the 2,500-event import limit; narrow the calendar.');
  }

  // https://developers.google.com/workspace/gmail/api/reference/rest/v1/users.messages/list
  // Candidates require explicit event confirmation. receivedAt is NEVER an event start time.
  async listEmailCandidates(now = Date.now()): Promise<EmailCandidate[]> {
    const url = new URL(`${GMAIL}/messages`);
    url.search = new URLSearchParams({
      q: `after:${Math.floor((now - 30 * DAY) / 1000)} {subject:invitation subject:invite subject:appointment subject:reservation subject:event filename:ics}`,
      maxResults: '10', includeSpamTrash: 'false',
    }).toString();
    const list = await this.get<{ messages?: Array<{ id?: string }> }>(url);
    const candidates: EmailCandidate[] = [];
    for (const entry of (list.messages ?? []).slice(0, 10)) {
      if (!entry.id) continue;
      const message = await this.get<{ id?: string; internalDate?: string; payload?: MessagePart; snippet?: string }>(
        new URL(`${GMAIL}/messages/${encodeURIComponent(entry.id)}?format=full`),
      );
      const received = Number(message.internalDate);
      if (!Number.isFinite(received) || received <= 0) continue;
      const sections = await this.messageText(entry.id, message.payload ?? {});
      candidates.push({
        id: entry.id,
        subject: message.payload?.headers?.find(header => header.name?.toLowerCase() === 'subject')?.value || '(No subject)',
        body: (sections || message.snippet || '').slice(0, 80_000),
        receivedAt: new Date(received).toISOString(),
        sourceUrl: `https://mail.google.com/mail/u/0/#all/${encodeURIComponent(entry.id)}`,
      });
    }
    return candidates;
  }

  private requireConfig(): void {
    if (!this.config.clientId || !this.config.clientSecret || !this.config.redirectUri) {
      throw new Error('Google OAuth is not configured. Set client ID, client secret, and redirect URI.');
    }
  }

  private async tokenRequest(values: Record<string, string>, previous?: GoogleTokens): Promise<GoogleTokens> {
    this.requireConfig();
    const response = await this.fetcher(TOKEN_ENDPOINT, {
      method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: new URLSearchParams({ client_id: this.config.clientId, client_secret: this.config.clientSecret, ...values }),
      signal: AbortSignal.timeout(15_000),
    });
    if (!response.ok) throw new Error(`Google authorization failed (HTTP ${response.status}). Reconnect Google if access was revoked.`);
    const data = await response.json() as { access_token?: string; refresh_token?: string; expires_in?: number; scope?: string };
    if (!data.access_token || !Number.isFinite(data.expires_in) || Number(data.expires_in) <= 0) {
      throw new Error('Google returned an invalid token response.');
    }
    const refreshToken = data.refresh_token || previous?.refreshToken;
    const scope = data.scope || previous?.scope;
    const tokens: GoogleTokens = {
      accessToken: data.access_token, expiresAt: Date.now() + Number(data.expires_in) * 1000,
      ...(refreshToken ? { refreshToken } : {}), ...(scope ? { scope } : {}),
    };
    this.tokenStore.write(tokens);
    return tokens;
  }

  private async accessToken(forceRefresh = false): Promise<string> {
    const tokens = this.tokenStore.read();
    if (!tokens) throw new Error('Connect Google before importing events or email.');
    if (!forceRefresh && tokens.expiresAt > Date.now() + 60_000) return tokens.accessToken;
    if (!tokens.refreshToken) throw new Error('Google access expired. Reconnect Google.');
    this.refreshing ??= this.tokenRequest({ grant_type: 'refresh_token', refresh_token: tokens.refreshToken }, tokens)
      .finally(() => { this.refreshing = undefined; });
    return (await this.refreshing).accessToken;
  }

  private async get<T>(url: URL): Promise<T> {
    for (let attempt = 0; attempt < 2; attempt++) {
      const token = await this.accessToken(attempt === 1);
      const response = await this.fetcher(url, {
        headers: { Authorization: `Bearer ${token}` }, signal: AbortSignal.timeout(15_000),
      });
      if (response.status === 401 && attempt === 0) continue;
      if (!response.ok) throw new Error(`Google data request failed (HTTP ${response.status}). Check API enablement and granted permissions.`);
      return await response.json() as T;
    }
    throw new Error('Google rejected the refreshed authorization. Reconnect Google.');
  }

  private async messageText(messageId: string, root: MessagePart): Promise<string> {
    const plain: string[] = [], html: string[] = [], calendar: string[] = [];
    let visited = 0;
    const visit = async (part: MessagePart): Promise<void> => {
      if (++visited > 100) return;
      const mime = part.mimeType?.toLowerCase();
      const isCalendar = mime === 'text/calendar' || part.filename?.toLowerCase().endsWith('.ics');
      const isText = mime === 'text/plain' || mime === 'text/html' || isCalendar;
      if (isText && (part.body?.size ?? 0) <= 80_000) {
        let encoded = part.body?.data;
        if (!encoded && part.body?.attachmentId) {
          const attachment = await this.get<{ data?: string }>(new URL(
            `${GMAIL}/messages/${encodeURIComponent(messageId)}/attachments/${encodeURIComponent(part.body.attachmentId)}`,
          ));
          encoded = attachment.data;
        }
        if (encoded) {
          const value = Buffer.from(encoded, 'base64url').toString('utf8').slice(0, 80_000);
          if (isCalendar) calendar.push(value);
          else if (mime === 'text/plain') plain.push(value);
          else html.push(htmlToText(value));
        }
      }
      for (const child of part.parts ?? []) await visit(child);
    };
    await visit(root);
    return [...(plain.length ? plain : html), ...calendar].join('\n\n').trim();
  }
}

function htmlToText(html: string): string {
  // This text is still untrusted email content; callers must render it as text, never HTML.
  return html.replace(/<(script|style)\b[^>]*>[\s\S]*?<\/\1>/gi, '')
    .replace(/<br\s*\/?\s*>|<\/(p|div|li|tr)>/gi, '\n').replace(/<[^>]*>/g, ' ')
    .replace(/&nbsp;/gi, ' ').replace(/&lt;/gi, '<').replace(/&gt;/gi, '>')
    .replace(/&quot;/gi, '"').replace(/&#39;/gi, "'").replace(/&amp;/gi, '&').trim();
}
