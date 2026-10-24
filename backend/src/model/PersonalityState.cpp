#include "model/PersonalityState.hpp"

#include <algorithm>
#include <map>

namespace enro {

namespace {

struct Step {
  double warmth, harshness, humor, verbosity;
};

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

}  // namespace

PersonalityState::PersonalityState(const User& user)
    : p_{user.archetype_id, user.warmth, user.harshness, user.humor, user.verbosity} {}

void PersonalityState::nudge(const std::string& signal) {
  // Small steps so one bad morning doesn't flip the persona. The caller persists current() to the user row.
  static const std::map<std::string, Step> steps = {
      {"ignored_call", {-0.02, 0.05, -0.02, -0.03}},
      {"answered_first_ring", {0.03, -0.03, 0.02, 0.0}},
      {"asked_gentler", {0.05, -0.08, 0.0, 0.0}},
      {"asked_harsher", {-0.03, 0.08, 0.0, 0.0}},
      {"late", {-0.02, 0.04, -0.02, -0.02}},
      {"on_time", {0.02, -0.02, 0.02, 0.0}},
  };
  auto it = steps.find(signal);
  if (it == steps.end()) return;
  p_.warmth = clamp01(p_.warmth + it->second.warmth);
  p_.harshness = clamp01(p_.harshness + it->second.harshness);
  p_.humor = clamp01(p_.humor + it->second.humor);
  p_.verbosity = clamp01(p_.verbosity + it->second.verbosity);
}

Personality PersonalityState::forUrgency(Urgency urgency) const {
  Personality p = p_;
  double lift = urgency == Urgency::URGENT ? 0.3 : urgency == Urgency::FIRM ? 0.15 : 0.0;
  p.harshness = clamp01(p.harshness + lift);
  p.verbosity = clamp01(p.verbosity - lift);
  p.humor = clamp01(p.humor - lift / 2);
  return p;
}

}  // namespace enro
