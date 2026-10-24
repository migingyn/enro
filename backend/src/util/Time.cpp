#include "util/Time.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <mutex>

namespace enro::time {

long long nowTs() {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

long long nowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
      .count();
}

namespace {

// TZ switching is process-global, so serialize it. Fine for the hackathon's call volume.
std::mutex tz_mu;

struct TzScope {
  std::string old;
  bool had = false;
  explicit TzScope(const std::string& tz) {
    if (const char* o = std::getenv("TZ")) {
      old = o;
      had = true;
    }
    setenv("TZ", tz.c_str(), 1);
    tzset();
  }
  ~TzScope() {
    if (had) setenv("TZ", old.c_str(), 1);
    else unsetenv("TZ");
    tzset();
  }
};

std::tm localIn(long long ts, const std::string& tz) {
  std::lock_guard<std::mutex> lock(tz_mu);
  TzScope scope(tz);
  std::time_t t = static_cast<std::time_t>(ts);
  std::tm tm{};
  localtime_r(&t, &tm);
  return tm;
}

}  // namespace

std::string dateIn(long long ts, const std::string& tz) {
  std::tm tm = localIn(ts, tz);
  char buf[16];
  std::snprintf(buf, sizeof buf, "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
  return buf;
}

std::string hhmmIn(long long ts, const std::string& tz) {
  std::tm tm = localIn(ts, tz);
  char buf[8];
  std::snprintf(buf, sizeof buf, "%02d:%02d", tm.tm_hour, tm.tm_min);
  return buf;
}

long long atLocalTime(const std::string& date, const std::string& hhmm, const std::string& tz) {
  int y = 0, mo = 0, d = 0, h = 0, mi = 0;
  std::sscanf(date.c_str(), "%d-%d-%d", &y, &mo, &d);
  std::sscanf(hhmm.c_str(), "%d:%d", &h, &mi);
  std::lock_guard<std::mutex> lock(tz_mu);
  TzScope scope(tz);
  std::tm tm{};
  tm.tm_year = y - 1900;
  tm.tm_mon = mo - 1;
  tm.tm_mday = d;
  tm.tm_hour = h;
  tm.tm_min = mi;
  tm.tm_isdst = -1;
  return static_cast<long long>(std::mktime(&tm));
}

long long midnightIn(const std::string& date, const std::string& tz) { return atLocalTime(date, "00:00", tz); }

}  // namespace enro::time
