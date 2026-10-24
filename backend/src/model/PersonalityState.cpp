#include "model/PersonalityState.hpp"

namespace enro {

PersonalityState::PersonalityState(const User& user)
    : p_{user.archetype_id, user.warmth, user.harshness, user.humor, user.verbosity} {}

void PersonalityState::nudge(const std::string&) {
  // STUB
  // TODO(T13): small clamped steps per signal (e.g. ignored_call -> harshness +0.05), persist to the user row.
}

Personality PersonalityState::forUrgency(Urgency) const {
  // STUB
  // TODO(T13): shift harshness up and verbosity down as urgency rises; clamp to 0..1.
  return p_;
}

}  // namespace enro
