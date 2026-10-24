// nlohmann/json conversions for models/Models.hpp. Field names match contracts/schemas.
// Usage: nlohmann::json j = plan;  auto p = j.get<enro::DayPlan>();
#pragma once

#include "models/Models.hpp"

namespace nlohmann {
// std::optional<T> <-> null or T (used by TripTick::reroute)
template <typename T>
struct adl_serializer<std::optional<T>> {
  static void to_json(json& j, const std::optional<T>& v) { j = v ? json(*v) : json(nullptr); }
  static void from_json(const json& j, std::optional<T>& v) {
    v = j.is_null() ? std::nullopt : std::optional<T>(j.get<T>());
  }
};
}  // namespace nlohmann

namespace enro {

NLOHMANN_JSON_SERIALIZE_ENUM(ConsentStatus, {{ConsentStatus::PENDING, "PENDING"},
                                             {ConsentStatus::CONFIRMED, "CONFIRMED"},
                                             {ConsentStatus::REVOKED, "REVOKED"}})
NLOHMANN_JSON_SERIALIZE_ENUM(Urgency, {{Urgency::CHILL, "CHILL"}, {Urgency::FIRM, "FIRM"}, {Urgency::URGENT, "URGENT"}})
NLOHMANN_JSON_SERIALIZE_ENUM(TaskKind, {{TaskKind::PLAN_TOMORROW, "PLAN_TOMORROW"},
                                        {TaskKind::WAKE_CHECK, "WAKE_CHECK"},
                                        {TaskKind::WAKE_CALL, "WAKE_CALL"},
                                        {TaskKind::LEAVE_REMINDER, "LEAVE_REMINDER"},
                                        {TaskKind::DEPARTURE_CHECK, "DEPARTURE_CHECK"},
                                        {TaskKind::TRANSIT_TICK, "TRANSIT_TICK"},
                                        {TaskKind::POLL_CALENDAR, "POLL_CALENDAR"},
                                        {TaskKind::POLL_GMAIL, "POLL_GMAIL"},
                                        {TaskKind::POST_EVENT_LOG, "POST_EVENT_LOG"}})
NLOHMANN_JSON_SERIALIZE_ENUM(ConsequenceStatus, {{ConsequenceStatus::OK, "OK"},
                                                 {ConsequenceStatus::AT_RISK, "AT_RISK"},
                                                 {ConsequenceStatus::MISSED, "MISSED"}})

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(User, id, name, phone, timezone, home_area, archetype_id,
                                                alarm_time, grace_minutes, preferred_mode, prep_minutes,
                                                warmth, harshness, humor, verbosity)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EmergencyContact, id, user_id, spoken_name, relationship, phone,
                                                consent, added_ts)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Event, id, user_id, title, location, event_type, source,
                                                start_ts, end_ts, canceled)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(LeavePlan, event_id, mode, prep_minutes, travel_minutes,
                                                buffer_minutes, reminder_at, leave_by, urgency, urgency_score,
                                                reasoning)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AgentTask, due_at, user_id, kind, payload)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Decision, user_id, kind, verdict, source, confidence, ts,
                                                latency_ms, inputs)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Outcome, user_id, event_id, event_type, mode, planned_leave,
                                                actual_leave, arrival)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PunctualityStats, prep_bias, travel_factor, arrival_delta,
                                                snooze_inflation, samples)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Call, id, user_id, provider_sid, purpose, state, attempt, turns)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Turn, call_id, speaker, text, intent, ts)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(LadderStep, step, at, tone, notify_contacts)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(WakePlan, alarm_time, alarm_ts, grace_minutes, steps)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(DayPlan, user_id, date, timezone, generated_at, events, plans,
                                                wake, reasoning)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConsequenceItem, event_id, title, start_ts, status, minutes_late)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConsequenceReport, user_id, delay_minutes, worst, items, summary)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ChannelResponse, channel, time_of_day, answer_rate, samples)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(TransportCount, destination_type, distance_bucket, counts,
                                                predicted)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PunctualityReport, user_id, by_event_type, by_mode,
                                                channel_response, transport, headlines)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Reroute, reason, suggestion, new_eta_ts)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(TripTick, trip_id, seq, ts, lat, lng, route, next_stop, eta_ts,
                                                scheduled_arrival_ts, delay_minutes, on_route, verdict, line,
                                                reroute)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CallsPerMinuteBucket, minute, calls)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SimResult, run_id, users, threads, runtime_ms, tasks_scheduled,
                                                llm_calls, calls_per_minute, cost_per_user_month_usd,
                                                cost_breakdown)

}  // namespace enro
