// Emergency contacts with a consent gate (PRD 7.7). Only CONFIRMED, non-REVOKED contacts are
// ever messaged. This check is enforced here and tested in tests/test_contact_consent.cpp.
// Owner: T10.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "models/Models.hpp"

namespace enro {

struct AppContext;

class ContactService {
 public:
  explicit ContactService(AppContext& ctx) : ctx_(ctx) {}

  // Contacts that may be messaged right now. The ONLY list notifyAll may use.
  std::vector<EmergencyContact> notifiable(const std::string& user_id);
  // Texts every notifiable contact. hint is the only detail shared ("they have something at 9").
  // Returns how many messages were sent.
  int notifyAll(const std::string& user_id, const std::string& hint);
  // User confirmed the read-back in onboarding. Sends the one-time "you were added" SMS.
  std::optional<EmergencyContact> confirm(const std::string& contact_id);
  // Contact replied STOP.
  void revokeByPhone(const std::string& phone);

 private:
  AppContext& ctx_;
};

}  // namespace enro
