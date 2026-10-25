// T04: POLL_CALENDAR / POLL_GMAIL feed calendar and inbox data (seed now, Google later) through the
// same pipeline: calendar sync -> events, inbox -> triage -> plan updates.
#include <doctest/doctest.h>

#include "TestApp.hpp"
#include "google/SeedSources.hpp"
#include "scheduler/Pollers.hpp"
#include "store/Seed.hpp"
#include "util/Time.hpp"

using namespace enro;

namespace {
std::unique_ptr<AppContext> appWithSeedFiles() {
  // The real backend/seed files, so the inbox has messages to feed through.
  auto app = test::fakeApp({{"SEED_DIR", Env::fromMap({}).seedDir()}, {"POLL_SECONDS", "60"}});
  seedDemo(*app);
  return app;
}
int emailDecisions(AppContext& app) {
  int n = 0;
  for (auto& d : app.repos->recentDecisions(500)) n += d.kind == "email_relevance";
  return n;
}
}  // namespace

TEST_CASE("start queues one calendar poll and one inbox poll per user, due now") {
  auto app = appWithSeedFiles();
  long long now = time::nowTs();
  app->pollers->start(now);
  CHECK(app->queue->size() == 2);
  CHECK(app->queue->nextDueAt() == now);
}

TEST_CASE("the inbox poll triages every delivered message once and reschedules itself") {
  auto app = appWithSeedFiles();
  long long now = time::nowTs();
  app->pollers->start(now);
  app->scheduler->tick(now);
  int first = emailDecisions(*app);
  CHECK(first == 14);  // 15 seed messages, msg_1003 held back for the live email drop
  CHECK(app->queue->nextDueAt() == now + 60);
  app->scheduler->tick(now + 60);
  CHECK(emailDecisions(*app) == first);  // nothing new arrived
}

TEST_CASE("the calendar poll adds new events but never undoes a change an email made") {
  auto app = appWithSeedFiles();
  auto dis = *app->repos->event("evt_cse110_dis");
  dis.canceled = true;  // as if "Section canceled today" was triaged
  app->repos->upsertEvent(dis);
  app->repos->upsertEvent({"evt_extra", "u_demo", "Not in the source", "x", "casual", "calendar", 1, 2, false});
  long long now = time::nowTs();
  app->pollers->start(now);
  app->scheduler->tick(now);
  CHECK(app->repos->event("evt_cse110_dis")->canceled);  // still canceled
}

TEST_CASE("stale poll tasks from an earlier boot are ignored, so polls never multiply") {
  auto app = appWithSeedFiles();
  long long now = time::nowTs();
  app->pollers->start(now - 600);  // an earlier boot left tasks behind
  app->pollers->start(now);        // this boot
  app->scheduler->tick(now);
  // Only the current generation reschedules: one calendar + one inbox task remain.
  CHECK(app->queue->size() == 2);
}
