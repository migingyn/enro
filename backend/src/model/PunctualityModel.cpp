#include "model/PunctualityModel.hpp"

#include <algorithm>
#include <cmath>

#include "util/Time.hpp"

namespace enro {

namespace {

PunctualityStats statsOf(const Ewma& prep, const Ewma& travel, const Ewma& arrival, const Ewma& snooze, int samples) {
  PunctualityStats s;
  s.prep_bias = prep.value();
  s.travel_factor = travel.samples() ? travel.value() : 1.0;
  s.arrival_delta = arrival.value();
  s.snooze_inflation = snooze.samples() ? snooze.value() : 1.0;
  s.samples = samples;
  return s;
}

double round2(double v) { return std::round(v * 100.0) / 100.0; }

std::string countWord(int n) {
  static const char* words[] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
                                "ten",  "eleven", "twelve", "thirteen", "fourteen", "fifteen"};
  return n >= 0 && n <= 15 ? words[n] : std::to_string(n);
}

std::string tripNoun(const std::string& mode) {
  if (mode == "walk") return "walks";
  if (mode == "drive") return "drives";
  return mode + " rides";
}

// Seed history has no distances, so estimate one from predicted travel time and typical speed.
int estimateMeters(const std::string& mode, int travel_minutes) {
  static const std::map<std::string, double> kmh = {{"walk", 4.8}, {"bike", 15}, {"bus", 18}, {"drive", 35}};
  auto it = kmh.find(mode);
  return static_cast<int>((it == kmh.end() ? 4.8 : it->second) * 1000.0 * travel_minutes / 60.0);
}

}  // namespace

std::string PunctualityModel::timeOfDay(long long ts) {
  int hour = std::stoi(time::hhmmIn(ts, "America/Los_Angeles").substr(0, 2));
  return hour < 12 ? "morning" : hour < 18 ? "day" : "evening";
}

void PunctualityModel::addOutcome(const Outcome& o, int predicted_travel_minutes, long long event_start) {
  auto& t = types_[o.event_type];
  t.prep.add((o.actual_leave - o.planned_leave) / 60.0);
  if (predicted_travel_minutes > 0 && o.arrival > o.actual_leave) {
    double factor = (o.arrival - o.actual_leave) / 60.0 / predicted_travel_minutes;
    t.travel.add(factor);
    if (!o.mode.empty()) modes_[o.mode].travel.add(factor);
  }
  if (event_start > 0) t.arrival.add((o.arrival - event_start) / 60.0);
  if (transport_ && !o.mode.empty() && predicted_travel_minutes > 0)
    transport_->record(o.event_type, estimateMeters(o.mode, predicted_travel_minutes), o.mode);
}

void PunctualityModel::addSnooze(const std::string& event_type, int requested, int actual) {
  if (requested <= 0) return;
  auto& t = types_[event_type];
  t.snooze.add(static_cast<double>(actual) / requested);
  t.recent.push_back({requested, actual});
  if (t.recent.size() > 10) t.recent.pop_front();
}

void PunctualityModel::addChannelEvent(const std::string& channel, long long ts, bool answered) {
  auto& r = channels_[{channel, timeOfDay(ts)}];
  r.total += 1;
  r.answered += answered ? 1 : 0;
}

void PunctualityModel::loadHistory(const nlohmann::json& h) {
  for (auto& o : h.value("outcomes", nlohmann::json::array())) {
    Outcome out;
    out.user_id = o.value("user_id", "");
    out.event_id = o.value("event_id", "");
    out.event_type = o.value("event_type", "");
    out.mode = o.value("mode", "");
    out.planned_leave = o.value("planned_leave", 0LL);
    out.actual_leave = o.value("actual_leave", 0LL);
    out.arrival = o.value("arrival", 0LL);
    addOutcome(out, o.value("predicted_travel_minutes", 0), o.value("start_ts", 0LL));
  }
  for (auto& w : h.value("wake_ups", nlohmann::json::array())) {
    std::string type = w.value("event_type", "class");  // wake-ups are for the first thing of the day
    for (auto& s : w.value("snooze_requests", nlohmann::json::array()))
      addSnooze(type, s.value("requested_minutes", 0), s.value("actual_minutes", 0));
  }
  for (auto& c : h.value("channel_events", nlohmann::json::array()))
    addChannelEvent(c.value("channel", "call"), c.value("ts", 0LL), c.value("answered", false));
}

PunctualityStats PunctualityModel::forEventType(const std::string& event_type) const {
  auto it = types_.find(event_type);
  if (it == types_.end()) return {};
  auto& t = it->second;
  return statsOf(t.prep, t.travel, t.arrival, t.snooze, std::max(t.prep.samples(), t.snooze.samples()));
}

PunctualityStats PunctualityModel::forMode(const std::string& mode) const {
  auto it = modes_.find(mode);
  if (it == modes_.end()) return {};
  PunctualityStats s;
  s.travel_factor = it->second.travel.value();
  s.snooze_inflation = 0;
  s.samples = it->second.travel.samples();
  return s;
}

std::vector<SnoozeSample> PunctualityModel::recentSnoozes(const std::string& event_type) const {
  auto it = types_.find(event_type);
  if (it == types_.end()) return {};
  return {it->second.recent.begin(), it->second.recent.end()};
}

PunctualityReport PunctualityModel::report(const std::string& user_id) const {
  PunctualityReport r;
  r.user_id = user_id;
  for (auto& [type, t] : types_) {
    auto s = forEventType(type);
    s.prep_bias = round2(s.prep_bias);
    s.travel_factor = round2(s.travel_factor);
    s.arrival_delta = round2(s.arrival_delta);
    s.snooze_inflation = round2(s.snooze_inflation);
    r.by_event_type[type] = s;
  }
  for (auto& [mode, m] : modes_) {
    auto s = forMode(mode);
    s.travel_factor = round2(s.travel_factor);
    r.by_mode[mode] = s;
  }
  for (auto& [key, rate] : channels_)
    r.channel_response.push_back({key.first, key.second, round2(double(rate.answered) / rate.total), rate.total});
  if (transport_) r.transport = transport_->counts();

  // Headlines: the biggest prep bias, the most-off travel mode, and the snooze habit.
  std::string worst_type;
  double worst_bias = 1.5;
  for (auto& [type, s] : r.by_event_type)
    if (s.samples >= 3 && s.prep_bias > worst_bias) worst_type = type, worst_bias = s.prep_bias;
  if (!worst_type.empty())
    r.headlines.push_back("You leave " + std::to_string(std::lround(worst_bias)) + " minutes later than planned for " +
                          worst_type + ".");

  std::string slow_mode;
  int slow_samples = 0;
  for (auto& [mode, s] : r.by_mode)
    if (s.travel_factor >= 1.05 && s.samples > slow_samples) slow_mode = mode, slow_samples = s.samples;
  if (!slow_mode.empty())
    r.headlines.push_back("Your " + tripNoun(slow_mode) + " take " +
                          std::to_string(std::lround((r.by_mode[slow_mode].travel_factor - 1) * 100)) +
                          "% longer than the map says.");

  for (auto& [type, t] : types_) {
    if (t.recent.size() < 3) continue;
    auto last = std::vector<SnoozeSample>(t.recent.end() - 3, t.recent.end());
    bool same = std::all_of(last.begin(), last.end(), [&](auto& s) { return s.requested_minutes == last[0].requested_minutes; });
    if (!same) continue;
    int avg = 0;
    for (auto& s : last) avg += s.actual_minutes;
    avg = static_cast<int>(std::lround(avg / 3.0));
    if (avg <= last[0].requested_minutes) continue;
    r.headlines.push_back("Last three times you said " + countWord(last[0].requested_minutes) + " more minutes, it was " +
                          countWord(avg) + ".");
    break;
  }
  return r;
}

}  // namespace enro
