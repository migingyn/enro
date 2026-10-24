// models/: the C++ structs round-trip every contract example unchanged. Shared (contracts).
#include <doctest/doctest.h>

#include "config/Env.hpp"
#include "config/Fixtures.hpp"
#include "models/Json.hpp"

using namespace enro;
using nlohmann::json;

template <typename T>
void roundTrips(const json& j) {
  json back = j.get<T>();
  CHECK_MESSAGE(back == j, json::diff(j, back).dump());
}

TEST_CASE("contract examples round-trip through Models.hpp") {
  Fixtures fx(Env::fromMap({}).contractsDir());
  roundTrips<DayPlan>(fx.json("plan"));
  roundTrips<DayPlan>(fx.json("ws_plan")["data"]);
  roundTrips<ConsequenceReport>(fx.json("consequences"));
  roundTrips<PunctualityReport>(fx.json("punctuality"));
  roundTrips<SimResult>(fx.json("sim_result"));
  roundTrips<Decision>(fx.json("ws_decision")["data"]);
  roundTrips<Call>(fx.json("ws_call_state")["data"]);
  roundTrips<Turn>(fx.json("ws_turn")["data"]);
  roundTrips<TripTick>(fx.json("ws_trip_tick")["data"]);
  roundTrips<User>(fx.json("onboarding_complete")["user"]);
  roundTrips<EmergencyContact>(fx.json("contact_confirm")["contact"]);
}
