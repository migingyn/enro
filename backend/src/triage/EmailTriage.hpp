// Email triage (PRD 7.2): reflex filter on subject/sender/snippet, then LLM extraction only for
// PLAN_CHANGE. Stores the extracted fact, never the body. LRU by message id. Owner: T09.
#pragma once

#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

#include "cache/LruCache.hpp"
#include "google/EmailSource.hpp"
#include "models/Models.hpp"
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

  // Triage new messages: one email_relevance Decision each, extraction + plan update for PLAN_CHANGE.
  // A message id seen before returns its earlier result without any new work.
  std::vector<TriageResult> process(const std::string& user_id, const std::vector<EmailMessage>& messages);

  // Which event a plan-changing email is about, and what changed. LLM first, keyword fallback
  // (so the fake LLM still handles the demo email). nullopt when no known event matches.
  std::optional<nlohmann::json> extract(const std::string& user_id, const EmailMessage& m,
                                        const std::vector<Event>& candidates);

 private:
  std::vector<Event> candidateEvents(const std::string& user_id) const;
  bool apply(const std::string& user_id, const nlohmann::json& fact);

  AppContext& ctx_;
  LruCache<std::string, TriageResult> seen_{2000};
};

}  // namespace enro
