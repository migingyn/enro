// Core API routes (PRD 11). Shared file: each route's TODO names the task that replaces its stub.
#include <fstream>
#include <thread>

#include "app/App.hpp"
#include "http/Routes.hpp"
#include "models/Json.hpp"
#include "planner/DayPlanner.hpp"
#include "util/Log.hpp"
#include "util/Time.hpp"

namespace enro {

void registerApiRoutes(CrowApp& app, AppContext& ctx) {
  CROW_ROUTE(app, "/health")([&ctx] {
    auto& p = ctx.providers;
    return jsonResponse({{"status", "ok"},
                         {"version", "0.1.0"},
                         {"uptime_s", time::nowTs() - ctx.started_at},
                         {"providers",
                          {{"voice", p.voice},
                           {"llm", p.llm},
                           {"reflex", p.reflex},
                           {"calendar", p.calendar},
                           {"email", p.email},
                           {"router", p.router},
                           {"transit", p.transit}}}});
  });

  // GET /api/users/:id/plan?date=YYYY-MM-DD
  CROW_ROUTE(app, "/api/users/<string>/plan")([&ctx](const crow::request& req, const std::string& user_id) {
    if (!ctx.repos->user(user_id)) return errorResponse(404, "not_found", "no user " + user_id);
    const char* date = req.url_params.get("date");
    DayPlan day = DayPlanner(ctx).build(user_id, date ? date : "");
    nlohmann::json body = day;
    ctx.dashboard->broadcast("plan", body);
    return jsonResponse(body);
  });

  // GET /api/users/:id/consequences?delay=minutes
  CROW_ROUTE(app, "/api/users/<string>/consequences")([&ctx](const crow::request& req, const std::string& user_id) {
    if (!ctx.repos->user(user_id)) return errorResponse(404, "not_found", "no user " + user_id);
    int delay = 10;
    if (const char* d = req.url_params.get("delay")) {
      try {
        delay = std::stoi(d);
      } catch (...) {
        return errorResponse(400, "bad_request", "delay must be a whole number of minutes");
      }
    }
    return jsonResponse(DayPlanner(ctx).consequences(user_id, delay));
  });

  // GET /api/users/:id/punctuality
  CROW_ROUTE(app, "/api/users/<string>/punctuality")([&ctx](const std::string& user_id) {
    PunctualityReport r = ctx.punctuality->report(user_id);
    // No history loaded yet (backend/seed/demo_history.json missing): show the contract example instead of
    // an empty card.
    if (r.by_event_type.empty()) {
      LOG_WARN("punctuality", "no learned history, serving contracts/examples/punctuality.json");
      return fixtureResponse(ctx, "punctuality");
    }
    return jsonResponse(r);
  });

  // POST /api/contacts/:id/confirm
  CROW_ROUTE(app, "/api/contacts/<string>/confirm").methods(crow::HTTPMethod::POST)([&ctx](const std::string&) {
    // STUB
    // TODO(T10): ctx.contacts->confirm(contact_id); 404 when unknown.
    return fixtureResponse(ctx, "contact_confirm");
  });

  // POST /api/location  (Beacon page, departure verification, P1)
  CROW_ROUTE(app, "/api/location").methods(crow::HTTPMethod::POST)([&ctx](const crow::request& req) {
    if (!jsonBody(req)) return errorResponse(400, "bad_request", "expected JSON body (location_request)");
    // STUB
    // TODO(T10): distance from home; departed when > ~300 m, then cancel DEPARTURE_CHECK nudges.
    return fixtureResponse(ctx, "location");
  });

  // POST /api/demo/trigger  {user_id, scenario}
  CROW_ROUTE(app, "/api/demo/trigger").methods(crow::HTTPMethod::POST)([&ctx](const crow::request& req) {
    auto body = jsonBody(req);
    if (!body) return errorResponse(400, "bad_request", "expected JSON body (demo_trigger_request)");
    std::string user_id = body->value("user_id", "u_demo");
    std::string scenario = body->value("scenario", "");

    nlohmann::json res = ctx.fixtures.json("demo_trigger");
    res["scenario"] = scenario;

    if (scenario == "wake_ladder") {
      res["call_id"] = ctx.wake_ladder->start(user_id);
      res["message"] = "Wake ladder started. CALL_1 ringing now. Open /call/" + res["call_id"].get<std::string>();
    } else if (scenario == "leave_reminder") {
      DayPlan day = DayPlanner(ctx).build(user_id);
      long long now = time::nowTs();
      const LeavePlan* next = day.plans.empty() ? nullptr : &day.plans.front();
      for (auto& p : day.plans)
        if (p.leave_by >= now) { next = &p; break; }
      nlohmann::json input = {{"event_id", next ? next->event_id : ""},
                              {"urgency", next ? nlohmann::json(next->urgency) : nlohmann::json("FIRM")},
                              {"minutes_to_leave", next ? std::max(0LL, (next->leave_by - now) / 60) : 10},
                              {"in_transit", false},
                              {"time_of_day", time::hhmmIn(now, day.timezone)}};
      ctx.recordDecision(user_id, "interrupt_channel", ctx.reflex->interruptChannel(input), input);
      res["call_id"] = ctx.wake_ladder->callNow(user_id, "leave_reminder", 1);
      res["message"] = "Leave reminder call ringing.";
    } else if (scenario == "email_drop") {
      ctx.email->injectDemo("msg_1003");
      auto results = ctx.triage->process(user_id, ctx.email->poll(user_id));
      // STUB
      // TODO(T09): triage updates the event and rebuilds the plan itself; drop this fixture broadcast.
      ctx.dashboard->broadcast("plan", ctx.fixtures.json("ws_plan")["data"]);
      res["call_id"] = "";
      res["message"] = "Dropped 'Section canceled today'. Triaged " + std::to_string(results.size()) + " email(s).";
    } else {
      return errorResponse(400, "bad_scenario", "scenario must be wake_ladder, leave_reminder, or email_drop");
    }
    return jsonResponse(res);
  });

  // POST /api/demo/trip  {trip, speed}
  CROW_ROUTE(app, "/api/demo/trip").methods(crow::HTTPMethod::POST)([&ctx](const crow::request& req) {
    auto body = jsonBody(req);
    if (!body) return errorResponse(400, "bad_request", "expected JSON body (demo_trip_request)");
    std::string trip = body->value("trip", "bus_trip_01");
    double speed = body->value("speed", 10.0);
    TripRunInfo info = ctx.trip_runner->start(trip, speed);
    if (!info.ok) return errorResponse(404, "trip_not_found", "no playable backend/seed/trips/" + trip + ".json");
    return jsonResponse({{"ok", true}, {"trip", trip}, {"speed", speed}, {"ticks", info.ticks}, {"duration_s", info.duration_s}});
  });

  // POST /api/sim/load  {users, threads}
  CROW_ROUTE(app, "/api/sim/load").methods(crow::HTTPMethod::POST)([&ctx](const crow::request& req) {
    auto body = jsonBody(req);
    if (!body) return errorResponse(400, "bad_request", "expected JSON body (sim_load_request)");
    ctx.load_sim->run(body->value("users", 100000), body->value("threads", 8));
    // STUB
    // TODO(T16): return and broadcast the real SimResult from ctx.load_sim->run.
    auto result = ctx.fixtures.json("sim_result");
    ctx.dashboard->broadcast("sim_result", result);
    return jsonResponse(result);
  });

  // GET /audio/:id.mp3
  CROW_ROUTE(app, "/audio/<string>")([&ctx](const std::string& file) {
    std::string id = file.size() > 4 && file.ends_with(".mp3") ? file.substr(0, file.size() - 4) : file;
    auto bytes = ctx.audio->get(id);
    if (!bytes) return errorResponse(404, "not_found", "no cached audio " + id);
    crow::response res(200, *bytes);
    res.set_header("Content-Type", "audio/mpeg");
    return res;
  });
}

}  // namespace enro
