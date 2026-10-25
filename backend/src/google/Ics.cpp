#include "google/Ics.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <map>
#include <set>

#include "google/GoogleHttp.hpp"
#include "util/Time.hpp"

namespace enro {

namespace {

struct Prop {
  std::string name;
  std::map<std::string, std::string> params;
  std::string value;
};

std::string upper(std::string s) {
  for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}

std::string trim(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
  return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

// RFC 5545 3.1: a line starting with space or tab continues the previous one.
std::vector<std::string> unfold(const std::string& text) {
  std::vector<std::string> lines;
  size_t i = 0;
  while (i <= text.size()) {
    size_t nl = text.find('\n', i);
    std::string line = text.substr(i, nl == std::string::npos ? std::string::npos : nl - i);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (!line.empty() && (line[0] == ' ' || line[0] == '\t') && !lines.empty()) lines.back() += line.substr(1);
    else lines.push_back(line);
    if (nl == std::string::npos) break;
    i = nl + 1;
  }
  return lines;
}

// NAME;P1=V1;P2="V;2":value. Colons and semicolons inside quotes don't split.
bool parseProp(const std::string& line, Prop& out) {
  bool quoted = false;
  size_t colon = std::string::npos;
  for (size_t i = 0; i < line.size(); ++i) {
    if (line[i] == '"') quoted = !quoted;
    else if (line[i] == ':' && !quoted) {
      colon = i;
      break;
    }
  }
  if (colon == std::string::npos) return false;
  std::string head = line.substr(0, colon);
  out.value = line.substr(colon + 1);
  std::vector<std::string> parts;
  std::string cur;
  quoted = false;
  for (char c : head) {
    if (c == '"') quoted = !quoted;
    if (c == ';' && !quoted) {
      parts.push_back(cur);
      cur.clear();
    } else {
      cur += c;
    }
  }
  parts.push_back(cur);
  out.name = upper(trim(parts[0]));
  out.params.clear();
  for (size_t i = 1; i < parts.size(); ++i) {
    auto eq = parts[i].find('=');
    if (eq == std::string::npos) continue;
    std::string v = parts[i].substr(eq + 1);
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
    out.params[upper(trim(parts[i].substr(0, eq)))] = v;
  }
  return true;
}

std::string unescapeText(const std::string& s) {
  std::string out;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      char n = s[++i];
      out += (n == 'n' || n == 'N') ? '\n' : n;
    } else {
      out += s[i];
    }
  }
  return trim(out);
}

bool knownZone(const std::string& tz) {
  if (tz.empty() || tz.find("..") != std::string::npos || tz[0] == '/') return false;
  if (tz == "UTC" || tz == "Etc/UTC") return true;
  std::error_code ec;
  return std::filesystem::is_regular_file("/usr/share/zoneinfo/" + tz, ec);
}

// Civil seconds (wall clock read as if it were UTC) of instant ts in tz.
long long civilIn(long long ts, const std::string& tz) {
  std::string date = time::dateIn(ts, tz), hm = time::hhmmIn(ts, tz);
  int y = 0, mo = 0, d = 0, h = 0, mi = 0;
  std::sscanf(date.c_str(), "%d-%d-%d", &y, &mo, &d);
  std::sscanf(hm.c_str(), "%d:%d", &h, &mi);
  long long sec = ((ts % 60) + 60) % 60;
  return daysFromCivil(y, mo, d) * 86400 + h * 3600LL + mi * 60LL + sec;
}

// Parses a DATE-TIME property into an instant. Sets reason and returns -1 when it would be a guess.
long long resolveDateTime(const Prop& p, std::string& reason, std::string& tzid_out) {
  auto vt = p.params.find("VALUE");
  if ((vt != p.params.end() && upper(vt->second) == "DATE") || p.value.size() == 8) {
    reason = "all_day";
    return -1;
  }
  int y, mo, d, h, mi, s, n = 0;
  if (std::sscanf(p.value.c_str(), "%4d%2d%2dT%2d%2d%2d%n", &y, &mo, &d, &h, &mi, &s, &n) != 6 || n != 15) {
    reason = "malformed_time";
    return -1;
  }
  bool utc = p.value.size() == 16 && (p.value[15] == 'Z' || p.value[15] == 'z');
  if (p.value.size() != 15 && !utc) {
    reason = "malformed_time";
    return -1;
  }
  if (mo < 1 || mo > 12 || d < 1 || h > 23 || mi > 59 || s > 59 ||
      d > (mo == 2 ? ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0 ? 29 : 28)
                   : (mo == 4 || mo == 6 || mo == 9 || mo == 11 ? 30 : 31))) {
    reason = "malformed_time";
    return -1;
  }
  auto tz = p.params.find("TZID");
  if (utc) {
    if (tz != p.params.end()) {
      reason = "conflicting_timezone";
      return -1;
    }
    tzid_out = "UTC";
    return daysFromCivil(y, mo, d) * 86400 + h * 3600LL + mi * 60LL + s;
  }
  if (tz == p.params.end()) {
    reason = "floating_time";  // no safe implicit timezone
    return -1;
  }
  long long ts = resolveLocalTime(y, mo, d, h, mi, s, tz->second);
  if (ts < 0) {
    reason = knownZone(tz->second) ? "ambiguous_time" : "unknown_timezone";
    return -1;
  }
  tzid_out = tz->second;
  return ts;
}

IcsInvite buildInvite(const std::vector<Prop>& props, const std::string& method) {
  IcsInvite inv;
  inv.method = method;
  std::map<std::string, std::vector<const Prop*>> by;
  for (auto& p : props) by[p.name].push_back(&p);
  auto first = [&](const std::string& n) -> std::string { return by.count(n) ? unescapeText(by[n][0]->value) : ""; };

  inv.uid = first("UID");
  inv.title = first("SUMMARY");
  inv.location = first("LOCATION");
  std::string seq = first("SEQUENCE");
  if (!seq.empty()) inv.sequence = std::all_of(seq.begin(), seq.end(), ::isdigit) ? std::stoi(seq) : -1;

  if (method == "CANCEL" || upper(first("STATUS")) == "CANCELLED") {
    inv.status = "cancel";
    return inv;
  }
  inv.status = "review";
  if (inv.uid.empty() || by["UID"].size() != 1 || inv.sequence < 0) {
    inv.reason = "bad_uid_or_sequence";
    return inv;
  }
  for (const char* r : {"RRULE", "RDATE", "EXDATE", "EXRULE", "RECURRENCE-ID"}) {
    if (by.count(r)) {
      inv.reason = "recurring";
      return inv;
    }
  }
  if (inv.title.empty() || inv.location.empty()) {
    inv.reason = "incomplete";
    return inv;
  }
  if (by["DTSTART"].size() != 1) {
    inv.reason = "incomplete";
    return inv;
  }
  long long start = resolveDateTime(*by["DTSTART"][0], inv.reason, inv.tzid);
  if (start < 0) return inv;
  inv.start_ts = start;
  inv.end_ts = start + 3600;
  if (by.count("DTEND")) {
    std::string r, tz;
    long long end = resolveDateTime(*by["DTEND"][0], r, tz);
    if (end > start) inv.end_ts = end;
  }
  inv.reason.clear();
  inv.status = "event";
  return inv;
}

}  // namespace

long long resolveLocalTime(int y, int mo, int d, int h, int mi, int s, const std::string& tzid) {
  if (!knownZone(tzid)) return -1;
  long long civil = daysFromCivil(y, mo, d) * 86400 + h * 3600LL + mi * 60LL + s;
  // Collect offsets on both sides of any nearby transition, then round-trip every candidate.
  // Two matches = repeated (fall-back) hour; zero = skipped (spring-forward) hour.
  std::set<long long> offsets, matches;
  for (int hours : {-48, -24, -12, 0, 12, 24, 48}) {
    long long sample = civil + hours * 3600LL;
    offsets.insert(civilIn(sample, tzid) - sample);
  }
  for (long long off : offsets) {
    long long t = civil - off;
    if (civilIn(t, tzid) == civil) matches.insert(t);
  }
  return matches.size() == 1 ? *matches.begin() : -1;
}

std::vector<IcsInvite> parseIcs(const std::string& text) {
  std::vector<IcsInvite> out;
  std::string method;
  bool in_cal = false, in_event = false;
  int nested = 0;  // VALARM etc. inside a VEVENT
  std::vector<Prop> props;
  for (auto& line : unfold(text)) {
    Prop p;
    if (!parseProp(line, p)) continue;
    std::string v = upper(trim(p.value));
    if (p.name == "BEGIN" && v == "VCALENDAR") {
      in_cal = true;
      method.clear();
    } else if (p.name == "END" && v == "VCALENDAR") {
      in_cal = false;
    } else if (!in_cal) {
      continue;
    } else if (p.name == "BEGIN" && v == "VEVENT" && !in_event) {
      in_event = true;
      nested = 0;
      props.clear();
    } else if (p.name == "END" && v == "VEVENT" && in_event && nested == 0) {
      in_event = false;
      // Only organizer-side messages describe the event. REPLY, COUNTER, REFRESH... don't.
      if (method.empty() || method == "REQUEST" || method == "PUBLISH" || method == "CANCEL") {
        out.push_back(buildInvite(props, method));
      }
    } else if (in_event && p.name == "BEGIN") {
      ++nested;
    } else if (in_event && p.name == "END") {
      --nested;
    } else if (in_event && nested == 0) {
      props.push_back(p);
    } else if (!in_event && p.name == "METHOD") {
      method = v;
    }
  }
  return out;
}

std::vector<IcsInvite> latestRevisions(const std::vector<IcsInvite>& invites) {
  std::map<std::string, IcsInvite> best;
  for (auto& inv : invites) {
    if (inv.uid.empty()) continue;
    auto it = best.find(inv.uid);
    if (it == best.end() || inv.sequence > it->second.sequence ||
        (inv.sequence == it->second.sequence && inv.status == "cancel")) {
      best[inv.uid] = inv;
    }
  }
  std::vector<IcsInvite> out;
  for (auto& [_, inv] : best) out.push_back(inv);
  return out;
}

std::string icsEventId(const std::string& uid) {
  unsigned long long h = 1469598103934665603ULL;
  for (unsigned char c : uid) {
    h ^= c;
    h *= 1099511628211ULL;
  }
  char buf[17];
  std::snprintf(buf, sizeof buf, "%016llx", h);
  return std::string("email-ics:") + buf;
}

}  // namespace enro
