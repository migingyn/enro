// Google Calendar API (CALENDAR_SOURCE=google). Owner: T03.
// Reads the primary calendar, next 7 days, recurring events expanded (singleEvents=true).
// No syncToken: Google rejects syncToken together with timeMin/timeMax, and a bounded 7-day window
// is a few hundred events at most, so a full window fetch per poll is simpler and always consistent.
#pragma once

#include <nlohmann/json.hpp>

#include "google/CalendarSource.hpp"
#include "google/GoogleAuth.hpp"

namespace enro {

class CalendarClient : public CalendarSource {
 public:
  CalendarClient(GoogleAuth& auth, Repos& repos);
  std::string name() const override { return "google"; }
  std::vector<Event> events(const std::string& user_id, long long from_ts, long long to_ts) override;

  static constexpr long long kWindowSeconds = 7 * 86400;

  // One events.list page -> Events the planner can act on. Drops all-day, past, out-of-window,
  // cancelled, declined-by-me, tentative (not a confirmed commitment), and unlocated events.
  static std::vector<Event> parseEvents(const nlohmann::json& page, const std::string& user_id, long long from_ts,
                                        long long to_ts);
  // "class" | "exam" | "interview" | "work" | "appointment" | "flight" | "casual", from keywords in the title.
  static std::string guessEventType(const std::string& title);

 private:
  GoogleAuth& auth_;
  Repos& repos_;
};

}  // namespace enro
