// Loads example JSON from contracts/examples/. Stub routes return these so the
// frontend and backend agree on shapes from hour one (PRD 15.1).
#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace enro {

class Fixtures {
 public:
  explicit Fixtures(std::string contracts_dir);

  // contracts/examples/<name>.json. Throws std::runtime_error if missing or invalid.
  nlohmann::json json(const std::string& name) const;
  // contracts/examples/<relative path> as raw text (e.g. "twiml/voice.xml"). Empty if missing.
  std::string text(const std::string& relative_path) const;

  // Reads any JSON file (used for backend/config and backend/seed). nullopt if missing or invalid.
  static std::optional<nlohmann::json> readJsonFile(const std::string& path);

 private:
  std::string dir_;
  mutable std::mutex mu_;
  mutable std::map<std::string, nlohmann::json> cache_;
};

}  // namespace enro
