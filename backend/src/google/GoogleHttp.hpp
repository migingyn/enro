// One tiny HTTP seam for every Google call (OAuth, Calendar, Gmail, Routes). The real one uses cpr
// with timeouts; tests inject a fake that returns fixture JSON. Owner: T03.
#pragma once

#include <functional>
#include <map>
#include <string>

namespace enro {

struct HttpRequest {
  std::string method = "GET";  // GET | POST
  std::string url;
  std::map<std::string, std::string> headers;
  std::string body;
  int timeout_ms = 15000;
};

struct HttpResponse {
  int status = 0;  // 0 = network error / timeout
  std::string body;
};

using HttpClient = std::function<HttpResponse(const HttpRequest&)>;

// cpr-backed client. Never throws; a network failure comes back as status 0.
HttpClient cprHttpClient();

// application/x-www-form-urlencoded / query-string escaping (RFC 3986 unreserved kept as is).
std::string urlEncode(const std::string& s);
std::string formEncode(const std::map<std::string, std::string>& fields);

// Epoch seconds from an RFC 3339 timestamp ("2026-10-25T08:30:00-07:00", "...Z", fractional seconds ok).
// Returns -1 when the string has no explicit offset or is malformed.
long long parseRfc3339(const std::string& s);
// Epoch seconds as "YYYY-MM-DDTHH:MM:SSZ".
std::string toRfc3339(long long ts);
// Days since 1970-01-01 for a civil date (proleptic Gregorian).
long long daysFromCivil(int y, int m, int d);

}  // namespace enro
