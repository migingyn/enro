// Gmail API (EMAIL_SOURCE=gmail). Polls with historyId stored in kv, fetches metadata + snippet
// only (format=metadata). Never stores bodies (PRD 7.2). Owner: T03.
#pragma once

#include "google/EmailSource.hpp"
#include "google/GoogleAuth.hpp"

namespace enro {

class GmailClient : public EmailSource {
 public:
  GmailClient(GoogleAuth& auth, Repos& repos);
  std::string name() const override { return "gmail"; }
  std::vector<EmailMessage> poll(const std::string& user_id) override;

 private:
  GoogleAuth& auth_;
  Repos& repos_;
};

}  // namespace enro
