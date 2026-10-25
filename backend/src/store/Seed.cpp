#include "store/Seed.hpp"

#include <limits>

#include "app/App.hpp"
#include "models/Json.hpp"
#include "util/Log.hpp"

namespace enro {

SeedReport seedDemo(AppContext& ctx) {
  SeedReport r;
  auto& repos = *ctx.repos;
  std::string seed = ctx.env.seedDir();

  // user + contacts
  if (repos.count("users") == 0) {
    User user;
    std::vector<EmergencyContact> contacts;
    if (auto j = Fixtures::readJsonFile(seed + "/demo_user.json"); j && j->contains("user")) {
      user = (*j)["user"].get<User>();
      contacts = j->value("contacts", nlohmann::json::array()).get<std::vector<EmergencyContact>>();
    } else {
      LOG_WARN("seed", "demo_user.json missing, using contracts/examples/onboarding_complete.json");
      auto fx = ctx.fixtures.json("onboarding_complete");
      user = fx["user"].get<User>();
      contacts = fx["contacts"].get<std::vector<EmergencyContact>>();
      auto confirmed = ctx.fixtures.json("contact_confirm")["contact"].get<EmergencyContact>();
      for (auto& c : contacts)
        if (c.id == confirmed.id) c = confirmed;
    }
    repos.upsertUser(user);
    for (auto& c : contacts) repos.upsertContact(c);
    r.users = 1;
    r.contacts = static_cast<int>(contacts.size());
  }

  // events from the calendar source
  if (repos.count("events") == 0) {
    for (auto& u : repos.users()) {
      for (auto& e : ctx.calendar->events(u.id, 0, std::numeric_limits<long long>::max())) {
        repos.upsertEvent(e);
        ++r.events;
      }
    }
  }

  // three weeks of history -> outcomes table + punctuality model
  auto history = Fixtures::readJsonFile(seed + "/demo_history.json");
  if (history) {
    if (repos.count("outcomes") == 0 && history->contains("outcomes")) {
      for (auto& o : (*history)["outcomes"]) {
        repos.insertOutcome(o.get<Outcome>());
        ++r.outcomes;
      }
    }
    ctx.punctuality->loadHistory(*history);
  } else {
    LOG_WARN("seed", "demo_history.json missing, punctuality model starts empty");
  }

  // Real phones for a live demo come from .env only (the repo is public): DEMO_USER_PHONE rings instead of
  // the demo user's 555 number, DEMO_CONTACT_PHONE replaces the first CONFIRMED contact's number.
  if (ctx.env.has("DEMO_USER_PHONE")) {
    for (auto& u : repos.users()) {
      u.phone = ctx.env.get("DEMO_USER_PHONE");
      repos.upsertUser(u);
    }
  }
  if (ctx.env.has("DEMO_CONTACT_PHONE")) {
    for (auto& u : repos.users()) {
      for (auto& c : repos.contacts(u.id)) {
        if (c.consent != ConsentStatus::CONFIRMED) continue;
        c.phone = ctx.env.get("DEMO_CONTACT_PHONE");
        repos.upsertContact(c);
        break;
      }
    }
  }

  LOG_INFO("seed", "seeded users=" << r.users << " contacts=" << r.contacts << " events=" << r.events
                                   << " outcomes=" << r.outcomes);
  return r;
}

}  // namespace enro
