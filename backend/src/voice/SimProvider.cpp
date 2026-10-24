#include "voice/SimProvider.hpp"

#include "models/Json.hpp"
#include "util/Log.hpp"
#include "util/Time.hpp"

namespace enro {

std::string SimProvider::placeCall(const CallRequest& req) {
  LOG_INFO("sim", "ringing " << req.call_id << " (" << req.purpose << ", attempt " << req.attempt << ")");
  send(req.call_id, "ring", {{"call_id", req.call_id}, {"purpose", req.purpose}, {"attempt", req.attempt}, {"caller", "Enro"}});
  return "SIM-" + req.call_id;
}

void SimProvider::say(const std::string& call_id, const std::string& text, const std::string& persona, Urgency urgency,
                      const std::string& audio_url) {
  send(call_id, "say",
       {{"call_id", call_id},
        {"text", text},
        {"audio_url", audio_url.empty() ? nlohmann::json(nullptr) : nlohmann::json(audio_url)},
        {"persona", persona},
        {"urgency", urgency}});
}

void SimProvider::hangup(const std::string& call_id, const std::string& reason) {
  send(call_id, "hangup", {{"call_id", call_id}, {"reason", reason}});
}

std::string SimProvider::sendSms(const std::string& to_phone, const std::string& body) {
  std::lock_guard<std::mutex> lock(mu_);
  std::string id = "SIM-SMS-" + std::to_string(sms_.size() + 1);
  sms_.push_back({{"id", id}, {"to", to_phone}, {"body", body}, {"ts", time::nowTs()}});
  LOG_INFO("sim", "SMS to ..." << to_phone.substr(to_phone.size() >= 4 ? to_phone.size() - 4 : 0) << ": " << body);
  return id;
}

std::vector<nlohmann::json> SimProvider::sentSms() const {
  std::lock_guard<std::mutex> lock(mu_);
  return sms_;
}

void SimProvider::attachBrowser(const std::string& call_id, void* conn, Sender sender) {
  std::vector<std::string> queued;
  {
    std::lock_guard<std::mutex> lock(mu_);
    browsers_[call_id].emplace_back(conn, sender);
    queued.swap(pending_[call_id]);
  }
  for (auto& msg : queued) sender(msg);
}

void SimProvider::detachBrowser(void* conn) {
  std::lock_guard<std::mutex> lock(mu_);
  for (auto& [id, list] : browsers_) {
    std::erase_if(list, [conn](auto& p) { return p.first == conn; });
  }
}

void SimProvider::onBrowserMessage(const std::string& call_id, const std::string& raw) {
  nlohmann::json msg;
  try {
    msg = nlohmann::json::parse(raw);
  } catch (...) {
    LOG_WARN("sim", "ignoring non-JSON message on " << call_id);
    return;
  }
  std::string type = msg.value("type", "");
  if (type == "answer") emit(call_id, "answered");
  else if (type == "decline") emit(call_id, "declined");
  else if (type == "speech") emit(call_id, "speech", msg["data"].value("text", ""));
  else LOG_WARN("sim", "unknown message type '" << type << "' on " << call_id);
}

void SimProvider::send(const std::string& call_id, const std::string& type, nlohmann::json data) {
  std::string text = nlohmann::json{{"type", type}, {"ts", time::nowTs()}, {"data", std::move(data)}}.dump();
  std::vector<Sender> targets;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = browsers_.find(call_id);
    if (it == browsers_.end() || it->second.empty()) {
      pending_[call_id].push_back(text);
      return;
    }
    for (auto& [conn, s] : it->second) targets.push_back(s);
  }
  for (auto& s : targets) s(text);
}

}  // namespace enro
