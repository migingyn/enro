// Shared test helper: a full AppContext in fake mode with an in-memory database.
#pragma once

#include <memory>

#include "app/App.hpp"

namespace enro::test {

inline std::unique_ptr<AppContext> fakeApp(std::map<std::string, std::string> extra = {}) {
  std::map<std::string, std::string> env = {{"DB_PATH", ":memory:"}};
  for (auto& [k, v] : extra) env[k] = v;
  return buildApp(Env::fromMap(env));
}

}  // namespace enro::test
