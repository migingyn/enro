#include "model/PunctualityModel.hpp"

namespace enro {

void PunctualityModel::addOutcome(const Outcome&, int) {
  // STUB
  // TODO(T13): update Ewma for prep_bias (actual_leave - planned_leave, minutes), travel_factor
  // ((arrival - actual_leave) / predicted travel), arrival_delta (arrival - event start) by type and by mode.
}

void PunctualityModel::addSnooze(const std::string&, int, int) {
  // STUB
  // TODO(T13): Ewma of actual / requested per event type ("said five, took eleven").
}

void PunctualityModel::addChannelEvent(const std::string&, long long, bool) {
  // STUB
  // TODO(T13): answer rate per channel x time of day (morning < 12:00, day < 18:00, evening).
}

void PunctualityModel::loadHistory(const nlohmann::json&) {
  // STUB
  // TODO(T13): iterate outcomes, wake_ups[].snooze_requests, channel_events (format: prompts/S3-config-seed.md).
}

PunctualityStats PunctualityModel::forEventType(const std::string&) const {
  // STUB
  // TODO(T13): learned stats for this type; neutral defaults (0 bias, factor 1) when no samples.
  return {};
}

PunctualityStats PunctualityModel::forMode(const std::string&) const {
  // STUB
  // TODO(T13): learned stats for this mode.
  return {};
}

PunctualityReport PunctualityModel::report(const std::string& user_id) const {
  // STUB
  // TODO(T13): fill by_event_type, by_mode, channel_response, transport (from TransportLearner), and headlines
  // like "You leave 12 minutes later than planned for class." (see contracts/examples/punctuality.json).
  PunctualityReport r;
  r.user_id = user_id;
  return r;
}

}  // namespace enro
