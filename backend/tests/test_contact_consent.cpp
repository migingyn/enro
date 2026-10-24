// T10: Emergency contact consent gate (PRD 7.7). These safety checks must stay green.
#include <doctest/doctest.h>

#include "TestApp.hpp"

using namespace enro;

namespace {
void addContact(AppContext& app, const std::string& id, ConsentStatus consent) {
  app.repos->upsertContact({id, "u_demo", id, "friend", "+1555000" + id, consent, 0});
}
}  // namespace

TEST_CASE("PENDING and REVOKED contacts are never notifiable or messaged") {
  auto app = test::fakeApp();
  addContact(*app, "p", ConsentStatus::PENDING);
  addContact(*app, "r", ConsentStatus::REVOKED);
  for (auto& c : app->contacts->notifiable("u_demo")) CHECK(c.consent == ConsentStatus::CONFIRMED);
  app->contacts->notifyAll("u_demo", "they have something at 9");
  for (auto& sms : app->sim->sentSms()) {
    CHECK(sms["to"] != "+1555000p");
    CHECK(sms["to"] != "+1555000r");
  }
}

TEST_CASE("TODO(T10): CONFIRMED contacts get exactly one SMS on CALL_3" * doctest::skip()) {}
TEST_CASE("TODO(T10): SMS says it's an automated message from Enro and includes no calendar details beyond the hint" * doctest::skip()) {}
TEST_CASE("TODO(T10): confirm() sends the one-time 'you were added' SMS once" * doctest::skip()) {}
TEST_CASE("TODO(T10): STOP revokes and later notifyAll skips the contact" * doctest::skip()) {}
