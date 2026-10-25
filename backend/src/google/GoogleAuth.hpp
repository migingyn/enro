// Google OAuth 2.0 (Calendar + Gmail read-only scopes). Refresh tokens stay server side in
// the oauth_tokens table (PRD 13). REST through cpr. Owner: T03.
// Setup: a "Web application" OAuth client whose redirect URI is redirectUri() exactly.
#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <string>

#include "config/Env.hpp"
#include "google/GoogleHttp.hpp"
#include "store/Repos.hpp"

namespace enro {

class GoogleAuth {
 public:
  // http defaults to cprHttpClient(); tests inject a fake.
  GoogleAuth(const Env& env, Repos& repos, HttpClient http = {});

  static constexpr const char* kScopes =
      "https://www.googleapis.com/auth/calendar.events.readonly https://www.googleapis.com/auth/gmail.readonly";
  static constexpr long long kStateTtlSeconds = 600;

  // URL to send the browser to. Carries a random, expiring, single-use state that maps back to user_id.
  // In fake mode (not configured) it points straight at our own callback with a fake code.
  std::string authUrl(const std::string& user_id);
  // The user the state was issued for, or nullopt if unknown, expired, or already used. Consumes it.
  std::optional<std::string> consumeState(const std::string& state);
  // Exchanges the code from /oauth/google/callback and stores the refresh token. Returns success.
  bool handleCallback(const std::string& code, const std::string& user_id);
  // A valid access token for API calls (refreshing when needed), or nullopt if not connected.
  // force_refresh skips the cache (used after a 401).
  std::optional<std::string> accessToken(const std::string& user_id, bool force_refresh = false);

  bool configured() const { return !client_id_.empty() && !client_secret_.empty(); }
  std::string redirectUri() const;  // <PUBLIC_BASE_URL or http://localhost:PORT>/oauth/google/callback

  // Authorized GET against a Google API: adds the bearer token, retries once with a fresh token on 401.
  // Returns status 401 without a request when the user never connected Google.
  HttpResponse get(const std::string& user_id, const std::string& url);

 private:
  struct Cached {
    std::string token;
    long long expires_at = 0;
  };
  struct PendingState {
    std::string user_id;
    long long expires_at = 0;
  };
  // POSTs to the token endpoint; on success caches the access token and returns the parsed body.
  std::optional<nlohmann::json> tokenRequest(const std::string& user_id, std::map<std::string, std::string> fields);

  std::string client_id_, client_secret_, base_url_;
  Repos& repos_;
  HttpClient http_;
  std::mutex mu_;
  std::map<std::string, Cached> tokens_;
  std::map<std::string, PendingState> states_;
};

}  // namespace enro
