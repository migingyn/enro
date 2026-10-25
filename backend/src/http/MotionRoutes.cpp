// Phone-motion routes (S7). Sensor Logger's HTTP push posts accelerometer batches here.
//   POST /api/motion/<token>  ingest a batch. Disabled (404) unless MOTION_INGEST_TOKEN is set.
//   GET  /api/motion          current snapshot for the dashboard.
#include <chrono>
#include <mutex>

#include "app/App.hpp"
#include "http/Routes.hpp"
#include "util/Time.hpp"

namespace enro {

namespace {

constexpr size_t kMaxBodyBytes = 2 * 1024 * 1024;  // 2,000 readings is well under this
constexpr int kMaxPostsPerSecond = 20;              // Sensor Logger defaults to one push per second

// Compares every byte regardless of where the first mismatch is, so timing doesn't leak the token prefix.
bool sameToken(const std::string& given, const std::string& expected) {
  unsigned char diff = given.size() == expected.size() ? 0 : 1;
  for (size_t i = 0; i < given.size(); ++i)
    diff |= static_cast<unsigned char>(given[i]) ^ static_cast<unsigned char>(expected[i % expected.size()]);
  return diff == 0;
}

}  // namespace

void registerMotionRoutes(CrowApp& app, AppContext& ctx) {
  CROW_ROUTE(app, "/api/motion")([&ctx] {
    return jsonResponse(toJson(ctx.motion->snapshot(static_cast<double>(time::nowMs()))));
  });

  const std::string token = ctx.env.get("MOTION_INGEST_TOKEN", "");
  CROW_ROUTE(app, "/api/motion/<string>")
      .methods(crow::HTTPMethod::POST)([&ctx, token](const crow::request& req, const std::string& given) {
        if (token.empty()) return errorResponse(404, "not_found", "motion ingest is off (set MOTION_INGEST_TOKEN)");
        if (!sameToken(given, token)) return errorResponse(401, "unauthorized", "bad motion token");
        if (req.body.size() > kMaxBodyBytes) return errorResponse(413, "too_large", "motion batch over 2 MB");

        // Simple fixed one-second window across all callers: there is one phone.
        static std::mutex mu;
        static long long window_start = 0;
        static int posts = 0;
        long long now_ms = time::nowMs();
        {
          std::lock_guard lock(mu);
          if (now_ms - window_start >= 1000) {
            window_start = now_ms;
            posts = 0;
          }
          if (++posts > kMaxPostsPerSecond)
            return errorResponse(429, "rate_limited", "reduce Sensor Logger HTTP push frequency");
        }

        nlohmann::json body;
        try {
          body = nlohmann::json::parse(req.body);
        } catch (...) {
          return errorResponse(400, "bad_request", "expected Sensor Logger JSON");
        }
        MotionIngestResult r = ctx.motion->ingest(body, static_cast<double>(now_ms));
        return jsonResponse({{"ok", true}, {"accepted", r.accepted}, {"ignored", r.ignored}});
      });
}

}  // namespace enro
