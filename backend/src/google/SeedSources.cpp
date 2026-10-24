#include "google/SeedSources.hpp"

#include <algorithm>

#include "models/Json.hpp"
#include "util/Log.hpp"

namespace enro {

SeedCalendar::SeedCalendar(const std::string& seed_dir, const Fixtures& fixtures) {
  if (auto j = Fixtures::readJsonFile(seed_dir + "/demo_calendar.json"); j && j->contains("events")) {
    all_ = (*j)["events"].get<std::vector<Event>>();
    LOG_INFO("seed", "calendar: " << all_.size() << " events from demo_calendar.json");
  } else {
    all_ = fixtures.json("plan")["events"].get<std::vector<Event>>();
    LOG_WARN("seed", "demo_calendar.json missing, using " << all_.size() << " events from contracts/examples/plan.json");
  }
}

std::vector<Event> SeedCalendar::events(const std::string& user_id, long long from_ts, long long to_ts) {
  std::vector<Event> out;
  for (auto& e : all_) {
    if (e.user_id == user_id && e.start_ts >= from_ts && e.start_ts < to_ts) out.push_back(e);
  }
  return out;
}

SeedEmail::SeedEmail(const std::string& seed_dir, std::set<std::string> held_back) : held_back_(std::move(held_back)) {
  auto j = Fixtures::readJsonFile(seed_dir + "/demo_emails.json");
  if (!j || !j->contains("messages")) {
    LOG_WARN("seed", "demo_emails.json missing, seed inbox is empty");
    return;
  }
  for (auto& m : (*j)["messages"]) {
    all_.push_back({m.value("id", ""), m.value("from", ""), m.value("subject", ""), m.value("snippet", ""),
                    m.value("received_ts", 0LL)});
  }
  LOG_INFO("seed", "inbox: " << all_.size() << " messages from demo_emails.json");
}

std::vector<EmailMessage> SeedEmail::poll(const std::string&) {
  // The seed inbox belongs to the demo user; every poll returns what hasn't been delivered yet.
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<EmailMessage> out;
  for (auto& m : all_) {
    if (held_back_.count(m.id) || delivered_.count(m.id)) continue;
    delivered_.insert(m.id);
    out.push_back(m);
  }
  return out;
}

bool SeedEmail::injectDemo(const std::string& message_id) {
  std::lock_guard<std::mutex> lock(mu_);
  bool known = std::any_of(all_.begin(), all_.end(), [&](auto& m) { return m.id == message_id; });
  if (!known) {
    // No seed file yet: synthesize the demo email so the email-drop beat still works.
    all_.push_back({message_id, "cse110-tas@ucsd.edu", "Section canceled today",
                    "Hi all, discussion section is canceled today. See you Wednesday.", 0});
  }
  held_back_.erase(message_id);
  delivered_.erase(message_id);
  return true;
}

}  // namespace enro
