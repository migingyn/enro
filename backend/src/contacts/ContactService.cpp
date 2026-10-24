#include "contacts/ContactService.hpp"

#include "app/App.hpp"
#include "util/Log.hpp"
#include "util/Time.hpp"

namespace enro {

namespace {

std::string firstName(const std::optional<User>& u) {
  if (!u || u->name.empty()) return "Your friend";
  return u->name.substr(0, u->name.find(' '));
}

// The wording is fixed in code on purpose (PRD 7.7): it must say it's automated and from Enro, keep a
// light tone, share nothing beyond the time hint, and offer STOP. No persona or LLM touches it.
std::string notifyText(const std::string& name, const std::string& hint) {
  std::string when = hint.empty() ? "something on today" : "something at " + hint;
  return "Automated message from Enro: " + name + " hasn't picked up their wake-up calls and has " + when +
         ". You're their backup, so a quick call might help. Reply STOP to stop these texts.";
}

std::string addedText(const std::string& name) {
  return "Automated message from Enro: " + name +
         " added you as a wake-up backup. If they sleep through all their calls, we'll text you. "
         "Reply STOP to opt out.";
}

}  // namespace

std::vector<EmergencyContact> ContactService::notifiable(const std::string& user_id) {
  std::vector<EmergencyContact> out;
  for (auto& c : ctx_.repos->contacts(user_id)) {
    if (c.consent == ConsentStatus::CONFIRMED) out.push_back(c);  // PENDING and REVOKED never pass
  }
  return out;
}

int ContactService::notifyAll(const std::string& user_id, const std::string& hint) {
  std::string body = notifyText(firstName(ctx_.repos->user(user_id)), hint);
  int sent = 0;
  for (auto& c : notifiable(user_id)) {
    ctx_.voice->sendSms(c.phone, body);
    ++sent;
  }
  return sent;
}

std::optional<EmergencyContact> ContactService::confirm(const std::string& contact_id) {
  auto c = ctx_.repos->contact(contact_id);
  if (!c) return std::nullopt;
  if (c->consent == ConsentStatus::REVOKED) return c;  // they said STOP; only they can undo that
  c->consent = ConsentStatus::CONFIRMED;
  ctx_.repos->upsertContact(*c);
  std::string sent_key = "contact_added_sms:" + c->id;
  if (!ctx_.repos->kv(sent_key)) {
    ctx_.voice->sendSms(c->phone, addedText(firstName(ctx_.repos->user(c->user_id))));
    ctx_.repos->setKv(sent_key, std::to_string(time::nowTs()));
  }
  return c;
}

void ContactService::revokeByPhone(const std::string& phone) {
  for (auto& u : ctx_.repos->users()) {
    for (auto& c : ctx_.repos->contacts(u.id)) {
      if (c.phone != phone || c.consent == ConsentStatus::REVOKED) continue;
      c.consent = ConsentStatus::REVOKED;
      ctx_.repos->upsertContact(c);
      LOG_INFO("contacts", "revoked " << c.id << " after STOP");
    }
  }
}

}  // namespace enro
