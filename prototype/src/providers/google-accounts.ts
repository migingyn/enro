import { GoogleProvider, type GoogleConfig, type GoogleTokens, type EmailCandidate } from './google.js';
import { parseInvitationCandidates } from './invitations.js';
import type { EnroEvent } from '../types.js';

export type GoogleAccountTokens = Record<string, GoogleTokens>;
interface AccountStore { read(): GoogleAccountTokens | null; write(accounts: GoogleAccountTokens): void }

/** Keep account tokens and source IDs separate, including on reconnect and refresh. */
export class GoogleAccounts {
  constructor(private config: () => GoogleConfig, private store: AccountStore, private fetcher: typeof fetch = fetch) {}

  authorizationUrl(state: string, email?: string) {
    return new GoogleProvider(this.config(), { read: () => null, write: () => {} }, this.fetcher).authorizationUrl(state, email);
  }

  async connect(code: string, expectedEmail?: string): Promise<string> {
    let pending: GoogleTokens | null = null;
    const provider = new GoogleProvider(this.config(), { read: () => pending, write: tokens => { pending = tokens; } }, this.fetcher);
    await provider.exchangeCode(code);
    const email = await provider.accountEmail();
    if (expectedEmail && email !== expectedEmail.trim().toLowerCase()) {
      throw new Error('A different Google account was selected. Connect again with the requested account.');
    }
    if (!pending) throw new Error('Google did not return account credentials.');
    this.store.write({ ...this.store.read(), [email]: pending });
    return email;
  }

  private provider(email: string) {
    return new GoogleProvider(this.config(), {
      read: () => this.store.read()?.[email] || null,
      write: tokens => this.store.write({ ...this.store.read(), [email]: tokens }),
    }, this.fetcher);
  }

  list() {
    return Object.keys(this.store.read() || {}).sort().map(email => ({ email, connected: this.provider(email).connected() }));
  }

  async import(emailOnly = false): Promise<{ calendar: EnroEvent[]; invitations: EnroEvent[] }> {
    const emails = this.list().map(account => account.email);
    if (!emails.length) throw new Error('Connect a Google account before importing.');
    // Fail the whole import before updating stored events if any account fails.
    const batches = await Promise.all(emails.map(async email => {
      const provider = this.provider(email);
      const [calendar, candidates] = await Promise.all([
        emailOnly ? Promise.resolve([]) : provider.listEvents(), provider.listEmailCandidates(),
      ]);
      const scope = (event: EnroEvent): EnroEvent => ({
        ...event, id: event.id.replace(':', `:${email}:`), source: { ...event.source, account: email },
      });
      return { calendar: calendar.map(scope), invitations: parseInvitationCandidates(candidates.map(candidate => this.scopeEmail(candidate, email))).map(scope) };
    }));
    return { calendar: batches.flatMap(batch => batch.calendar), invitations: batches.flatMap(batch => batch.invitations) };
  }

  async candidates(): Promise<EmailCandidate[]> {
    const emails = this.list().map(account => account.email);
    if (!emails.length) throw new Error('Connect a Google account before reading email.');
    const batches = await Promise.all(emails.map(async email => (await this.provider(email).listEmailCandidates()).map(candidate => this.scopeEmail(candidate, email))));
    return batches.flat().sort((a, b) => Date.parse(b.receivedAt) - Date.parse(a.receivedAt));
  }

  private scopeEmail(candidate: EmailCandidate, email: string): EmailCandidate {
    return { ...candidate, account: email, sourceUrl: `https://mail.google.com/mail/u/?authuser=${encodeURIComponent(email)}#all/${encodeURIComponent(candidate.id)}` };
  }
}
