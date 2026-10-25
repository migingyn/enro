#include "google/CalendarClient.hpp"

#include <algorithm>
#include <cctype>

#include "util/Log.hpp"
#include "util/Time.hpp"

namespace enro {

namespace {

constexpr const char* kEventsUrl = "https://www.googleapis.com/calendar/v3/calendars/primary/events";
constexpr int kMaxPages = 10;

bool hasWord(const std::string& lower, std::initializer_list<const char*> words) {
  return std::any_of(words.begin(), words.end(), [&](const char* w) { return lower.find(w) != std::string::npos; });
}

}  // namespace

CalendarClient::CalendarClient(GoogleAuth& auth, Repos& repos) : auth_(auth), repos_(repos) {}

std::string CalendarClient::guessEventType(const std::string& title) {
  std::string t = title;
  for (auto& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (hasWord(t, {"interview"})) return "interview";
  if (hasWord(t, {"exam", "midterm", "final", "quiz"})) return "exam";
  if (hasWord(t, {"flight", "airport"})) return "flight";
  if (hasWord(t, {"doctor", "dentist", "appointment", "appt", "clinic"})) return "appointment";
  if (hasWord(t, {"lecture", "class", "section", "discussion", "lab", "seminar", "cse", "math"})) return "class";
  if (hasWord(t, {"shift", "standup", "meeting", "work", "1:1", "sync"})) return "work";
  return "casual";
}

std::vector<Event> CalendarClient::parseEvents(const nlohmann::json& page, const std::string& user_id, long long from_ts,
                                               long long to_ts) {
  std::vector<Event> out;
  if (!page.contains("items") || !page["items"].is_array()) return out;
  for (auto& item : page["items"]) {
    if (!item.is_object() || !item.contains("id") || !item.contains("start")) continue;
    // start.date (no dateTime) = all-day: no leave-by time to plan for.
    std::string start = item["start"].value("dateTime", "");
    long long start_ts = parseRfc3339(start);
    if (start_ts < 0 || start_ts < from_ts || start_ts >= to_ts) continue;
    std::string status = item.value("status", "confirmed");
    if (status == "cancelled" || status == "tentative") continue;
    std::string location = item.value("location", "");
    location.erase(0, location.find_first_not_of(" \t\r\n"));
    if (location.empty()) continue;
    bool declined = false;
    if (item.contains("attendees") && item["attendees"].is_array()) {
      for (auto& a : item["attendees"]) {
        if (a.value("self", false) && a.value("responseStatus", "") == "declined") declined = true;
      }
    }
    if (declined) continue;

    Event e;
    e.id = "gcal:" + item["id"].get<std::string>();
    e.user_id = user_id;
    e.title = item.value("summary", "Untitled event");
    e.location = location;
    e.event_type = guessEventType(e.title);
    e.source = "calendar";
    e.start_ts = start_ts;
    long long end_ts = item.contains("end") ? parseRfc3339(item["end"].value("dateTime", "")) : -1;
    e.end_ts = end_ts > start_ts ? end_ts : start_ts + 3600;
    out.push_back(e);
  }
  return out;
}

std::vector<Event> CalendarClient::events(const std::string& user_id, long long from_ts, long long to_ts) {
  long long now = time::nowTs();
  long long lo = std::max(from_ts, now), hi = std::min(to_ts, now + kWindowSeconds);
  std::vector<Event> out;
  if (lo >= hi) return out;
  std::string page_token;
  for (int page = 0; page < kMaxPages; ++page) {
    std::string url = std::string(kEventsUrl) + "?" +
                      formEncode({{"timeMin", toRfc3339(lo)},
                                  {"timeMax", toRfc3339(hi)},
                                  {"singleEvents", "true"},
                                  {"orderBy", "startTime"},
                                  {"showDeleted", "false"},
                                  {"maxResults", "250"}});
    if (!page_token.empty()) url += "&pageToken=" + urlEncode(page_token);
    HttpResponse r = auth_.get(user_id, url);
    if (r.status != 200) {
      LOG_WARN("google", "calendar events.list failed for " << user_id << " (HTTP " << r.status << ")");
      return out;
    }
    auto j = nlohmann::json::parse(r.body, nullptr, false);
    if (j.is_discarded()) return out;
    auto batch = parseEvents(j, user_id, lo, hi);
    out.insert(out.end(), batch.begin(), batch.end());
    page_token = j.value("nextPageToken", "");
    if (page_token.empty()) break;
  }
  LOG_INFO("google", "calendar: " << out.size() << " plannable events for " << user_id);
  return out;
}

}  // namespace enro
