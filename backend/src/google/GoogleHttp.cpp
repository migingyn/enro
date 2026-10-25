#include "google/GoogleHttp.hpp"

#include <cpr/cpr.h>

#include <cctype>
#include <cstdio>
#include <ctime>

namespace enro {

HttpClient cprHttpClient() {
  return [](const HttpRequest& req) -> HttpResponse {
    cpr::Header headers;
    for (auto& [k, v] : req.headers) headers[k] = v;
    cpr::Response r = req.method == "POST"
                          ? cpr::Post(cpr::Url{req.url}, headers, cpr::Body{req.body}, cpr::Timeout{req.timeout_ms})
                          : cpr::Get(cpr::Url{req.url}, headers, cpr::Timeout{req.timeout_ms});
    return {static_cast<int>(r.status_code), r.text};
  };
}

std::string urlEncode(const std::string& s) {
  static const char* hex = "0123456789ABCDEF";
  std::string out;
  for (unsigned char c : s) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += static_cast<char>(c);
    } else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 15];
    }
  }
  return out;
}

std::string formEncode(const std::map<std::string, std::string>& fields) {
  std::string out;
  for (auto& [k, v] : fields) {
    if (!out.empty()) out += '&';
    out += urlEncode(k) + '=' + urlEncode(v);
  }
  return out;
}

long long daysFromCivil(int y, int m, int d) {
  // Howard Hinnant's algorithm.
  y -= m <= 2;
  const long long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<long long>(doe) - 719468;
}

long long parseRfc3339(const std::string& s) {
  int y, mo, d, h, mi, sec, n = 0;
  if (std::sscanf(s.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d%n", &y, &mo, &d, &h, &mi, &sec, &n) != 6 || n != 19) return -1;
  if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || sec > 60) return -1;
  size_t i = 19;
  if (i < s.size() && s[i] == '.') {
    ++i;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
  }
  if (i >= s.size()) return -1;  // no offset: a floating time is never guessed
  long long offset = 0;
  if (s[i] == 'Z' || s[i] == 'z') {
    if (i + 1 != s.size()) return -1;
  } else if (s[i] == '+' || s[i] == '-') {
    int oh, om;
    if (s.size() != i + 6 || std::sscanf(s.c_str() + i + 1, "%2d:%2d", &oh, &om) != 2) return -1;
    offset = (oh * 3600LL + om * 60LL) * (s[i] == '-' ? -1 : 1);
  } else {
    return -1;
  }
  return daysFromCivil(y, mo, d) * 86400 + h * 3600LL + mi * 60LL + sec - offset;
}

std::string toRfc3339(long long ts) {
  std::time_t t = static_cast<std::time_t>(ts);
  std::tm tm{};
  gmtime_r(&t, &tm);
  char buf[32];
  std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buf;
}

}  // namespace enro
