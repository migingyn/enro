#include "google/CalendarClient.hpp"

#include "util/Log.hpp"

namespace enro {

CalendarClient::CalendarClient(GoogleAuth& auth, Repos& repos) : auth_(auth), repos_(repos) {}

std::vector<Event> CalendarClient::events(const std::string& user_id, long long, long long) {
  // STUB
  // TODO(T03): GET calendar/v3/calendars/primary/events with timeMin/timeMax (first sync) or syncToken
  // (kv "gcal_sync:<user>"), map to Event (source "calendar", event_type guessed from title), handle 410 Gone.
  LOG_WARN("google", "CalendarClient not implemented yet (T03), no events for " << user_id);
  return {};
}

}  // namespace enro
