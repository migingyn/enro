// Live personality parameters (PRD 7.10): warmth, harshness, humor, verbosity, each 0..1.
// Start from the archetype in backend/config/personas.json, nudged by replies and outcomes.
// Owner: T13 (P1: personality adaptation).
#pragma once

#include <string>

#include "models/Models.hpp"

namespace enro {

struct Personality {
  std::string archetype_id;
  double warmth = 0.5, harshness = 0.5, humor = 0.5, verbosity = 0.5;
};

class PersonalityState {
 public:
  explicit PersonalityState(const User& user);

  Personality current() const { return p_; }
  // signal: e.g. "ignored_call", "answered_first_ring", "asked_gentler", "late", "on_time".
  void nudge(const std::string& signal);
  // Per-message shift by urgency (harsher and shorter when URGENT), without changing the stored state.
  Personality forUrgency(Urgency urgency) const;

 private:
  Personality p_;
};

}  // namespace enro
