// Time helpers. All timestamps in Enro are Unix epoch seconds (UTC).
#pragma once

#include <string>

namespace enro::time {

long long nowTs();
long long nowMs();

// Local wall-clock helpers for a given IANA timezone (e.g. "America/Los_Angeles").
// "YYYY-MM-DD" of ts in tz.
std::string dateIn(long long ts, const std::string& tz);
// Epoch seconds of local midnight for date "YYYY-MM-DD" in tz.
long long midnightIn(const std::string& date, const std::string& tz);
// Epoch seconds of "HH:MM" on date in tz.
long long atLocalTime(const std::string& date, const std::string& hhmm, const std::string& tz);
// "HH:MM" of ts in tz.
std::string hhmmIn(long long ts, const std::string& tz);

}  // namespace enro::time
