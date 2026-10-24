// Google Calendar API (CALENDAR_SOURCE=google). Incremental sync with syncToken stored in kv.
// Owner: T03.
#pragma once

#include "google/CalendarSource.hpp"
#include "google/GoogleAuth.hpp"

namespace enro {

class CalendarClient : public CalendarSource {
 public:
  CalendarClient(GoogleAuth& auth, Repos& repos);
  std::string name() const override { return "google"; }
  std::vector<Event> events(const std::string& user_id, long long from_ts, long long to_ts) override;

 private:
  GoogleAuth& auth_;
  Repos& repos_;
};

}  // namespace enro
