#include "google/GoogleAuth.hpp"

#include "util/Log.hpp"

namespace enro {

GoogleAuth::GoogleAuth(const Env& env, Repos& repos)
    : client_id_(env.get("GOOGLE_CLIENT_ID")),
      client_secret_(env.get("GOOGLE_CLIENT_SECRET")),
      base_url_(env.get("PUBLIC_BASE_URL", "http://localhost:" + std::to_string(env.port()))),
      repos_(repos) {}

std::string GoogleAuth::redirectUri() const { return base_url_ + "/oauth/google/callback"; }

std::string GoogleAuth::authUrl(const std::string& user_id) const {
  // STUB
  // TODO(T03): build https://accounts.google.com/o/oauth2/v2/auth with client_id, redirect_uri, response_type=code,
  // scope=kScopes, access_type=offline, prompt=consent, state=user_id (URL-encoded).
  return redirectUri() + "?code=fake&state=" + user_id;
}

bool GoogleAuth::handleCallback(const std::string& code, const std::string& user_id) {
  // STUB
  // TODO(T03): POST https://oauth2.googleapis.com/token (grant_type=authorization_code), then
  // repos_.saveRefreshToken(user_id, "google", refresh_token).
  LOG_INFO("google", "fake OAuth callback for " << user_id << " (code " << code << ")");
  return true;
}

std::optional<std::string> GoogleAuth::accessToken(const std::string& user_id) {
  // STUB
  // TODO(T03): refresh with the stored refresh token (grant_type=refresh_token), cache until expiry.
  (void)user_id;
  return std::nullopt;
}

}  // namespace enro
