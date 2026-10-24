#include "contacts/ContactService.hpp"

#include "app/App.hpp"

namespace enro {

// Safe by default: until T10 lands, nobody is notifiable and nothing is sent.

std::vector<EmergencyContact> ContactService::notifiable(const std::string&) {
  // STUB
  // TODO(T10): ctx_.repos->contacts(user_id) filtered to consent == CONFIRMED (REVOKED and PENDING excluded).
  return {};
}

int ContactService::notifyAll(const std::string&, const std::string&) {
  // STUB
  // TODO(T10): for each notifiable() contact, ctx_.voice->sendSms with scripts.json contact_sms (automated message
  // from Enro, light tone, only `hint`), record a Decision per message. Never message anyone else.
  return 0;
}

std::optional<EmergencyContact> ContactService::confirm(const std::string&) {
  // STUB
  // TODO(T10): set consent CONFIRMED via ctx_.repos->upsertContact, send contact_added_sms once (reply STOP).
  return std::nullopt;
}

void ContactService::revokeByPhone(const std::string&) {
  // STUB
  // TODO(T10): set consent REVOKED for every contact with this phone.
}

}  // namespace enro
