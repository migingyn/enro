#include "config/Places.hpp"

#include <algorithm>
#include <cctype>

namespace enro {

namespace {
std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
  return s;
}
}  // namespace

Places Places::fromConfig(const nlohmann::json& cfg) {
  Places p;
  for (auto& e : cfg.value("places", nlohmann::json::array())) {
    if (!e.contains("match") || !e.contains("lat") || !e.contains("lng")) continue;
    p.entries_.push_back({lower(e["match"].get<std::string>()), e["lat"].get<double>(), e["lng"].get<double>()});
  }
  return p;
}

Place Places::resolve(const std::string& text) const {
  std::string hay = lower(text);
  for (auto& e : entries_) {
    if (!e.match_lower.empty() && hay.find(e.match_lower) != std::string::npos) return {text, e.lat, e.lng};
  }
  return {text, std::nullopt, std::nullopt};
}

}  // namespace enro
