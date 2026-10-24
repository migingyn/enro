#include "google/GmailClient.hpp"

#include "util/Log.hpp"

namespace enro {

GmailClient::GmailClient(GoogleAuth& auth, Repos& repos) : auth_(auth), repos_(repos) {}

std::vector<EmailMessage> GmailClient::poll(const std::string& user_id) {
  // STUB
  // TODO(T03): users.history.list from kv "gmail_history:<user>" (first run: messages.list newer_than:2d),
  // then messages.get format=metadata (From, Subject) + snippet. Save the new historyId.
  LOG_WARN("google", "GmailClient not implemented yet (T03), no messages for " << user_id);
  return {};
}

}  // namespace enro
