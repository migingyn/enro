#include "triage/EmailTriage.hpp"

#include <algorithm>
#include <cctype>
#include <set>

#include "app/App.hpp"
#include "models/Json.hpp"
#include "planner/DayPlanner.hpp"
#include "util/Log.hpp"
#include "util/Time.hpp"

namespace enro {

namespace {

constexpr int kLookaheadDays = 7;

const char* kExtractSystem =
    "You read one email (sender, subject, snippet) and a list of the user's upcoming events. "
    "Decide which single event the email changes. Return event_id exactly as listed, or an empty string if none. "
    "canceled is true only if that event is canceled. new_start_ts is the new start as Unix seconds if the email "
    "moves it, else null. new_location is the new place if it changes, else null.";

const nlohmann::json kExtractSchema = {
    {"type", "object"},
    {"properties",
     {{"event_id", {{"type", "string"}}},
      {"canceled", {{"type", "boolean"}}},
      {"new_start_ts", {{"type", {"integer", "null"}}}},
      {"new_location", {{"type", {"string", "null"}}}}}},
    {"required", {"event_id", "canceled", "new_start_ts", "new_location"}}};

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
  return s;
}

// Words of an event title worth matching on, plus the course code without its space ("cse110").
std::vector<std::string> titleTokens(const std::string& title) {
  static const std::set<std::string> stop = {"with", "the", "and", "for", "group", "meeting"};
  std::vector<std::string> words, out;
  std::string w;
  for (char c : lower(title) + " ") {
    if (std::isalnum(static_cast<unsigned char>(c))) {
      w += c;
    } else if (!w.empty()) {
      words.push_back(w);
      w.clear();
    }
  }
  for (size_t i = 0; i < words.size(); ++i) {
    if (words[i].size() >= 3 && !stop.count(words[i])) out.push_back(words[i]);
    bool letters = std::all_of(words[i].begin(), words[i].end(), ::isalpha);
    if (letters && i + 1 < words.size() && std::all_of(words[i + 1].begin(), words[i + 1].end(), ::isdigit))
      out.push_back(words[i] + words[i + 1]);  // "CSE 110" -> "cse110"
  }
  return out;
}

// "section" in an email means the discussion section; "lab" likewise.
bool synonymHit(const std::string& token, const std::string& hay) {
  return token == "discussion" && (hay.find("section") != std::string::npos || hay.find("lab") != std::string::npos);
}

std::optional<nlohmann::json> keywordExtract(const EmailMessage& m, const std::vector<Event>& candidates) {
  std::string hay = lower(m.from + " " + m.subject + " " + m.snippet);
  if (hay.find("cancel") == std::string::npos) return std::nullopt;  // only cancellations without an LLM
  const Event* best = nullptr;
  int best_score = 0;
  for (auto& e : candidates) {
    int score = 0;
    for (auto& t : titleTokens(e.title)) score += hay.find(t) != std::string::npos || synonymHit(t, hay);
    if (score > best_score) best = &e, best_score = score;  // earliest wins ties (candidates are sorted)
  }
  if (!best) return std::nullopt;
  return nlohmann::json{{"event_id", best->id}, {"canceled", true}, {"new_start_ts", nullptr}, {"new_location", nullptr}};
}

}  // namespace

std::vector<Event> EmailTriage::candidateEvents(const std::string& user_id) const {
  auto user = ctx_.repos->user(user_id);
  std::string tz = user && !user->timezone.empty() ? user->timezone : "America/Los_Angeles";
  long long from = time::midnightIn(DayPlanner(ctx_).defaultDate(user_id), tz);
  std::vector<Event> out;
  for (auto& e : ctx_.repos->events(user_id, from, from + kLookaheadDays * 86400LL))
    if (!e.canceled) out.push_back(e);
  return out;
}

std::optional<nlohmann::json> EmailTriage::extract(const std::string& user_id, const EmailMessage& m,
                                                   const std::vector<Event>& candidates) {
  auto user = ctx_.repos->user(user_id);
  std::string tz = user && !user->timezone.empty() ? user->timezone : "America/Los_Angeles";
  nlohmann::json events = nlohmann::json::array();
  for (auto& e : candidates) {
    events.push_back({{"event_id", e.id}, {"title", e.title}, {"location", e.location},
                      {"start", time::dateIn(e.start_ts, tz) + " " + time::hhmmIn(e.start_ts, tz)},
                      {"start_ts", e.start_ts}});
  }
  nlohmann::json input = {{"email", {{"from", m.from}, {"subject", m.subject}, {"snippet", m.snippet}}},
                          {"timezone", tz},
                          {"events", events}};
  auto known = [&](const std::string& id) {
    return std::any_of(candidates.begin(), candidates.end(), [&](auto& e) { return e.id == id; });
  };

  nlohmann::json out = ctx_.llm->chatJson(kExtractSystem, input.dump(), kExtractSchema);
  std::string id = out.value("event_id", "");
  if (!id.empty() && known(id)) {
    return nlohmann::json{{"event_id", id},
                          {"canceled", out.value("canceled", false)},
                          {"new_start_ts", out.contains("new_start_ts") ? out["new_start_ts"] : nullptr},
                          {"new_location", out.contains("new_location") ? out["new_location"] : nullptr}};
  }
  if (!id.empty()) LOG_WARN("triage", "LLM named unknown event '" << id << "' for " << m.id);
  return keywordExtract(m, candidates);
}

bool EmailTriage::apply(const std::string& user_id, const nlohmann::json& fact) {
  auto e = ctx_.repos->event(fact.value("event_id", ""));
  if (!e || e->user_id != user_id) return false;
  if (fact.value("canceled", false)) e->canceled = true;
  if (fact.contains("new_start_ts") && fact["new_start_ts"].is_number_integer()) {
    long long duration = e->end_ts - e->start_ts;
    e->start_ts = fact["new_start_ts"].get<long long>();
    e->end_ts = e->start_ts + duration;
  }
  if (fact.contains("new_location") && fact["new_location"].is_string()) e->location = fact["new_location"];
  ctx_.repos->upsertEvent(*e);
  return true;
}

std::vector<TriageResult> EmailTriage::process(const std::string& user_id, const std::vector<EmailMessage>& messages) {
  std::vector<TriageResult> out;
  bool plan_changed = false;
  for (auto& m : messages) {
    if (auto cached = seen_.get(m.id)) {  // nothing is parsed twice (PRD 7.2)
      out.push_back(*cached);
      continue;
    }
    // Reflex sees metadata + snippet only.
    nlohmann::json input = {{"message_id", m.id}, {"from", m.from}, {"subject", m.subject}, {"snippet", m.snippet}};
    ReflexVerdict v = ctx_.reflex->emailRelevance(input);
    ctx_.recordDecision(user_id, "email_relevance", v, {{"message_id", m.id}, {"from", m.from}, {"subject", m.subject}});

    TriageResult r{m, v, std::nullopt};
    if (v.verdict == "PLAN_CHANGE") {
      r.extracted = extract(user_id, m, candidateEvents(user_id));
      if (r.extracted && apply(user_id, *r.extracted)) {
        ctx_.repos->upsertEmailFact({m.id, user_id, v.verdict, time::nowTs(), *r.extracted});
        plan_changed = true;
        LOG_INFO("triage", m.id << " -> " << r.extracted->dump());
      } else {
        r.extracted.reset();
        LOG_INFO("triage", m.id << " is PLAN_CHANGE but matches no upcoming event");
      }
    }
    seen_.put(m.id, r);
    out.push_back(std::move(r));
  }
  if (plan_changed) ctx_.dashboard->broadcast("plan", DayPlanner(ctx_).build(user_id));
  return out;
}

}  // namespace enro
