// Where events come from. Selected by CALENDAR_SOURCE: seed | google.
#pragma once

#include <string>
#include <vector>

#include "models/Models.hpp"

namespace enro {

class CalendarSource {
 public:
  virtual ~CalendarSource() = default;
  virtual std::string name() const = 0;
  // Events whose start is in [from_ts, to_ts).
  virtual std::vector<Event> events(const std::string& user_id, long long from_ts, long long to_ts) = 0;
};

}  // namespace enro
