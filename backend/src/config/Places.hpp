// Coordinates for known locations, from backend/config/places.json. Events and home_area are plain
// strings; resolve() turns them into a routable Place by case-insensitive substring match.
#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "routing/Router.hpp"

namespace enro {

class Places {
 public:
  // {"places": [{"match": "Lusk Blvd", "label": "...", "lat": 32.9, "lng": -117.2}, ...]}
  static Places fromConfig(const nlohmann::json& cfg);

  // The first entry whose `match` appears in text, as a Place labeled `text`.
  // No match: a Place with no coordinates (routers fall back to a default distance).
  Place resolve(const std::string& text) const;
  size_t size() const { return entries_.size(); }

 private:
  struct Entry {
    std::string match_lower;
    double lat = 0, lng = 0;
  };
  std::vector<Entry> entries_;
};

}  // namespace enro
