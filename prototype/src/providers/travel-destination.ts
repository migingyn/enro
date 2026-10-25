import type { EnroEvent } from '../types.js';

type LocatedEvent = Pick<EnroEvent, 'location' | 'source'>;

// UCSD's classroom directory identifies these building codes and rooms:
// https://mathweb.ucsd.edu/~gptesler/assigningseats.html
const ucsdBuildings: Record<string, string> = {
  WLH: 'Warren Lecture Hall',
  COA: 'Coalition',
  PCYNH: 'Pepper Canyon Hall',
};

export function isOnlineLocation(location: string): boolean {
  const value = location.trim();
  if (/^(?:online|virtual|remote|zoom)(?:\s+(?:event|meeting|session|only))?$/i.test(value)) return true;
  try {
    const host = new URL(value).hostname.toLowerCase();
    return host === 'zoom.us' || host.endsWith('.zoom.us') || host === 'meet.google.com'
      || host === 'teams.microsoft.com' || host === 'teams.live.com';
  } catch { return false; }
}

export function travelContext(event: LocatedEvent): { kind: 'online' } | { kind: 'in-person'; destination: string } {
  const location = event.location.trim();
  if (isOnlineLocation(location)) return { kind: 'online' };
  if (event.source.account?.toLowerCase().endsWith('@ucsd.edu')) {
    const code = /^(WLH|COA|PCYNH)(?:\s+[A-Z]?\d+[A-Z]?(?:-\d+[A-Z]?)?)?$/i.exec(location)?.[1]?.toUpperCase();
    if (code && ucsdBuildings[code]) return { kind: 'in-person', destination: `${ucsdBuildings[code]}, UC San Diego, La Jolla, CA` };
  }
  return { kind: 'in-person', destination: location };
}

export function destinationForTravel(event: LocatedEvent): string {
  const context = travelContext(event);
  if (context.kind === 'online') throw new Error('This event is online and has no travel destination. Choose an in-person event to plan a journey.');
  return context.destination;
}
