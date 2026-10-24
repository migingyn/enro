#include "config/Env.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "util/Log.hpp"

extern char** environ;

#ifndef ENRO_REPO_ROOT
#define ENRO_REPO_ROOT "."
#endif

namespace enro {

namespace {

std::string trim(const std::string& s) {
  auto b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  auto e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

std::string defaultRoot() {
  if (const char* r = std::getenv("ENRO_ROOT"); r && *r) return r;
  return ENRO_REPO_ROOT;
}

}  // namespace

std::map<std::string, std::string> Env::parse(const std::string& text) {
  std::map<std::string, std::string> out;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#') continue;
    if (line.rfind("export ", 0) == 0) line = trim(line.substr(7));
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string key = trim(line.substr(0, eq));
    std::string value = trim(line.substr(eq + 1));
    if (!value.empty() && value.front() == '#') {
      value.clear();  // FOO=   # only a comment
    } else if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'') && value.back() == value.front()) {
      value = value.substr(1, value.size() - 2);
    } else if (auto hash = value.find(" #"); hash != std::string::npos) {
      value = trim(value.substr(0, hash));  // inline comment: FOO=bar  # note
    }
    if (!key.empty()) out[key] = value;
  }
  return out;
}

Env Env::load() {
  Env env;
  std::string root = defaultRoot();
  std::string file = std::getenv("ENRO_ENV_FILE") ? std::getenv("ENRO_ENV_FILE") : root + "/.env";
  if (std::ifstream f(file); f) {
    std::stringstream ss;
    ss << f.rdbuf();
    env.values_ = parse(ss.str());
    LOG_INFO("env", "loaded " << env.values_.size() << " values from " << file);
  } else {
    LOG_INFO("env", "no .env at " << file << ", using defaults (fake providers)");
  }
  for (char** e = environ; e && *e; ++e) {
    std::string kv = *e;
    auto eq = kv.find('=');
    if (eq != std::string::npos) env.values_[kv.substr(0, eq)] = kv.substr(eq + 1);
  }
  return env;
}

Env Env::fromMap(std::map<std::string, std::string> values) {
  Env env;
  env.values_ = std::move(values);
  return env;
}

std::string Env::get(const std::string& key, const std::string& fallback) const {
  auto it = values_.find(key);
  return it == values_.end() || it->second.empty() ? fallback : it->second;
}

int Env::getInt(const std::string& key, int fallback) const {
  try {
    return has(key) ? std::stoi(get(key)) : fallback;
  } catch (...) {
    LOG_WARN("env", key << " is not a number, using " << fallback);
    return fallback;
  }
}

bool Env::getBool(const std::string& key, bool fallback) const {
  if (!has(key)) return fallback;
  std::string v = get(key);
  std::transform(v.begin(), v.end(), v.begin(), ::tolower);
  return v == "1" || v == "true" || v == "yes" || v == "on";
}

bool Env::has(const std::string& key) const {
  auto it = values_.find(key);
  return it != values_.end() && !it->second.empty();
}

std::string Env::root() const { return get("ENRO_ROOT", ENRO_REPO_ROOT); }
std::string Env::contractsDir() const { return get("CONTRACTS_DIR", root() + "/contracts"); }
std::string Env::configDir() const { return get("CONFIG_DIR", root() + "/backend/config"); }
std::string Env::seedDir() const { return get("SEED_DIR", root() + "/backend/seed"); }
std::string Env::dbPath() const {
  std::string p = get("DB_PATH", root() + "/enro.db");
  if (p == ":memory:") return p;
  // Relative DB_PATH (PRD default ./enro.db) is relative to the repo root, not the cwd.
  return std::filesystem::path(p).is_relative() ? root() + "/" + p : p;
}

}  // namespace enro
