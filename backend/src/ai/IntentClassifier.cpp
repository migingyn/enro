#include "ai/IntentClassifier.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <regex>
#include <vector>

namespace enro {

namespace {

const std::vector<std::string> kLabels = {"SNOOZE_REQUEST", "AWAKE", "READY", "STALLING", "LEFT", "ASK_CONSEQUENCES", "UNCLEAR"};

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
  return s;
}

bool has(const std::string& s, std::initializer_list<const char*> needles) {
  return std::any_of(needles.begin(), needles.end(), [&](const char* n) { return s.find(n) != std::string::npos; });
}

// "five more minutes", "10 min", "half an hour" -> minutes. 0 when no number is said.
int minutesSaid(const std::string& s) {
  std::smatch m;
  if (std::regex_search(s, m, std::regex(R"((\d+)\s*(more\s*)?(m\b|min|minute))"))) return std::stoi(m[1]);
  if (has(s, {"half an hour", "half hour"})) return 30;
  if (has(s, {"an hour", "one hour"})) return 60;
  static const std::map<std::string, int> words = {
      {"one", 1},   {"two", 2},      {"three", 3},   {"four", 4},     {"five", 5},   {"six", 6},
      {"seven", 7}, {"eight", 8},    {"nine", 9},    {"ten", 10},     {"fifteen", 15}, {"twenty", 20},
      {"thirty", 30}, {"a couple", 2}, {"a few", 3}, {"a minute", 1}, {"a sec", 1},  {"a second", 1}};
  for (auto& [w, n] : words)
    if (std::regex_search(s, std::regex("\\b" + w + R"(\b\s*(more\s*)?(min|minute|sec))"))) return n;
  return 0;
}

}  // namespace

Intent IntentClassifier::keywords(const std::string& raw) {
  std::string s = lower(raw);
  int minutes = minutesSaid(s);
  if (has(s, {"what happens", "what if", "will i be late", "am i late", "how late", "do i have time"}))
    return {"ASK_CONSEQUENCES", minutes, 0.8};
  if (has(s, {"leaving now", "i'm leaving", "im leaving", "heading out", "out the door", "on my way", "left already",
              "on the bus", "walking now"}))
    return {"LEFT", 0, 0.9};
  if (minutes > 0 || has(s, {"snooze", "more minutes", "few more", "five more", "later", "let me sleep", "not yet"}))
    return {"SNOOZE_REQUEST", minutes > 0 ? minutes : 5, 0.85};
  if (has(s, {"ready", "dressed", "getting dressed", "brushing", "showered", "packing", "putting my shoes"}))
    return {"READY", 0, 0.8};
  if (has(s, {"i'm up", "im up", "i am up", "i'm awake", "im awake", "i am awake", "awake", "getting up", "i'm out of bed",
              "okay okay", "ok ok", "yeah i'm up"}))
    return {"AWAKE", 0, 0.85};
  if (has(s, {"ugh", "hmm", "go away", "leave me", "tired", "don't want", "dont want"}) ||
      std::regex_search(s, std::regex(R"(\b(no|nah|mm+|stop)\b)")))
    return {"STALLING", 0, 0.6};
  return {"UNCLEAR", 0, 0.3};
}

Intent IntentClassifier::classify(const std::string& text, const nlohmann::json& context) {
  Intent kw = keywords(text);
  if (llm_.name() == "fake" || text.empty()) return kw;

  nlohmann::json schema = {{"type", "object"},
                           {"properties",
                            {{"label", {{"type", "string"}, {"enum", kLabels}}},
                             {"minutes", {{"type", "integer"}}},
                             {"confidence", {{"type", "number"}}}}},
                           {"required", {"label", "minutes", "confidence"}}};
  std::string system =
      "You classify what a sleepy person said on a wake-up or leave-now phone call. Labels: SNOOZE_REQUEST (asks for "
      "more time; set minutes, default 5), AWAKE (says they're up), READY (getting ready), STALLING (groggy, refusing, "
      "no commitment), LEFT (leaving or already out), ASK_CONSEQUENCES (asks what happens if they're late), UNCLEAR. "
      "Reply with JSON only.";
  nlohmann::json user = {{"text", text}, {"context", context}};
  try {
    auto out = llm_.chatJson(system, user.dump(), schema);
    std::string label = out.value("label", "");
    if (std::find(kLabels.begin(), kLabels.end(), label) == kLabels.end()) return kw;
    Intent r{label, out.value("minutes", 0), out.value("confidence", 0.7)};
    if (r.label == "SNOOZE_REQUEST" && r.minutes <= 0) r.minutes = kw.minutes > 0 ? kw.minutes : 5;
    return r;
  } catch (...) {
    return kw;
  }
}

}  // namespace enro
