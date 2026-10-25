#include "google/GmailClient.hpp"

#include <algorithm>
#include <cctype>
#include <set>

#include "google/CalendarClient.hpp"
#include "util/Log.hpp"
#include "util/Time.hpp"

namespace enro {

namespace {

constexpr const char* kGmail = "https://gmail.googleapis.com/gmail/v1/users/me";
constexpr size_t kMaxIcsBytes = 80000;

std::string lower(std::string s) {
  for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string header(const nlohmann::json& payload, const std::string& name) {
  if (!payload.contains("headers")) return "";
  for (auto& h : payload["headers"]) {
    if (lower(h.value("name", "")) == lower(name)) return h.value("value", "");
  }
  return "";
}

void collectParts(const nlohmann::json& part, std::vector<GmailClient::CalendarPart>& out, int& visited) {
  if (!part.is_object() || ++visited > 100) return;
  std::string mime = lower(part.value("mimeType", ""));
  std::string filename = lower(part.value("filename", ""));
  bool is_ics = mime == "text/calendar" || mime == "application/ics" ||
                (filename.size() > 4 && filename.ends_with(".ics"));
  if (is_ics && part.contains("body")) {
    auto& body = part["body"];
    if (body.value("size", 0) <= static_cast<int>(kMaxIcsBytes)) {
      out.push_back({body.value("data", ""), body.value("attachmentId", "")});
    }
  }
  if (part.contains("parts")) {
    for (auto& child : part["parts"]) collectParts(child, out, visited);
  }
}

}  // namespace

GmailClient::GmailClient(GoogleAuth& auth, Repos& repos) : auth_(auth), repos_(repos) {}

std::vector<GmailClient::CalendarPart> GmailClient::calendarParts(const nlohmann::json& payload) {
  std::vector<CalendarPart> out;
  int visited = 0;
  collectParts(payload, out, visited);
  return out;
}

std::string GmailClient::base64UrlDecode(const std::string& in) {
  auto val = [](char c) -> int {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-' || c == '+') return 62;
    if (c == '_' || c == '/') return 63;
    return -1;
  };
  std::string out;
  int buf = 0, bits = 0;
  for (char c : in) {
    int v = val(c);
    if (v < 0) continue;  // padding, whitespace
    buf = (buf << 6) | v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out += static_cast<char>((buf >> bits) & 0xFF);
    }
  }
  return out;
}

std::string GmailClient::fetchAttachment(const std::string& user_id, const std::string& message_id,
                                         const std::string& id) {
  HttpResponse r = auth_.get(user_id, std::string(kGmail) + "/messages/" + urlEncode(message_id) + "/attachments/" +
                                          urlEncode(id));
  if (r.status != 200) return "";
  auto j = nlohmann::json::parse(r.body, nullptr, false);
  return j.is_discarded() ? "" : j.value("data", "");
}

int GmailClient::applyInvites(const std::string& user_id, const std::string& message_id,
                              const std::vector<IcsInvite>& invites, long long now) {
  int written = 0;
  for (auto& inv : latestRevisions(invites)) {
    std::string id = icsEventId(inv.uid);
    // SEQUENCE orders revisions across polls; an older revision arriving late never overwrites a newer one.
    std::string seq_key = "ics_seq:" + id;
    auto prior = repos_.kv(seq_key);
    if (prior && std::stoi(*prior) > inv.sequence) continue;

    EmailFact fact{message_id, user_id, "", now, {{"uid", inv.uid}, {"event_id", id}, {"title", inv.title}}};
    if (inv.status == "cancel") {
      fact.verdict = "INVITE_CANCEL";
      if (auto e = repos_.event(id)) {
        e->canceled = true;
        repos_.upsertEvent(*e);
      }
    } else if (inv.status == "review") {
      fact.verdict = "INVITE_REVIEW";
      fact.extracted["reason"] = inv.reason;
    } else {
      if (inv.start_ts < now || inv.start_ts >= now + CalendarClient::kWindowSeconds) continue;
      Event e;
      e.id = id;
      e.user_id = user_id;
      e.title = inv.title;
      e.location = inv.location;
      e.event_type = CalendarClient::guessEventType(inv.title);
      e.source = "email";
      e.start_ts = inv.start_ts;
      e.end_ts = inv.end_ts;
      repos_.upsertEvent(e);
      ++written;
      fact.verdict = "INVITE";
      fact.extracted["location"] = inv.location;
      fact.extracted["start_ts"] = inv.start_ts;
      fact.extracted["tzid"] = inv.tzid;
    }
    repos_.setKv(seq_key, std::to_string(inv.sequence));
    repos_.upsertEmailFact(fact);
  }
  return written;
}

std::vector<EmailMessage> GmailClient::poll(const std::string& user_id) {
  std::vector<EmailMessage> out;
  std::string history_key = "gmail_history:" + user_id;
  auto history = repos_.kv(history_key);
  std::vector<std::string> ids;
  std::string next_history;

  if (history) {
    HttpResponse r = auth_.get(user_id, std::string(kGmail) + "/history?" +
                                            formEncode({{"startHistoryId", *history},
                                                        {"historyTypes", "messageAdded"},
                                                        {"maxResults", "100"}}));
    if (r.status == 404) {
      history.reset();  // historyId too old: start over like a first run
    } else if (r.status != 200) {
      LOG_WARN("google", "gmail history.list failed for " << user_id << " (HTTP " << r.status << ")");
      return out;
    } else {
      auto j = nlohmann::json::parse(r.body, nullptr, false);
      if (j.is_discarded()) return out;
      next_history = j.value("historyId", *history);
      std::set<std::string> seen;
      for (auto& h : j.value("history", nlohmann::json::array())) {
        for (auto& added : h.value("messagesAdded", nlohmann::json::array())) {
          std::string id = added["message"].value("id", "");
          if (!id.empty() && seen.insert(id).second) ids.push_back(id);
        }
      }
      // Bounded: keep only the newest kMaxMessages (history is oldest first).
      if (ids.size() > kMaxMessages) ids.erase(ids.begin(), ids.end() - kMaxMessages);
    }
  }
  if (!history) {
    HttpResponse prof = auth_.get(user_id, std::string(kGmail) + "/profile");
    HttpResponse list = auth_.get(user_id, std::string(kGmail) + "/messages?" +
                                               formEncode({{"q", "newer_than:2d"},
                                                           {"maxResults", std::to_string(kMaxMessages)},
                                                           {"includeSpamTrash", "false"}}));
    if (prof.status != 200 || list.status != 200) {
      LOG_WARN("google", "gmail first poll failed for " << user_id << " (HTTP " << prof.status << "/" << list.status
                                                         << ")");
      return out;
    }
    auto p = nlohmann::json::parse(prof.body, nullptr, false);
    auto l = nlohmann::json::parse(list.body, nullptr, false);
    if (p.is_discarded() || l.is_discarded()) return out;
    auto hid = p["historyId"];
    next_history = hid.is_string() ? hid.get<std::string>() : hid.is_number() ? std::to_string(hid.get<long long>()) : "";
    for (auto& m : l.value("messages", nlohmann::json::array())) {
      if (ids.size() < kMaxMessages && m.contains("id")) ids.push_back(m["id"].get<std::string>());
    }
  }

  long long now = time::nowTs();
  for (auto& id : ids) {
    HttpResponse r = auth_.get(user_id, std::string(kGmail) + "/messages/" + urlEncode(id) + "?format=full");
    if (r.status != 200) continue;  // deleted since the history entry, or transient; skip
    auto m = nlohmann::json::parse(r.body, nullptr, false);
    if (m.is_discarded()) continue;
    auto payload = m.value("payload", nlohmann::json::object());
    EmailMessage msg;
    msg.id = id;
    msg.from = header(payload, "From");
    msg.subject = header(payload, "Subject");
    msg.snippet = m.value("snippet", "");
    std::string internal = m.value("internalDate", "0");
    // Receive time is metadata for triage only. It never becomes an event time.
    msg.received_ts = std::all_of(internal.begin(), internal.end(), ::isdigit) && !internal.empty()
                          ? std::stoll(internal) / 1000
                          : 0;

    std::vector<IcsInvite> invites;
    for (auto& part : calendarParts(payload)) {
      std::string data = part.data_b64url.empty() && !part.attachment_id.empty()
                             ? fetchAttachment(user_id, id, part.attachment_id)
                             : part.data_b64url;
      std::string ics = base64UrlDecode(data);
      if (ics.size() > kMaxIcsBytes) continue;
      auto parsed = parseIcs(ics);
      invites.insert(invites.end(), parsed.begin(), parsed.end());
    }
    if (!invites.empty()) applyInvites(user_id, id, invites, now);
    out.push_back(msg);
  }
  if (!next_history.empty()) repos_.setKv(history_key, next_history);
  LOG_INFO("google", "gmail: " << out.size() << " new messages for " << user_id);
  return out;
}

}  // namespace enro
