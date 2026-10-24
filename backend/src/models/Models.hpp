// Shared data models. PRD section 10, plus the response shapes from section 11.
//
// CONTRACT FILE. These structs mirror contracts/schemas/*.schema.json and
// frontend/src/api/types.ts. Change all three in the same PR and tell the team
// (see contracts/README.md). JSON conversion lives in models/Json.hpp.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace enro {

// ---------- section 10 ----------

struct User {
  std::string id, name, phone, timezone, home_area, archetype_id;
  std::string alarm_time;                             // user's own alarm, "HH:MM", from onboarding
  int grace_minutes = 5;
  std::map<std::string, std::string> preferred_mode;  // trip type -> mode
  std::map<std::string, int> prep_minutes;            // "casual" | "formal" -> minutes (onboarding answer)
  double warmth = 0.5, harshness = 0.5, humor = 0.5, verbosity = 0.5;
};

enum class ConsentStatus { PENDING, CONFIRMED, REVOKED };
struct EmergencyContact {
  std::string id, user_id, spoken_name, relationship, phone;
  ConsentStatus consent = ConsentStatus::PENDING;
  long long added_ts = 0;
};

struct Event {
  std::string id, user_id, title, location, event_type, source;  // source: calendar or email
  long long start_ts = 0, end_ts = 0;
  bool canceled = false;
};

enum class Urgency { CHILL, FIRM, URGENT };
struct LeavePlan {
  std::string event_id;
  std::string mode;
  int prep_minutes = 0, travel_minutes = 0, buffer_minutes = 0;
  long long reminder_at = 0, leave_by = 0;
  Urgency urgency = Urgency::CHILL;
  int urgency_score = 0;  // 0..100, PRD 7.3
  std::vector<std::string> reasoning;
};

enum class TaskKind { PLAN_TOMORROW, WAKE_CHECK, WAKE_CALL, LEAVE_REMINDER, DEPARTURE_CHECK,
                      TRANSIT_TICK, POLL_CALENDAR, POLL_GMAIL, POST_EVENT_LOG };
struct AgentTask {
  long long due_at = 0;
  std::string user_id;
  TaskKind kind = TaskKind::PLAN_TOMORROW;
  nlohmann::json payload = nlohmann::json::object();
};

struct Decision {
  std::string user_id, kind, verdict, source;  // source: jev or rules
  double confidence = 0;
  long long ts = 0;
  int latency_ms = 0;
  nlohmann::json inputs = nlohmann::json::object();
};

struct Outcome {
  std::string user_id, event_id, event_type, mode;
  long long planned_leave = 0, actual_leave = 0, arrival = 0;
};

struct PunctualityStats {
  double prep_bias = 0, travel_factor = 1, arrival_delta = 0, snooze_inflation = 1;
  int samples = 0;
};

struct Call {
  std::string id, user_id, provider_sid, purpose, state;
  int attempt = 1, turns = 0;
};
struct Turn {
  std::string call_id, speaker, text, intent;
  long long ts = 0;
};

// ---------- section 11 response pieces ----------

struct LadderStep {
  std::string step;  // CALL_1 | CALL_2 | CALL_3 | AWAKE_VERIFY
  long long at = 0;
  std::string tone;  // calm | louder | yelling | check
  bool notify_contacts = false;
};
struct WakePlan {
  std::string alarm_time;
  long long alarm_ts = 0;
  int grace_minutes = 5;
  std::vector<LadderStep> steps;
};

struct DayPlan {  // GET /api/users/:id/plan, ws "plan"
  std::string user_id, date, timezone;
  long long generated_at = 0;
  std::vector<Event> events;
  std::vector<LeavePlan> plans;
  WakePlan wake;
  std::vector<std::string> reasoning;
};

enum class ConsequenceStatus { OK, AT_RISK, MISSED };
struct ConsequenceItem {
  std::string event_id, title;
  long long start_ts = 0;
  ConsequenceStatus status = ConsequenceStatus::OK;
  int minutes_late = 0;
};
struct ConsequenceReport {  // GET /api/users/:id/consequences
  std::string user_id;
  int delay_minutes = 0;
  ConsequenceStatus worst = ConsequenceStatus::OK;
  std::vector<ConsequenceItem> items;
  std::string summary;
};

struct ChannelResponse {
  std::string channel, time_of_day;
  double answer_rate = 0;
  int samples = 0;
};
struct TransportCount {
  std::string destination_type, distance_bucket;
  std::map<std::string, int> counts;
  std::string predicted;
};
struct PunctualityReport {  // GET /api/users/:id/punctuality
  std::string user_id;
  std::map<std::string, PunctualityStats> by_event_type;
  std::map<std::string, PunctualityStats> by_mode;
  std::vector<ChannelResponse> channel_response;
  std::vector<TransportCount> transport;
  std::vector<std::string> headlines;
};

struct Reroute {
  std::string reason, suggestion;
  long long new_eta_ts = 0;
};
struct TripTick {  // ws "trip_tick"
  std::string trip_id;
  int seq = 0;
  long long ts = 0;
  double lat = 0, lng = 0;
  std::string route, next_stop;
  long long eta_ts = 0, scheduled_arrival_ts = 0;
  int delay_minutes = 0;
  bool on_route = true;
  std::string verdict;  // IGNORE | COMMENT | ALERT | REROUTE
  std::string line;
  std::optional<Reroute> reroute;
};

struct CallsPerMinuteBucket {
  std::string minute;
  int calls = 0;
};
struct SimResult {  // POST /api/sim/load, ws "sim_result"
  std::string run_id;
  int users = 0, threads = 0;
  long long runtime_ms = 0, tasks_scheduled = 0, llm_calls = 0;
  std::vector<CallsPerMinuteBucket> calls_per_minute;
  double cost_per_user_month_usd = 0;
  std::map<std::string, double> cost_breakdown;
};

}  // namespace enro
