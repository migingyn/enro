// Where emails come from. Selected by EMAIL_SOURCE: seed | gmail.
// Only metadata and a snippet ever leave this layer. Never the body (PRD 7.2, 13).
#pragma once

#include <string>
#include <vector>

namespace enro {

struct EmailMessage {
  std::string id, from, subject, snippet;
  long long received_ts = 0;
};

class EmailSource {
 public:
  virtual ~EmailSource() = default;
  virtual std::string name() const = 0;
  // Messages that arrived since the last poll for this user.
  virtual std::vector<EmailMessage> poll(const std::string& user_id) = 0;
  // Demo "email drop" (PRD 14 step 4): make one held-back message arrive on the next poll.
  // Returns false if the source doesn't support it or the id is unknown.
  virtual bool injectDemo(const std::string& message_id) { return false; }
};

}  // namespace enro
