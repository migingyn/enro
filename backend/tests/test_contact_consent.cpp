// T10: Emergency contact consent gate (PRD 7.7). These safety checks must stay green.
#include <doctest/doctest.h>

#include "TestApp.hpp"
#include "store/Seed.hpp"

using namespace enro;

namespace {
void addContact(AppContext& app, const std::string& id, ConsentStatus consent, const std::string& phone) {
  app.repos->upsertContact({id, "u_demo", id, "friend", phone, consent, 0});
}
std::unique_ptr<AppContext> appWithContacts() {
  auto app = test::fakeApp();
  seedDemo(*app);  // Mom CONFIRMED, Jules PENDING
  addContact(*app, "r", ConsentStatus::REVOKED, "+15550000002");
  return app;
}
}  // namespace

TEST_CASE("PENDING and REVOKED contacts are never notifiable or messaged") {
  auto app = appWithContacts();
  for (auto& c : app->contacts->notifiable("u_demo")) CHECK(c.consent == ConsentStatus::CONFIRMED);
  app->contacts->notifyAll("u_demo", "8:00");
  for (auto& sms : app->sim->sentSms()) {
    CHECK(sms["to"] != "+18585550199");  // Jules, PENDING
    CHECK(sms["to"] != "+15550000002");  // REVOKED
  }
}

TEST_CASE("CONFIRMED contacts get exactly one SMS each") {
  auto app = appWithContacts();
  CHECK(app->contacts->notifyAll("u_demo", "8:00") == 1);
  auto sms = app->sim->sentSms();
  REQUIRE(sms.size() == 1);
  CHECK(sms[0]["to"] == "+16195554417");
}

TEST_CASE("SMS says it's automated from Enro, carries only the hint, and offers STOP") {
  auto app = appWithContacts();
  app->contacts->notifyAll("u_demo", "8:00");
  std::string body = app->sim->sentSms().at(0)["body"];
  CHECK(body.find("Enro") != std::string::npos);
  CHECK(body.find("Automated message") != std::string::npos);
  CHECK(body.find("8:00") != std::string::npos);
  CHECK(body.find("STOP") != std::string::npos);
  CHECK(body.find("CSE") == std::string::npos);  // no calendar details
  CHECK(body.find("Qualcomm") == std::string::npos);
}

TEST_CASE("confirm() moves PENDING to CONFIRMED and sends the 'you were added' SMS once") {
  auto app = appWithContacts();
  auto c = app->contacts->confirm("c_jules");
  REQUIRE(c);
  CHECK(c->consent == ConsentStatus::CONFIRMED);
  app->contacts->confirm("c_jules");
  int added = 0;
  for (auto& s : app->sim->sentSms()) added += s["to"] == "+18585550199";
  CHECK(added == 1);
  CHECK_FALSE(app->contacts->confirm("nope"));
  CHECK(app->contacts->confirm("r")->consent == ConsentStatus::REVOKED);  // revoked stays revoked
}

TEST_CASE("STOP revokes and later notifyAll skips the contact") {
  auto app = appWithContacts();
  app->contacts->revokeByPhone("+16195554417");
  CHECK(app->contacts->notifiable("u_demo").empty());
  CHECK(app->contacts->notifyAll("u_demo", "8:00") == 0);
  CHECK(app->sim->sentSms().empty());
}
