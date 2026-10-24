// store/: schema, Repos, demo seeding. Shared.
#include <doctest/doctest.h>

#include "TestApp.hpp"
#include "store/Seed.hpp"

using namespace enro;

TEST_CASE("schema creates and Repos round-trip rows") {
  Database db(":memory:");
  db.createSchema();
  db.createSchema();  // idempotent
  Repos repos(db);

  User u;
  u.id = "u1";
  u.name = "Test";
  repos.upsertUser(u);
  CHECK(repos.user("u1")->name == "Test");

  Event e{"e1", "u1", "Class", "Center Hall", "class", "calendar", 100, 200, false};
  repos.upsertEvent(e);
  CHECK(repos.events("u1", 0, 1000).size() == 1);
  CHECK(repos.events("u1", 150, 1000).empty());

  repos.insertDecision({"u1", "email_relevance", "IGNORE", "rules", 0.9, 5, 1, {{"subject", "hi"}}});
  CHECK(repos.recentDecisions().front().verdict == "IGNORE");

  repos.setKv("gmail_history:u1", "42");
  CHECK(repos.kv("gmail_history:u1") == "42");
}

TEST_CASE("seedDemo loads the demo user even before backend/seed exists") {
  auto app = test::fakeApp();
  auto r = seedDemo(*app);
  CHECK(app->repos->user("u_demo"));
  CHECK(app->repos->count("contacts") >= 1);
  CHECK(app->repos->count("events") >= 5);
  CHECK(seedDemo(*app).users == 0);  // only seeds empty tables
  (void)r;
}
