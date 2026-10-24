// Environment loader (PRD 12). Reads .env at the repo root, then real environment
// variables override it. An empty or missing .env means every provider is fake.
#pragma once

#include <map>
#include <string>

namespace enro {

class Env {
 public:
  // Loads <root>/.env (or the file named by ENRO_ENV_FILE) and overlays the process environment.
  static Env load();
  // For tests: no file, only the given values (process environment is ignored).
  static Env fromMap(std::map<std::string, std::string> values);
  // Parses KEY=VALUE lines. Supports comments (#), blank lines, `export `, and quoted values.
  static std::map<std::string, std::string> parse(const std::string& text);

  std::string get(const std::string& key, const std::string& fallback = "") const;
  int getInt(const std::string& key, int fallback) const;
  bool getBool(const std::string& key, bool fallback) const;
  bool has(const std::string& key) const;  // set and non-empty

  // Paths. Each can be overridden by the env var of the same name.
  std::string root() const;          // ENRO_ROOT, default: repo root baked in at build time
  std::string contractsDir() const;  // CONTRACTS_DIR, default <root>/contracts
  std::string configDir() const;     // CONFIG_DIR, default <root>/backend/config
  std::string seedDir() const;       // SEED_DIR, default <root>/backend/seed

  // Common values with PRD 12 defaults.
  int port() const { return getInt("PORT", 8080); }
  std::string dbPath() const;  // DB_PATH, default <root>/enro.db
  bool demoMode() const { return getBool("DEMO_MODE", true); }

 private:
  std::map<std::string, std::string> values_;
};

}  // namespace enro
