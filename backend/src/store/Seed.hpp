// Demo seeding (DEMO_MODE=true): loads the demo user, contacts, calendar, and three weeks of history
// from backend/seed/ into SQLite on boot, only into empty tables. Falls back to contracts/examples
// when a seed file is missing so a fresh clone always has a demo user.
#pragma once

namespace enro {

struct AppContext;

struct SeedReport {
  int users = 0, contacts = 0, events = 0, outcomes = 0;
};

SeedReport seedDemo(AppContext& ctx);

}  // namespace enro
