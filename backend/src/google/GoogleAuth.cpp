#include "google/GoogleAuth.hpp"

#include <random>

#include "util/Log.hpp"
#include "util/Time.hpp"

namespace enro {

namespace {

constexpr const char* kAuthEndpoint = "https://accounts.google.com/o/oauth2/v2/auth";
constexpr const char* kTokenEndpoint = "https://oauth2.googleapis.com/token";

std::string randomState() {
  static thread_local std::mt19937_64 rng{std::random_device{}()};
  static const char* hex = "0123456789abcdef";
  std::string s;
  for (int i = 0; i < 32; ++i) s += hex[rng() & 15];
  return s;
}

}  // namespace

GoogleAuth::GoogleAuth(const Env& env, Repos& repos, HttpClient http)
    : client_id_(env.get("GOOGLE_CLIENT_ID")),
      client_secret_(env.get("GOOGLE_CLIENT_SECRET")),
      base_url_(env.get("PUBLIC_BASE_URL", "http://localhost:" + std::to_string(env.port()))),
      repos_(repos),
      http_(http ? std::move(http) : cprHttpClient()) {}

std::string GoogleAuth::redirectUri() const { return base_url_ + "/oauth/google/callback"; }

std::string GoogleAuth::authUrl(const std::string& user_id) {
  std::string state = randomState();
  {
    std::lock_guard<std::mutex> lock(mu_);
    long long now = time::nowTs();
    std::erase_if(states_, [&](auto& kv) { return kv.second.expires_at <= now; });
    states_[state] = {user_id, now + kStateTtlSeconds};
  }
  if (!configured()) return redirectUri() + "?code=fake&state=" + state;
  return std::string(kAuthEndpoint) + "?" +
         formEncode({{"client_id", client_id_},
                     {"redirect_uri", redirectUri()},
                     {"response_type", "code"},
                     {"scope", kScopes},
                     {"access_type", "offline"},
                     {"prompt", "consent"},
                     {"include_granted_scopes", "true"},
                     {"state", state}});
}

std::optional<std::string> GoogleAuth::consumeState(const std::string& state) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = states_.find(state);
  if (it == states_.end()) return std::nullopt;
  PendingState p = it->second;
  states_.erase(it);  // single use
  if (p.expires_at <= time::nowTs()) return std::nullopt;
  return p.user_id;
}

std::optional<nlohmann::json> GoogleAuth::tokenRequest(const std::string& user_id,
                                                       std::map<std::string, std::string> fields) {
  fields["client_id"] = client_id_;
  fields["client_secret"] = client_secret_;
  HttpResponse r = http_({"POST", kTokenEndpoint, {{"Content-Type", "application/x-www-form-urlencoded"}},
                          formEncode(fields), 15000});
  if (r.status != 200) {
    // Never log the body: it can echo the code or tokens.
    LOG_WARN("google", "token request failed for " << user_id << " (HTTP " << r.status << ")");
    return std::nullopt;
  }
  auto j = nlohmann::json::parse(r.body, nullptr, false);
  if (j.is_discarded() || !j.contains("access_token") || j.value("expires_in", 0) <= 0) {
    LOG_WARN("google", "invalid token response for " << user_id);
    return std::nullopt;
  }
  std::lock_guard<std::mutex> lock(mu_);
  tokens_[user_id] = {j["access_token"].get<std::string>(), time::nowTs() + j["expires_in"].get<long long>()};
  return j;
}

bool GoogleAuth::handleCallback(const std::string& code, const std::string& user_id) {
  if (!configured()) {
    LOG_INFO("google", "fake OAuth callback for " << user_id << " (Google not configured)");
    return true;
  }
  auto j = tokenRequest(user_id, {{"code", code}, {"grant_type", "authorization_code"}, {"redirect_uri", redirectUri()}});
  if (!j) return false;
  if (j->contains("refresh_token")) {
    repos_.saveRefreshToken(user_id, "google", (*j)["refresh_token"].get<std::string>());
  } else if (!repos_.refreshToken(user_id, "google")) {
    // prompt=consent should always return one; without it access dies in an hour.
    LOG_WARN("google", "no refresh token for " << user_id << "; revoke access and reconnect");
  }
  LOG_INFO("google", "connected Google for " << user_id);
  return true;
}

std::optional<std::string> GoogleAuth::accessToken(const std::string& user_id, bool force_refresh) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = tokens_.find(user_id);
    // 60 s of slack so a token doesn't expire mid-request.
    if (!force_refresh && it != tokens_.end() && it->second.expires_at > time::nowTs() + 60) return it->second.token;
  }
  if (!configured()) return std::nullopt;
  auto refresh = repos_.refreshToken(user_id, "google");
  if (!refresh) return std::nullopt;
  auto j = tokenRequest(user_id, {{"grant_type", "refresh_token"}, {"refresh_token", *refresh}});
  if (!j) return std::nullopt;
  // Google may rotate the refresh token.
  if (j->contains("refresh_token")) repos_.saveRefreshToken(user_id, "google", (*j)["refresh_token"].get<std::string>());
  return (*j)["access_token"].get<std::string>();
}

HttpResponse GoogleAuth::get(const std::string& user_id, const std::string& url) {
  for (int attempt = 0; attempt < 2; ++attempt) {
    auto token = accessToken(user_id, attempt == 1);
    if (!token) return {401, ""};
    HttpResponse r = http_({"GET", url, {{"Authorization", "Bearer " + *token}}, "", 15000});
    if (r.status == 401 && attempt == 0) continue;
    return r;
  }
  return {401, ""};
}

}  // namespace enro
