// Shared test helper: a full AppContext in fake mode with an in-memory database.
#pragma once

#include <memory>

#include "app/App.hpp"

namespace enro::test {

// SEED_DIR points nowhere by default, so unit tests don't depend on the demo data in backend/seed:
// seeding falls back to contracts/examples (same demo user and day) with no learned history.
// Pass {"SEED_DIR", Env::fromMap({}).seedDir()} to test against the real seed files.
inline std::unique_ptr<AppContext> fakeApp(std::map<std::string, std::string> extra = {}) {
  std::map<std::string, std::string> env = {{"DB_PATH", ":memory:"}, {"SEED_DIR", "/nonexistent-enro-seed"}};
  for (auto& [k, v] : extra) env[k] = v;
  return buildApp(Env::fromMap(env));
}

}  // namespace enro::test
