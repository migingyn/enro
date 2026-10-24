#include "ai/LineGenerator.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

#include "models/Json.hpp"
#include "util/Log.hpp"

namespace enro {

namespace {

void replaceAll(std::string& s, const std::string& from, const std::string& to) {
  for (size_t pos = 0; (pos = s.find(from, pos)) != std::string::npos; pos += to.size()) s.replace(pos, from.size(), to);
}

// facts may be null when a caller brace-initializes the request with {}.
template <typename T>
T fact(const nlohmann::json& facts, const char* key, T fallback) {
  return facts.is_object() ? facts.value(key, fallback) : fallback;
}

bool hasContacts(const LineRequest& req) { return fact(req.facts, "has_contacts", false); }

std::string urgencyName(Urgency u) { return nlohmann::json(u).get<std::string>(); }

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
  return s;
}

std::string trim(const std::string& s) {
  size_t b = s.find_first_not_of(" \t\r\n\"'");
  if (b == std::string::npos) return "";
  size_t e = s.find_last_not_of(" \t\r\n\"'");
  return s.substr(b, e - b + 1);
}

// Cuts to `max_words`, at the last sentence end inside the limit when there is one.
std::string clampWords(const std::string& line, int max_words) {
  std::istringstream in(line);
  std::vector<std::string> words;
  for (std::string w; in >> w;) words.push_back(w);
  if (static_cast<int>(words.size()) <= max_words) return line;
  std::string out;
  size_t sentence_end = 0;
  for (int i = 0; i < max_words; ++i) {
    if (i) out += ' ';
    out += words[i];
    char last = words[i].back();
    if (last == '.' || last == '!' || last == '?') sentence_end = out.size();
  }
  if (sentence_end > 0) return out.substr(0, sentence_end);
  while (!out.empty() && std::ispunct(static_cast<unsigned char>(out.back()))) out.pop_back();
  return out + ".";
}

// Built-in persona when personas.json is missing or has no such archetype.
const nlohmann::json& defaultArchetype() {
  static const nlohmann::json a = {
      {"id", "best_friend"},
      {"system_prompt",
       "You are Enro, the user's best friend who knows their calendar. Casual, caring, a little teasing."},
      {"urgency_style",
       {{"CHILL", "Relaxed and playful."},
        {"FIRM", "Friendly but you mean it. Point at the clock."},
        {"URGENT", "No jokes. Blunt and fast."}}}};
  return a;
}

}  // namespace

LineGenerator::LineGenerator(LlmClient& llm, nlohmann::json personas, nlohmann::json scripts)
    : llm_(llm), personas_(std::move(personas)), scripts_(std::move(scripts)) {}

int LineGenerator::maxWords(Urgency urgency) {
  switch (urgency) {
    case Urgency::URGENT: return 14;
    case Urgency::FIRM: return 24;
    default: return 36;
  }
}

bool LineGenerator::isSafe(const std::string& line, bool has_contacts) {
  // PRD 7.10: the only consequence a persona may name is calling emergency contacts, and only if they exist.
  static const char* threats[] = {"or else",  "you'll regret", "you will regret", "i'll hurt", "i will hurt",
                                  "punish",   "i'll post",     "i will post",     "your boss", "get you fired",
                                  "i'll tell", "i will tell",  "kill",            "hurt you",  "destroy"};
  static const char* contact_words[] = {"emergency contact", "your mom", "your dad", "your contact",
                                        "call your", "text your", "notify"};
  std::string l = lower(line);
  for (auto* t : threats)
    if (l.find(t) != std::string::npos) return false;
  if (!has_contacts)
    for (auto* c : contact_words)
      if (l.find(c) != std::string::npos) return false;
  return true;
}

const nlohmann::json* LineGenerator::archetype(const std::string& id) const {
  if (!personas_.contains("archetypes") || !personas_["archetypes"].is_array()) return nullptr;
  for (auto& a : personas_["archetypes"])
    if (a.value("id", std::string()) == id) return &a;
  return nullptr;
}

std::string LineGenerator::systemPrompt(const LineRequest& req) const {
  const nlohmann::json* found = archetype(req.persona);
  const nlohmann::json& a = found ? *found : defaultArchetype();
  std::string level = urgencyName(req.urgency);
  std::string style;
  if (a.contains("urgency_style") && a["urgency_style"].contains(level))
    style = a["urgency_style"][level].get<std::string>();
  bool has_contacts = hasContacts(req);

  std::ostringstream p;
  p << a.value("system_prompt", defaultArchetype()["system_prompt"].get<std::string>()) << "\n\n";
  p << "Urgency: " << level << ". " << style << "\n";
  p << "Write one spoken line for a phone call or earbud, at most " << maxWords(req.urgency) << " words. ";
  p << (req.urgency == Urgency::URGENT ? "One sentence. " : "One or two sentences. ");
  p << "Plain speech only: no emoji, no lists, no stage directions, no quotes.\n";
  p << "Use the real numbers from the facts (minutes, consequences, history). Never invent numbers or events.\n";
  p << "Never threaten the user. ";
  if (has_contacts)
    p << "The only consequence you may mention is that Enro will call their emergency contacts.";
  else
    p << "They have no emergency contacts, so never mention contacting anyone.";
  return p.str();
}

std::string LineGenerator::canned(const LineRequest& req) const {
  if (!scripts_.contains("lines")) return "";
  auto& lines = scripts_["lines"];
  std::string key = req.purpose + "_" + lower(urgencyName(req.urgency));
  if (!lines.contains(key)) key = req.purpose;
  if (!lines.contains(key)) return "";
  auto& options = lines[key];
  if (!options.is_array() || options.empty()) return "";
  std::string line = options[0].get<std::string>();
  replaceAll(line, "{name}", req.user_name);
  replaceAll(line, "{event}", fact(req.facts, "event", std::string("your first thing")));
  replaceAll(line, "{minutes}", std::to_string(fact(req.facts, "minutes", 0)));
  return line;
}

std::string LineGenerator::fallback(const LineRequest& req) const {
  std::string line = canned(req);
  if (!line.empty()) return line;
  std::string name = req.user_name.empty() ? std::string("there") : req.user_name;
  switch (req.urgency) {
    case Urgency::URGENT: return name + ". Up. Now.";
    case Urgency::FIRM: return name + ", it's Enro. Time to get up and get moving.";
    default: return "Hey " + name + ", it's Enro. No rush yet, but it's time to start getting ready.";
  }
}

std::string LineGenerator::generate(const LineRequest& req) {
  // The fake LLM echoes its input, so it would only produce noise. Canned lines are the keyless path.
  if (llm_.name() == "fake") return fallback(req);

  nlohmann::json user = {{"purpose", req.purpose}, {"user_name", req.user_name}, {"facts", req.facts}};
  std::string line;
  try {
    line = trim(llm_.chat(systemPrompt(req), {{"user", user.dump()}}));
  } catch (const std::exception& e) {
    LOG_WARN("lines", "llm failed for " << req.purpose << ": " << e.what());
  }
  if (line.empty()) return fallback(req);
  if (!isSafe(line, hasContacts(req))) {
    LOG_WARN("lines", "dropped unsafe line for " << req.purpose);
    return fallback(req);
  }
  return clampWords(line, maxWords(req.urgency));
}

}  // namespace enro
