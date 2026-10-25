// Minimal RFC 5545 reader for calendar invitations found in Gmail (text/calendar parts). Owner: T03.
// Only complete, one-off invitations with an unambiguous start become events. Anything we'd have to
// guess (floating or all-day times, unknown or ambiguous timezones, recurrence, missing title or
// location) comes back as "review" so a human looks at it; it is never planned.
// An email's receive time is never used as an event time.
#pragma once

#include <string>
#include <vector>

namespace enro {

struct IcsInvite {
  std::string uid, method;  // method: "" | REQUEST | PUBLISH | CANCEL
  std::string title, location, tzid;
  std::string status;       // "event" | "review" | "cancel"
  std::string reason;       // why it is "review", e.g. "floating_time"
  int sequence = 0;
  long long start_ts = 0, end_ts = 0;  // only meaningful for "event"
};

// Every VEVENT in every VCALENDAR block of text. REPLY/COUNTER/etc. are not invitations and are skipped.
std::vector<IcsInvite> parseIcs(const std::string& text);

// Latest revision per UID: highest SEQUENCE wins; on a tie, CANCEL wins.
std::vector<IcsInvite> latestRevisions(const std::vector<IcsInvite>& invites);

// Epoch seconds for a wall-clock time in an IANA zone, or -1 when the zone is unknown or the time is
// ambiguous (DST fall-back) or nonexistent (spring-forward).
long long resolveLocalTime(int y, int mo, int d, int h, int mi, int s, const std::string& tzid);

// Stable event id for an invitation UID ("email-ics:<fnv1a64 hex>").
std::string icsEventId(const std::string& uid);

}  // namespace enro
