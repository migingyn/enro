// T12: LineGenerator + personas.
#include <doctest/doctest.h>

#include <sstream>
#include <stdexcept>

#include "TestApp.hpp"
#include "ai/FakeLlm.hpp"
#include "ai/LineGenerator.hpp"

using namespace enro;

namespace {

// Stands in for a real provider: returns a fixed reply and records the prompt it was given.
class ScriptedLlm : public LlmClient {
 public:
  explicit ScriptedLlm(std::string reply) : reply_(std::move(reply)) {}
  std::string name() const override { return "scripted"; }
  nlohmann::json chatJson(const std::string&, const std::string&, const nlohmann::json&) override { return {}; }
  std::string chat(const std::string& system, const std::vector<ChatMessage>& messages) override {
    last_system = system;
    last_user = messages.empty() ? "" : messages.back().content;
    if (throws) throw std::runtime_error("timeout");
    return reply_;
  }
  std::string last_system, last_user;
  bool throws = false;

 private:
  std::string reply_;
};

int wordCount(const std::string& s) {
  std::istringstream in(s);
  int n = 0;
  for (std::string w; in >> w;) ++n;
  return n;
}

nlohmann::json personas() { return test::fakeApp()->personas; }

}  // namespace

TEST_CASE("canned lines fill placeholders, generate never returns empty") {
  FakeLlm llm;
  nlohmann::json scripts = {{"lines", {{"wake_call_1", {"Morning {name}, {event} in {minutes}."}}}}};
  LineGenerator g(llm, {}, scripts);
  LineRequest r{"wake_call_1", "best_friend", Urgency::CHILL, "Sam", {{"event", "CSE 110"}, {"minutes", 60}}};
  CHECK(g.canned(r) == "Morning Sam, CSE 110 in 60.");
  CHECK(g.generate(r) == "Morning Sam, CSE 110 in 60.");
  CHECK_FALSE(g.generate({"no_such_key", "coach", Urgency::URGENT, "Sam", {}}).empty());
}

TEST_CASE("canned lines prefer the urgency-specific key") {
  FakeLlm llm;
  nlohmann::json scripts = {{"lines",
                             {{"leave_reminder", {"Leave soon, {name}."}},
                              {"leave_reminder_urgent", {"{name}. Out the door. {minutes} minutes."}}}}};
  LineGenerator g(llm, {}, scripts);
  CHECK(g.canned({"leave_reminder", "coach", Urgency::URGENT, "Sam", {{"minutes", 4}}}) ==
        "Sam. Out the door. 4 minutes.");
  CHECK(g.canned({"leave_reminder", "coach", Urgency::FIRM, "Sam", {}}) == "Leave soon, Sam.");
}

TEST_CASE("personas.json has four archetypes with prompts for every urgency") {
  auto p = personas();
  REQUIRE(p.contains("archetypes"));
  CHECK(p["archetypes"].size() == 4);
  for (auto& a : p["archetypes"]) {
    CAPTURE(a.value("id", std::string()));
    CHECK_FALSE(a.value("system_prompt", std::string()).empty());
    for (auto* level : {"CHILL", "FIRM", "URGENT"}) CHECK_FALSE(a["urgency_style"].value(level, std::string()).empty());
    CHECK(LineGenerator::isSafe(a["system_prompt"].get<std::string>(), false));
  }
}

TEST_CASE("system prompt carries the persona and shifts tone by urgency") {
  FakeLlm llm;
  LineGenerator g(llm, personas(), {});
  std::string chill = g.systemPrompt({"wake_call_1", "drill_sergeant", Urgency::CHILL, "Sam", {}});
  std::string urgent = g.systemPrompt({"wake_call_3", "drill_sergeant", Urgency::URGENT, "Sam", {}});
  CHECK(chill.find("drill sergeant") != std::string::npos);
  CHECK(chill.find("Urgency: CHILL. Firm wake-up order") != std::string::npos);
  CHECK(urgent.find("Urgency: URGENT. Full volume") != std::string::npos);
  CHECK(chill.find("at most 36 words") != std::string::npos);
  CHECK(urgent.find("at most 14 words") != std::string::npos);
  CHECK(LineGenerator::maxWords(Urgency::URGENT) < LineGenerator::maxWords(Urgency::FIRM));
  CHECK(LineGenerator::maxWords(Urgency::FIRM) < LineGenerator::maxWords(Urgency::CHILL));

  // Unknown archetype or no personas.json still yields a usable prompt.
  LineGenerator bare(llm, {}, {});
  CHECK(bare.systemPrompt({"x", "nobody", Urgency::FIRM, "Sam", {}}).find("You are Enro") != std::string::npos);
}

TEST_CASE("URGENT lines are harsher and shorter than CHILL") {
  ScriptedLlm llm(
      "Sam, it's ten past seven and CSE 110 starts at nine, so you have about twenty minutes before you need to be "
      "showered, dressed, and walking out the door with your bag and your laptop charger packed.");
  LineGenerator g(llm, personas(), {});
  nlohmann::json facts = {{"event", "CSE 110"}, {"minutes", 20}};
  std::string chill = g.generate({"leave_reminder", "coach", Urgency::CHILL, "Sam", facts});
  std::string urgent = g.generate({"leave_reminder", "coach", Urgency::URGENT, "Sam", facts});
  CHECK(wordCount(chill) <= LineGenerator::maxWords(Urgency::CHILL));
  CHECK(wordCount(urgent) <= LineGenerator::maxWords(Urgency::URGENT));
  CHECK(wordCount(urgent) < wordCount(chill));
  CHECK(llm.last_system.find("Final whistle") != std::string::npos);
  // Facts reach the model as JSON so it can use the real numbers.
  CHECK(nlohmann::json::parse(llm.last_user)["facts"]["minutes"] == 20);

  // Keyless: the built-in fallback still changes tone with urgency.
  FakeLlm fake;
  LineGenerator k(fake, personas(), {});
  std::string kc = k.generate({"none", "coach", Urgency::CHILL, "Sam", {}});
  std::string ku = k.generate({"none", "coach", Urgency::URGENT, "Sam", {}});
  CHECK(wordCount(ku) < wordCount(kc));
  CHECK(kc != ku);
}

TEST_CASE("persona never threatens anything but calling emergency contacts") {
  CHECK_FALSE(LineGenerator::isSafe("Get up or else.", true));
  CHECK_FALSE(LineGenerator::isSafe("I'll tell your boss you overslept.", true));
  CHECK_FALSE(LineGenerator::isSafe("Up now or I'm calling your mom.", false));
  CHECK(LineGenerator::isSafe("Up now or I'm calling your mom.", true));
  CHECK(LineGenerator::isSafe("Sam, CSE 110 in twenty minutes. Move.", false));

  nlohmann::json scripts = {{"lines", {{"wake_call_3", {"{name}, wake up!"}}}}};

  // An unsafe model reply is dropped for the canned line.
  ScriptedLlm threat("Get up or else you'll regret it.");
  LineGenerator g(threat, personas(), scripts);
  CHECK(g.generate({"wake_call_3", "drill_sergeant", Urgency::URGENT, "Sam", {{"has_contacts", true}}}) ==
        "Sam, wake up!");

  // Mentioning contacts is allowed only when the user has them, and the prompt says which.
  ScriptedLlm contacts("Up now or I call your emergency contact.");
  LineGenerator c(contacts, personas(), scripts);
  CHECK(c.generate({"wake_call_3", "coach", Urgency::URGENT, "Sam", {{"has_contacts", true}}}) ==
        "Up now or I call your emergency contact.");
  CHECK(contacts.last_system.find("call their emergency contacts") != std::string::npos);
  CHECK(c.generate({"wake_call_3", "coach", Urgency::URGENT, "Sam", {}}) == "Sam, wake up!");
  CHECK(contacts.last_system.find("never mention contacting anyone") != std::string::npos);
}

TEST_CASE("LLM failure or empty reply falls back to the canned line") {
  nlohmann::json scripts = {{"lines", {{"arrived", {"Made it, {name}."}}}}};
  ScriptedLlm empty("  \n");
  LineGenerator e(empty, personas(), scripts);
  CHECK(e.generate({"arrived", "calm_assistant", Urgency::CHILL, "Sam", {}}) == "Made it, Sam.");

  ScriptedLlm broken("unused");
  broken.throws = true;
  LineGenerator b(broken, personas(), scripts);
  CHECK(b.generate({"arrived", "calm_assistant", Urgency::CHILL, "Sam", {}}) == "Made it, Sam.");

  ScriptedLlm quoted("\"You made it, Sam.\"");
  LineGenerator q(quoted, personas(), scripts);
  CHECK(q.generate({"arrived", "calm_assistant", Urgency::CHILL, "Sam", {}}) == "You made it, Sam.");
}
