// config/Env + Fixtures. Shared.
#include <doctest/doctest.h>

#include "config/Env.hpp"
#include "config/Fixtures.hpp"

using namespace enro;

TEST_CASE("Env::parse handles comments, quotes, export, and empty values") {
  auto m = Env::parse(
      "# comment\nPORT=8080\nexport LLM_PROVIDER=fake\nNAME=\"Sam Rivera\"\n"
      "PUBLIC_BASE_URL=                # only a comment\nDB_PATH=./enro.db  # inline\n");
  CHECK(m["PORT"] == "8080");
  CHECK(m["LLM_PROVIDER"] == "fake");
  CHECK(m["NAME"] == "Sam Rivera");
  CHECK(m["PUBLIC_BASE_URL"].empty());
  CHECK(m["DB_PATH"] == "./enro.db");
}

TEST_CASE("Env defaults are the fake providers from PRD 12") {
  auto env = Env::fromMap({});
  CHECK(env.port() == 8080);
  CHECK(env.demoMode());
  CHECK(env.get("VOICE_PROVIDER", "sim") == "sim");
  CHECK_FALSE(env.has("OPENAI_API_KEY"));
  CHECK(Env::fromMap({{"DB_PATH", ":memory:"}}).dbPath() == ":memory:");
}

TEST_CASE("Fixtures load every contract example") {
  Fixtures fx(Env::fromMap({}).contractsDir());
  CHECK(fx.json("plan")["user_id"] == "u_demo");
  CHECK(fx.text("twiml/voice.xml").find("<Gather") != std::string::npos);
  CHECK_THROWS(fx.json("does_not_exist"));
}
