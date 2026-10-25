#include "voice/TwilioProvider.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <regex>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include "util/Log.hpp"

namespace enro {

namespace {

const char* kApi = "https://api.twilio.com/2010-04-01/Accounts/";

long long nowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

std::string lastFour(const std::string& phone) { return phone.substr(phone.size() >= 4 ? phone.size() - 4 : 0); }

// SHA-1 (FIPS 180-1). Small enough to keep here instead of linking a crypto library.
std::array<uint8_t, 20> sha1(const std::string& msg) {
  uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
  std::string data = msg;
  uint64_t bit_len = static_cast<uint64_t>(msg.size()) * 8;
  data += static_cast<char>(0x80);
  while (data.size() % 64 != 56) data += '\0';
  for (int i = 7; i >= 0; --i) data += static_cast<char>((bit_len >> (i * 8)) & 0xFF);
  auto rol = [](uint32_t x, int n) { return (x << n) | (x >> (32 - n)); };
  for (size_t chunk = 0; chunk < data.size(); chunk += 64) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i) {
      const auto* p = reinterpret_cast<const uint8_t*>(&data[chunk + i * 4]);
      w[i] = (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
    }
    for (int i = 16; i < 80; ++i) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; ++i) {
      uint32_t f, k;
      if (i < 20) f = (b & c) | (~b & d), k = 0x5A827999;
      else if (i < 40) f = b ^ c ^ d, k = 0x6ED9EBA1;
      else if (i < 60) f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDC;
      else f = b ^ c ^ d, k = 0xCA62C1D6;
      uint32_t t = rol(a, 5) + f + e + k + w[i];
      e = d, d = c, c = rol(b, 30), b = a, a = t;
    }
    h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e;
  }
  std::array<uint8_t, 20> out{};
  for (int i = 0; i < 20; ++i) out[i] = static_cast<uint8_t>(h[i / 4] >> (24 - (i % 4) * 8));
  return out;
}

std::string toLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
  return s;
}

}  // namespace

// ---------------------------------------------------------------- helpers

namespace twilio {

std::string base64(const std::string& bytes) {
  static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  size_t i = 0;
  for (; i + 2 < bytes.size(); i += 3) {
    uint32_t n = (uint8_t(bytes[i]) << 16) | (uint8_t(bytes[i + 1]) << 8) | uint8_t(bytes[i + 2]);
    out += tbl[(n >> 18) & 63], out += tbl[(n >> 12) & 63], out += tbl[(n >> 6) & 63], out += tbl[n & 63];
  }
  if (size_t rest = bytes.size() - i) {
    uint32_t n = uint8_t(bytes[i]) << 16;
    if (rest == 2) n |= uint8_t(bytes[i + 1]) << 8;
    out += tbl[(n >> 18) & 63], out += tbl[(n >> 12) & 63];
    out += rest == 2 ? tbl[(n >> 6) & 63] : '=';
    out += '=';
  }
  return out;
}

std::string hmacSha1(const std::string& key, const std::string& message) {
  std::string k = key;
  if (k.size() > 64) {
    auto d = sha1(k);
    k.assign(d.begin(), d.end());
  }
  k.resize(64, '\0');
  std::string ipad(64, '\0'), opad(64, '\0');
  for (int i = 0; i < 64; ++i) ipad[i] = static_cast<char>(k[i] ^ 0x36), opad[i] = static_cast<char>(k[i] ^ 0x5c);
  auto inner = sha1(ipad + message);
  auto outer = sha1(opad + std::string(inner.begin(), inner.end()));
  return std::string(outer.begin(), outer.end());
}

std::map<std::string, std::string> parseForm(const std::string& body) {
  auto decode = [](const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
      if (s[i] == '+') out += ' ';
      else if (s[i] == '%' && i + 2 < s.size() && std::isxdigit(uint8_t(s[i + 1])) && std::isxdigit(uint8_t(s[i + 2]))) {
        out += static_cast<char>(std::stoi(s.substr(i + 1, 2), nullptr, 16));
        i += 2;
      } else out += s[i];
    }
    return out;
  };
  std::map<std::string, std::string> out;
  size_t start = 0;
  while (start <= body.size()) {
    size_t end = body.find('&', start);
    if (end == std::string::npos) end = body.size();
    std::string pair = body.substr(start, end - start);
    if (!pair.empty()) {
      size_t eq = pair.find('=');
      out[decode(pair.substr(0, eq))] = eq == std::string::npos ? "" : decode(pair.substr(eq + 1));
    }
    start = end + 1;
  }
  return out;
}

std::string eventForStatus(const std::string& s) {
  if (s == "no-answer" || s == "busy" || s == "failed" || s == "canceled") return "no_answer";
  if (s == "completed") return "completed";
  return "";  // queued | initiated | ringing | in-progress: nothing for the Conversation yet
}

bool isStop(const std::string& speech, const std::string& digits) {
  static const std::regex stop(R"(\b(stop|cancel|hang up|leave me alone)\b)");
  return digits == "0" || std::regex_search(toLower(speech), stop);
}

}  // namespace twilio

// ---------------------------------------------------------------- provider

TwilioProvider::TwilioProvider(const Env& env)
    : account_sid_(env.get("TWILIO_ACCOUNT_SID")),
      auth_token_(env.get("TWILIO_AUTH_TOKEN")),
      from_number_(env.get("TWILIO_FROM_NUMBER")),
      public_base_url_(env.get("PUBLIC_BASE_URL")) {
  while (!public_base_url_.empty() && public_base_url_.back() == '/') public_base_url_.pop_back();
  for (auto& p : readinessProblems()) LOG_WARN("twilio", "VOICE_PROVIDER=twilio but " << p);
}

std::vector<std::string> TwilioProvider::readinessProblems() const {
  std::vector<std::string> out;
  if (!std::regex_match(account_sid_, std::regex("AC[0-9a-fA-F]{32}")))
    out.push_back("TWILIO_ACCOUNT_SID is not an AC + 32 hex SID");
  if (auth_token_.size() < 16) out.push_back("TWILIO_AUTH_TOKEN is missing or too short");
  if (!std::regex_match(from_number_, std::regex(R"(\+[1-9]\d{7,14})")))
    out.push_back("TWILIO_FROM_NUMBER is not an E.164 number");
  if (public_base_url_.rfind("https://", 0) != 0) out.push_back("PUBLIC_BASE_URL does not start with https://");
  return out;
}

std::string TwilioProvider::placeCall(const CallRequest& req) {
  if (!readinessProblems().empty()) {
    LOG_ERROR("twilio", "not configured, call " << req.call_id << " not placed");
    return "TWILIO-UNCONFIGURED-" + req.call_id;
  }
  std::string base = public_base_url_;
  // One request, never retried: a timeout after sending is ambiguous (Twilio may have accepted the call),
  // and a blind retry would ring the user twice.
  cpr::Response r = cpr::Post(cpr::Url{std::string(kApi) + account_sid_ + "/Calls.json"},
                              cpr::Authentication{account_sid_, auth_token_, cpr::AuthMode::BASIC},
                              cpr::Payload{{"To", req.to_phone},
                                           {"From", from_number_},
                                           {"Url", base + "/twilio/voice?call_id=" + req.call_id},
                                           {"Method", "POST"},
                                           {"StatusCallback", base + "/twilio/status?call_id=" + req.call_id},
                                           {"StatusCallbackMethod", "POST"},
                                           {"StatusCallbackEvent", "initiated"},
                                           {"StatusCallbackEvent", "ringing"},
                                           {"StatusCallbackEvent", "answered"},
                                           {"StatusCallbackEvent", "completed"},
                                           {"Timeout", "25"},
                                           {"TimeLimit", "120"},
                                           {"Record", "false"}},
                              cpr::Timeout{15000});
  if (r.error.code == cpr::ErrorCode::OPERATION_TIMEDOUT) {
    LOG_ERROR("twilio", "call " << req.call_id << " UNCERTAIN: request timed out, Twilio may have placed it. Not retrying.");
    return "TWILIO-UNCERTAIN-" + req.call_id;
  }
  if (r.error || r.status_code / 100 != 2) {
    LOG_ERROR("twilio", "call " << req.call_id << " failed: HTTP " << r.status_code << " " << r.error.message << " "
                                << r.text.substr(0, 200));
    return "TWILIO-FAILED-" + req.call_id;
  }
  std::string sid = nlohmann::json::parse(r.text, nullptr, false).value("sid", "");
  {
    std::lock_guard<std::mutex> lock(mu_);
    calls_[req.call_id].sid = sid;
  }
  LOG_INFO("twilio", "ringing ..." << lastFour(req.to_phone) << " for " << req.call_id << " (" << sid << ")");
  return sid;
}

void TwilioProvider::say(const std::string& call_id, const std::string& text, const std::string& persona, Urgency,
                         const std::string& audio_url) {
  std::lock_guard<std::mutex> lock(mu_);
  calls_[call_id].queued.push_back({text, audio_url, persona});
}

void TwilioProvider::hangup(const std::string& call_id, const std::string& reason) {
  std::string sid;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto& c = calls_[call_id];
    if (c.in_turn) {  // inside a webhook: the <Hangup/> goes out with this turn's TwiML
      c.hangup = true;
      return;
    }
    sid = c.sid;
  }
  LOG_INFO("twilio", "hanging up " << call_id << " (" << reason << ")");
  if (!sid.empty()) hangupOverRest(call_id, sid);
}

void TwilioProvider::hangupOverRest(const std::string& call_id, const std::string& sid) {
  if (!readinessProblems().empty()) return;
  std::string url = std::string(kApi) + account_sid_ + "/Calls/" + sid + ".json";
  cpr::Authentication auth{account_sid_, auth_token_, cpr::AuthMode::BASIC};
  cpr::Response got = cpr::Get(cpr::Url{url}, auth, cpr::Timeout{10000});
  std::string status = nlohmann::json::parse(got.text, nullptr, false).value("status", "");
  // Terminal calls are left alone. Not yet answered -> canceled, live -> completed.
  if (status == "completed" || status == "canceled" || status == "busy" || status == "failed" || status == "no-answer")
    return;
  std::string next = status == "queued" || status == "ringing" ? "canceled" : "completed";
  cpr::Response r = cpr::Post(cpr::Url{url}, auth, cpr::Payload{{"Status", next}}, cpr::Timeout{10000});
  if (r.error || r.status_code / 100 != 2)
    LOG_WARN("twilio", "hangup " << call_id << " failed: HTTP " << r.status_code << " " << r.error.message);
}

std::string TwilioProvider::sendSms(const std::string& to_phone, const std::string& body) {
  if (!readinessProblems().empty()) {
    LOG_ERROR("twilio", "not configured, nothing sent to ..." << lastFour(to_phone));
    return "TWILIO-UNCONFIGURED-SMS";
  }
  cpr::Response r = cpr::Post(cpr::Url{std::string(kApi) + account_sid_ + "/Messages.json"},
                              cpr::Authentication{account_sid_, auth_token_, cpr::AuthMode::BASIC},
                              cpr::Payload{{"To", to_phone}, {"From", from_number_}, {"Body", body}},
                              cpr::Timeout{15000});
  if (r.error || r.status_code / 100 != 2) {
    LOG_ERROR("twilio", "SMS to ..." << lastFour(to_phone) << " failed: HTTP " << r.status_code << " "
                                    << r.error.message);
    return "TWILIO-FAILED-SMS";
  }
  LOG_INFO("twilio", "SMS to ..." << lastFour(to_phone) << " sent");
  return nlohmann::json::parse(r.text, nullptr, false).value("sid", "");
}

std::string TwilioProvider::signatureFor(const std::string& url,
                                         const std::map<std::string, std::string>& params) const {
  std::string data = url;
  for (auto& [k, v] : params) data += k + v;  // std::map iterates sorted by key
  return twilio::base64(twilio::hmacSha1(auth_token_, data));
}

bool TwilioProvider::validSignature(const std::string& url, const std::map<std::string, std::string>& params,
                                    const std::string& signature) const {
  if (auth_token_.empty() || signature.empty()) return false;
  std::string expected = signatureFor(url, params);
  if (expected.size() != signature.size()) return false;
  unsigned char diff = 0;  // constant time
  for (size_t i = 0; i < expected.size(); ++i) diff |= static_cast<unsigned char>(expected[i] ^ signature[i]);
  return diff == 0;
}

TwilioProvider::TurnOutput TwilioProvider::beginTurn(const std::string& call_id) {
  std::lock_guard<std::mutex> lock(mu_);
  auto& c = calls_[call_id];
  if (c.started_ms == 0) c.started_ms = nowMs();
  c.rounds += 1;
  c.in_turn = true;
  c.hangup = false;
  c.queued.clear();
  return {{}, false, c.rounds, nowMs() - c.started_ms};
}

TwilioProvider::TurnOutput TwilioProvider::endTurn(const std::string& call_id) {
  std::lock_guard<std::mutex> lock(mu_);
  auto& c = calls_[call_id];
  TurnOutput out{std::move(c.queued), c.hangup, c.rounds, nowMs() - c.started_ms};
  c.queued.clear();
  c.in_turn = false;
  c.hangup = false;
  return out;
}

void TwilioProvider::forget(const std::string& call_id) {
  std::lock_guard<std::mutex> lock(mu_);
  calls_.erase(call_id);
}

}  // namespace enro
