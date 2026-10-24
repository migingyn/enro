// Seed calendar and inbox (CALENDAR_SOURCE=seed, EMAIL_SOURCE=seed, the defaults).
// Read backend/seed/demo_calendar.json and demo_emails.json (formats in prompts/S3-config-seed.md).
// If a seed file is missing, fall back to the events in contracts/examples/plan.json.
#pragma once

#include <mutex>
#include <set>

#include "config/Fixtures.hpp"
#include "google/CalendarSource.hpp"
#include "google/EmailSource.hpp"

namespace enro {

class SeedCalendar : public CalendarSource {
 public:
  SeedCalendar(const std::string& seed_dir, const Fixtures& fixtures);
  std::string name() const override { return "seed"; }
  std::vector<Event> events(const std::string& user_id, long long from_ts, long long to_ts) override;

 private:
  std::vector<Event> all_;
};

class SeedEmail : public EmailSource {
 public:
  // Messages listed in `held_back` only arrive after injectDemo(id). Default: the demo's
  // "Section canceled today" email (msg_1003).
  SeedEmail(const std::string& seed_dir, std::set<std::string> held_back = {"msg_1003"});
  std::string name() const override { return "seed"; }
  std::vector<EmailMessage> poll(const std::string& user_id) override;
  bool injectDemo(const std::string& message_id) override;

 private:
  std::mutex mu_;
  std::vector<EmailMessage> all_;
  std::set<std::string> held_back_, delivered_;
};

}  // namespace enro
