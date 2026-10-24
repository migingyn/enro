#include "triage/EmailTriage.hpp"

#include "app/App.hpp"

namespace enro {

std::vector<TriageResult> EmailTriage::process(const std::string& user_id, const std::vector<EmailMessage>& messages) {
  std::vector<TriageResult> out;
  for (auto& m : messages) {
    nlohmann::json input = {{"message_id", m.id}, {"from", m.from}, {"subject", m.subject}};
    ReflexVerdict v = ctx_.reflex->emailRelevance(input);
    ctx_.recordDecision(user_id, "email_relevance", v, input);
    // STUB
    // TODO(T09): skip ids already in the LRU cache (T05); for PLAN_CHANGE call ctx_.llm->chatJson to extract
    // {event_id, canceled, new_start_ts, new_location}, save with ctx_.repos->upsertEmailFact, update the
    // event (ctx_.repos->upsertEvent), and rebuild + broadcast the plan.
    out.push_back({m, v, std::nullopt});
  }
  return out;
}

}  // namespace enro
