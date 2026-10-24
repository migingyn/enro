// Google OAuth 2.0 (Calendar + Gmail read-only scopes). Refresh tokens stay server side in
// the oauth_tokens table (PRD 13). REST through cpr. Owner: T03.
#pragma once

#include <optional>
#include <string>

#include "config/Env.hpp"
#include "store/Repos.hpp"

namespace enro {

class GoogleAuth {
 public:
  GoogleAuth(const Env& env, Repos& repos);

  static constexpr const char* kScopes =
      "https://www.googleapis.com/auth/calendar.readonly https://www.googleapis.com/auth/gmail.readonly";

  // URL to send the browser to. state carries the user id.
  std::string authUrl(const std::string& user_id) const;
  // Exchanges the code from /oauth/google/callback and stores the refresh token. Returns success.
  bool handleCallback(const std::string& code, const std::string& user_id);
  // A valid access token for API calls (refreshing when needed), or nullopt if not connected.
  std::optional<std::string> accessToken(const std::string& user_id);

  bool configured() const { return !client_id_.empty() && !client_secret_.empty(); }
  std::string redirectUri() const;  // <PUBLIC_BASE_URL or http://localhost:PORT>/oauth/google/callback

 private:
  std::string client_id_, client_secret_, base_url_;
  Repos& repos_;
};

}  // namespace enro
