// Gmail API (EMAIL_SOURCE=gmail). Owner: T03.
// Polls with historyId stored in kv "gmail_history:<user>" (first run: profile historyId + the 10 most
// recent messages from the last 2 days). At most kMaxMessages messages per poll.
// Returns metadata + snippet to triage. Bodies are never stored (PRD 7.2): the only part we decode is a
// text/calendar invitation, in memory, and only the extracted facts (title, place, start) are kept.
// Invitations: complete one-off ICS -> Event (source "email"); METHOD:CANCEL -> marks that event canceled;
// anything ambiguous -> an INVITE_REVIEW email fact, never planned. A message missing from a later
// bounded poll is not evidence of cancellation, so nothing is ever removed for being absent.
#pragma once

#include <nlohmann/json.hpp>

#include "google/EmailSource.hpp"
#include "google/GoogleAuth.hpp"
#include "google/Ics.hpp"

namespace enro {

class GmailClient : public EmailSource {
 public:
  GmailClient(GoogleAuth& auth, Repos& repos);
  std::string name() const override { return "gmail"; }
  std::vector<EmailMessage> poll(const std::string& user_id) override;

  static constexpr size_t kMaxMessages = 10;

  // A text/calendar part (or .ics attachment) of a messages.get format=full payload.
  struct CalendarPart {
    std::string data_b64url;     // inline body.data, or empty
    std::string attachment_id;   // fetch via attachments.get when data is empty
  };
  static std::vector<CalendarPart> calendarParts(const nlohmann::json& payload);
  static std::string base64UrlDecode(const std::string& in);
  // Writes events / cancellations / review facts for a message's invitations. Returns events written.
  int applyInvites(const std::string& user_id, const std::string& message_id, const std::vector<IcsInvite>& invites,
                   long long now);

 private:
  std::string fetchAttachment(const std::string& user_id, const std::string& message_id, const std::string& id);

  GoogleAuth& auth_;
  Repos& repos_;
};

}  // namespace enro
