import type { RouteEstimate, TravelMode } from '../types.js';
import type { Coordinates } from '../core/departure.js';
let lastOsrmRequestAt = 0;

/** Explicit driving-only prototype provider. No traffic, no silent fallback. */
export async function estimateOsrmDrive(origin: Coordinates, destination: Coordinates, fetcher: typeof fetch = fetch): Promise<RouteEstimate> {
  for (const point of [origin, destination]) {
    if (!Number.isFinite(point.latitude) || Math.abs(point.latitude) > 90 || !Number.isFinite(point.longitude) || Math.abs(point.longitude) > 180) throw new Error('Valid landmark coordinates are required.');
  }
  const points = [origin, destination].map(p => `${p.longitude},${p.latitude}`).join(';');
  if (fetcher === fetch) {
    if (Date.now() - lastOsrmRequestAt < 1000) throw new Error('Wait one second before another public OSRM request.');
    lastOsrmRequestAt = Date.now();
  }
  const response = await fetcher(`https://router.project-osrm.org/route/v1/driving/${points}?overview=false`, {
    headers: { 'User-Agent': 'Enro hackathon prototype' }, signal: AbortSignal.timeout(15000),
  });
  if (!response.ok) throw new Error(`OSRM routing unavailable (HTTP ${response.status}).`);
  const data = await response.json() as { code?: string; routes?: { duration: number; distance: number }[]; waypoints?: {distance: number}[] };
  const route = data.routes?.[0];
  if (data.code !== 'Ok' || !route || !Number.isFinite(route.duration) || route.duration <= 0 || !Number.isFinite(route.distance) || route.distance <= 0
    || data.waypoints?.length !== 2 || data.waypoints.some(p => !Number.isFinite(p.distance) || p.distance > 500)) throw new Error('No usable driving route was returned for these landmarks.');
  return { provider: 'osrm', durationSeconds: Math.ceil(route.duration), distanceMeters: route.distance, fetchedAt: new Date().toISOString() };
}

interface RouteInput {
  apiKey: string; origin: string; destination: string; travelMode: TravelMode; arrivalAt: string;
}
interface RouteResponse {
  duration?: string; distanceMeters?: number;
  legs?: Array<{ steps?: Array<{
    staticDuration?: string; travelMode?: string;
    transitDetails?: { stopDetails?: { departureTime?: string } };
  }> }>;
}

// https://developers.google.com/maps/documentation/routes/reference/rest/v2/TopLevel/computeRoutes
// arrivalTime only applies to TRANSIT. DRIVE uses up to three traffic-aware
// departure estimates and retains the longest duration as a conservative approximation.
export async function estimateRoute(input: RouteInput, fetcher: typeof fetch = fetch): Promise<RouteEstimate> {
  if (!input.apiKey?.trim()) throw new Error('Google Routes API key is not configured.');
  if (!input.origin.trim() || !input.destination.trim()) throw new Error('Origin and destination are required.');
  if (!['DRIVE', 'WALK', 'TRANSIT'].includes(input.travelMode)) throw new Error('Unsupported travel mode.');
  const arrival = Date.parse(input.arrivalAt);
  if (!Number.isFinite(arrival) || !/(Z|[+-]\d{2}:\d{2})$/i.test(input.arrivalAt)) {
    throw new Error('Arrival time must include a valid date and timezone offset.');
  }
  if (arrival <= Date.now()) throw new Error('Arrival time must be in the future.');

  const request = async (departure?: number): Promise<RouteResponse> => {
    const body: Record<string, unknown> = {
      origin: { address: input.origin.trim() }, destination: { address: input.destination.trim() },
      travelMode: input.travelMode, computeAlternativeRoutes: false,
    };
    let fieldMask = 'routes.duration,routes.distanceMeters';
    if (input.travelMode === 'DRIVE') {
      body.routingPreference = 'TRAFFIC_AWARE_OPTIMAL';
      body.departureTime = new Date(departure ?? arrival).toISOString();
    } else if (input.travelMode === 'TRANSIT') {
      body.arrivalTime = new Date(arrival).toISOString();
      fieldMask += ',routes.legs.steps.staticDuration,routes.legs.steps.travelMode,routes.legs.steps.transitDetails.stopDetails.departureTime';
    }
    const response = await fetcher('https://routes.googleapis.com/directions/v2:computeRoutes', {
      method: 'POST', headers: {
        'Content-Type': 'application/json', 'X-Goog-Api-Key': input.apiKey, 'X-Goog-FieldMask': fieldMask,
      }, body: JSON.stringify(body), signal: AbortSignal.timeout(20_000),
    });
    if (!response.ok) throw new Error(`Google Routes failed (HTTP ${response.status}). Check key restrictions, API enablement, and billing.`);
    const data = await response.json() as { routes?: RouteResponse[]; fallbackInfo?: unknown };
    if (!data.routes?.length) throw new Error('Google Routes found no route for this trip.');
    return data.routes[0]!;
  };

  let route = await request();
  let seconds = duration(route.duration);
  if (input.travelMode === 'DRIVE') {
    for (let attempt = 0; attempt < 2; attempt++) {
      const departure = arrival - seconds * 1000;
      if (departure <= Date.now()) throw new Error('The estimated departure time has already passed. Choose a later arrival.');
      const refined = await request(departure);
      const refinedSeconds = duration(refined.duration);
      route = refined;
      const difference = Math.abs(refinedSeconds - seconds);
      seconds = Math.max(seconds, refinedSeconds);
      if (difference <= 60) break;
    }
  } else if (input.travelMode === 'TRANSIT') {
    // A train might arrive well before the requested deadline. Subtracting just
    // route.duration from the deadline could miss it. Work from its actual departure.
    let walkingSeconds = 0;
    let foundTransit = false;
    for (const step of route.legs?.flatMap(leg => leg.steps ?? []) ?? []) {
      if (step.travelMode === 'TRANSIT' || step.transitDetails) {
        const leaves = Date.parse(step.transitDetails?.stopDetails?.departureTime ?? '');
        if (!Number.isFinite(leaves)) throw new Error('Transit route omitted a scheduled departure time.');
        seconds = Math.max(seconds, Math.ceil((arrival - leaves) / 1000) + walkingSeconds);
        foundTransit = true;
        break;
      }
      walkingSeconds += duration(step.staticDuration);
    }
    if (!foundTransit) throw new Error('Transit route did not include a scheduled transit leg.');
  }
  if (arrival - seconds * 1000 <= Date.now()) throw new Error('The estimated departure time has already passed. Choose a later arrival.');
  return {
    durationSeconds: Math.ceil(seconds),
    ...(Number.isFinite(route.distanceMeters) ? { distanceMeters: route.distanceMeters } : {}),
    fetchedAt: new Date().toISOString(), provider: 'google-routes',
  };
}

function duration(value: string | undefined): number {
  if (!value || !/^\d+(?:\.\d{1,9})?s$/.test(value)) throw new Error('Google Routes returned an invalid duration.');
  const seconds = Number(value.slice(0, -1));
  if (!Number.isFinite(seconds) || seconds < 0) throw new Error('Google Routes returned an invalid duration.');
  return seconds;
}
