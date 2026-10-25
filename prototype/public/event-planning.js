export function upcomingEvents(events, now = Date.now()) {
  return events.filter(event => event.status !== 'cancelled' && event.location?.trim() && Date.parse(event.startAt) > now)
    .sort((a, b) => Date.parse(a.startAt) - Date.parse(b.startAt));
}

export function isTravelEvent(event) {
  return event.travel?.kind !== 'online';
}

export function matchingPlan(plans, event, settings, now = Date.now()) {
  return plans.find(plan => plan.event.id === event.id && plan.event.status === 'confirmed'
    && plan.route?.provider === 'google-routes'
    && plan.event.startAt === event.startAt && plan.event.location === event.location
    && plan.origin === settings.origin && plan.travelMode === settings.travelMode
    && plan.preparationMinutes === settings.preparationMinutes && plan.bufferMinutes === settings.bufferMinutes
    && (plan.accessMinutes ?? 0) === (settings.accessMinutes ?? 0)
    && Date.parse(plan.route.fetchedAt) >= now - 15 * 60_000
    && Date.parse(plan.route.fetchedAt) <= now + 2 * 60_000
    && Date.parse(plan.leaveAt) > now);
}

// Retain each successful plan if another route fails. A retry reuses those results.
export async function calculateSelected({ ids, state, settings, api, now = Date.now() }) {
  const results = [], errors = [];
  const eligible = upcomingEvents(state.events, now);
  for (const id of new Set(ids)) {
    const event = eligible.find(item => item.id === id);
    if (!event) { errors.push({ id, message: 'This event changed or is no longer upcoming. Refresh and select it again.' }); continue; }
    if (!isTravelEvent(event)) { errors.push({ id, message: `${event.title}: this online event has no journey to plan.` }); continue; }
    try {
      if (event.status === 'tentative') {
        if (!event.id.startsWith('email-ics:')) throw new Error('Confirm attendance in your calendar first.');
        await api(`/api/events/${encodeURIComponent(event.id)}/confirm`, { method: 'POST', body: JSON.stringify({ attending: true }) });
      }
      const existing = matchingPlan(state.plans, event, settings, now);
      const plan = existing || await api('/api/plans', { method: 'POST', body: JSON.stringify({ eventId: id, ...settings }) });
      results.push(plan);
    } catch (error) { errors.push({ id, message: `${event.title}: ${error.message || 'Travel estimate unavailable.'}` }); }
  }
  return { results, errors };
}
