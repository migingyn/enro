// Voice onboarding save (PRD 5, 11). Owner: T01.
#pragma once

#include <nlohmann/json.hpp>

namespace enro {

struct AppContext;

// Saves body.profile as a User and body.contacts as PENDING contacts (only ContactService::confirm
// makes one CONFIRMED, PRD 7.7). Returns the onboarding_complete response: {user, contacts, oauth_url}.
// The user id is body.user_id, else u_demo in DEMO_MODE (onboarding sets up the demo user), else a new id.
nlohmann::json completeOnboarding(AppContext& ctx, const nlohmann::json& body, long long now);

}  // namespace enro
