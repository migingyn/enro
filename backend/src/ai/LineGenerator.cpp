#include "ai/LineGenerator.hpp"

namespace enro {

namespace {

void replaceAll(std::string& s, const std::string& from, const std::string& to) {
  for (size_t pos = 0; (pos = s.find(from, pos)) != std::string::npos; pos += to.size()) s.replace(pos, from.size(), to);
}

}  // namespace

LineGenerator::LineGenerator(LlmClient& llm, nlohmann::json personas, nlohmann::json scripts)
    : llm_(llm), personas_(std::move(personas)), scripts_(std::move(scripts)) {}

std::string LineGenerator::canned(const LineRequest& req) const {
  if (!scripts_.contains("lines") || !scripts_["lines"].contains(req.purpose)) return "";
  auto& options = scripts_["lines"][req.purpose];
  if (!options.is_array() || options.empty()) return "";
  std::string line = options[0].get<std::string>();
  replaceAll(line, "{name}", req.user_name);
  replaceAll(line, "{event}", req.facts.value("event", std::string("your first thing")));
  replaceAll(line, "{minutes}", std::to_string(req.facts.value("minutes", 0)));
  return line;
}

std::string LineGenerator::generate(const LineRequest& req) {
  // STUB
  // TODO(T12): build the persona system prompt (personas_ archetype + urgency_style[urgency]), pass facts,
  // call llm_.chat; keep lines short for calls; fall back to canned() on empty output or the fake LLM.
  (void)llm_;
  (void)personas_;
  std::string line = canned(req);
  if (!line.empty()) return line;
  return "Hey " + (req.user_name.empty() ? std::string("there") : req.user_name) + ", it's Enro. Time to get moving.";
}

}  // namespace enro
