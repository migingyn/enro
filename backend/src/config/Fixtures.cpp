#include "config/Fixtures.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "util/Log.hpp"

namespace enro {

Fixtures::Fixtures(std::string contracts_dir) : dir_(std::move(contracts_dir)) {}

nlohmann::json Fixtures::json(const std::string& name) const {
  std::lock_guard<std::mutex> lock(mu_);
  if (auto it = cache_.find(name); it != cache_.end()) return it->second;
  auto path = dir_ + "/examples/" + name + ".json";
  auto parsed = readJsonFile(path);
  if (!parsed) throw std::runtime_error("fixture not found or invalid: " + path);
  cache_[name] = *parsed;
  return *parsed;
}

std::string Fixtures::text(const std::string& relative_path) const {
  std::ifstream f(dir_ + "/examples/" + relative_path);
  if (!f) return "";
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

std::optional<nlohmann::json> Fixtures::readJsonFile(const std::string& path) {
  std::ifstream f(path);
  if (!f) return std::nullopt;
  try {
    return nlohmann::json::parse(f);
  } catch (const std::exception& e) {
    LOG_WARN("fixtures", "invalid JSON in " << path << ": " << e.what());
    return std::nullopt;
  }
}

}  // namespace enro
