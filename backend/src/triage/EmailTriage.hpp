// Email triage (PRD 7.2): reflex filter on subject/sender/snippet, then LLM extraction only for
// PLAN_CHANGE. Stores the extracted fact, never the body. LRU by message id. Owner: T09.
#pragma once

#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

#include "google/EmailSource.hpp"
#include "reflex/Reflex.hpp"

namespace enro {

struct AppContext;

struct TriageResult {
  EmailMessage message;
  ReflexVerdict verdict;
  // {event_id, canceled, new_start_ts, new_location} for PLAN_CHANGE, else nullopt
  std::optional<nlohmann::json> extracted;
};

class EmailTriage {
 public:
  explicit EmailTriage(AppContext& ctx) : ctx_(ctx) {}

  // Triage new messages; writes one email_relevance Decision per message.
  std::vector<TriageResult> process(const std::string& user_id, const std::vector<EmailMessage>& messages);

 private:
  AppContext& ctx_;
};

}  // namespace enro
