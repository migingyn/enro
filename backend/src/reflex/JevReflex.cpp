#include "reflex/JevReflex.hpp"

#include <chrono>

#include <cpr/cpr.h>

#include "util/Log.hpp"

namespace enro {

JevReflex::JevReflex(const Env& env)
    : api_key_(env.get("TYPESAFE_API_KEY")),
      model_(env.get("JEV_MODEL")),
      openrouter_key_(env.get("OPENROUTER_API_KEY")),
      base_url_(env.get("JEV_BASE_URL")),
      timeout_ms_(env.getInt("JEV_TIMEOUT_MS", 400)) {
  if (api_key_.empty() && openrouter_key_.empty()) LOG_WARN("jev", "REFLEX_PROVIDER=jev but no key, using rules");
}

// TypeSafe answers typed questions, OpenRouter answers chat completions. Accept either shape:
// {"answers":[{"choice":..,"probability":..}]}, {"choice":..}, or a chat message whose content is
// {"verdict":..,"confidence":..}. Anything outside the allowed verdicts is rejected.
std::optional<ReflexVerdict> JevReflex::parseVerdict(const nlohmann::json& body, const nlohmann::json& verdicts) {
  nlohmann::json a = body;
  if (body.contains("choices") && !body["choices"].empty()) {
    auto content = body["choices"][0].value("message", nlohmann::json::object()).value("content", "");
    a = nlohmann::json::parse(content, nullptr, false);
    if (a.is_discarded()) return std::nullopt;
  } else if (body.contains("answers") && body["answers"].is_array() && !body["answers"].empty()) {
    a = body["answers"][0];
  }
  if (!a.is_object()) return std::nullopt;

  std::string verdict;
  for (auto* k : {"verdict", "choice", "answer"})
    if (a.contains(k) && a[k].is_string()) {
      verdict = a[k].get<std::string>();
      break;
    }
  bool allowed = false;
  for (auto& v : verdicts) allowed = allowed || v == verdict;
  if (!allowed) return std::nullopt;

  ReflexVerdict out;
  out.verdict = verdict;
  out.source = "jev";
  for (auto* k : {"confidence", "probability"})
    if (a.contains(k) && a[k].is_number()) {
      out.confidence = a[k].get<double>();
      break;
    }
  return out;
}

std::optional<ReflexVerdict> JevReflex::ask(const std::string& decision, const nlohmann::json& verdicts,
                                            const nlohmann::json& input) {
  if (api_key_.empty() && openrouter_key_.empty()) return std::nullopt;

  // Unverified shape (PRD 7.11): the docs were unreachable when this was written. Check
  // docs.typesafe.ai /v1/systemone and fix the body here if Jev rejects it. Rules cover the gap.
  bool typesafe = !api_key_.empty();
  std::string url, key;
  nlohmann::json body;
  if (typesafe) {
    url = (base_url_.empty() ? "https://api.typesafe.ai" : base_url_) + "/v1/systemone";
    key = api_key_;
    body = {{"state", input},
            {"questions", nlohmann::json::array({{{"id", decision}, {"type", "choice"}, {"options", verdicts}}})}};
    if (!model_.empty()) body["model"] = model_;
  } else {
    url = (base_url_.empty() ? "https://openrouter.ai/api" : base_url_) + "/v1/chat/completions";
    key = openrouter_key_;
    body = {{"model", model_.empty() ? "typesafe/jev-router" : model_},
            {"messages",
             nlohmann::json::array(
                 {{{"role", "system"},
                   {"content", "Decision " + decision + ". Answer only JSON {\"verdict\": one of " + verdicts.dump() +
                                   ", \"confidence\": 0..1}."}},
                  {{"role", "user"}, {"content", input.dump()}}})}};
  }

  auto start = std::chrono::steady_clock::now();
  auto r = cpr::Post(cpr::Url{url},
                     cpr::Header{{"Authorization", "Bearer " + key}, {"Content-Type", "application/json"}},
                     cpr::Body{body.dump()}, cpr::Timeout{timeout_ms_});
  int latency =
      (int)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

  if (r.error || r.status_code != 200) {
    LOG_WARN("jev", decision << " fell back to rules after " << latency << "ms: "
                             << (r.error ? r.error.message : "HTTP " + std::to_string(r.status_code) + " " + r.text));
    return std::nullopt;
  }
  auto parsed = nlohmann::json::parse(r.text, nullptr, false);
  auto v = parsed.is_discarded() ? std::nullopt : parseVerdict(parsed, verdicts);
  if (!v) {
    LOG_WARN("jev", decision << " unreadable verdict, using rules: " << r.text);
    return std::nullopt;
  }
  v->latency_ms = latency;
  LOG_INFO("jev", decision << " -> " << v->verdict << " (" << v->confidence << ") in " << latency << "ms");
  return v;
}

// Each decision: try Jev, fall back to rules. Rules fallback keeps source "rules".
#define ENRO_JEV_DECISION(method, decision, ...)                                    \
  ReflexVerdict JevReflex::method(const nlohmann::json& input) {                    \
    if (auto v = ask(decision, nlohmann::json::array({__VA_ARGS__}), input)) return *v; \
    return fallback_.method(input);                                                 \
  }

ENRO_JEV_DECISION(emailRelevance, "email_relevance", "IGNORE", "MAYBE", "PLAN_CHANGE")
ENRO_JEV_DECISION(transitSpeak, "transit_speak", "IGNORE", "COMMENT", "ALERT", "REROUTE")
ENRO_JEV_DECISION(interruptChannel, "interrupt_channel", "WAIT", "EARBUD", "CALL")
ENRO_JEV_DECISION(readiness, "readiness", "READY", "STALLING", "UNCLEAR")
ENRO_JEV_DECISION(snooze, "snooze", "GRANT", "COUNTER", "REFUSE")
ENRO_JEV_DECISION(stopCondition, "stop_condition", "CONTINUE", "BACK_OFF", "STOP", "ESCALATE")

#undef ENRO_JEV_DECISION

}  // namespace enro
