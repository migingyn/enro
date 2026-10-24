// Browser call simulator (VOICE_PROVIDER=sim, the default). Calls ring the CallSimulator page
// over /ws/sim-call/:call_id instead of a phone. Backup if phones fail on stage (PRD 17).
// Messages follow contracts/schemas/simcall_*.schema.json.
#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "voice/VoiceProvider.hpp"

namespace enro {

class SimProvider : public VoiceProvider {
 public:
  using Sender = std::function<void(const std::string& text)>;

  std::string name() const override { return "sim"; }

  std::string placeCall(const CallRequest& req) override;
  void say(const std::string& call_id, const std::string& text, const std::string& persona, Urgency urgency,
           const std::string& audio_url) override;
  void hangup(const std::string& call_id, const std::string& reason) override;
  std::string sendSms(const std::string& to_phone, const std::string& body) override;

  // Called by http/SimCallSocket.cpp when a browser opens or closes /ws/sim-call/:key.
  // key is a call id (that call only) or a user id (every call to that user, e.g. the whole wake
  // ladder). Messages for a call nobody is listening to are queued and flushed on connect.
  void attachBrowser(const std::string& call_id, void* conn, Sender send);
  void detachBrowser(void* conn);
  // A message from the browser (answer | decline | speech envelope). data.call_id wins over key.
  void onBrowserMessage(const std::string& key, const std::string& raw);

  // Texts "sent" in sim mode, newest last (for tests and the dashboard).
  std::vector<nlohmann::json> sentSms() const;

 private:
  void send(const std::string& call_id, const std::string& type, nlohmann::json data);

  mutable std::mutex mu_;
  std::map<std::string, std::vector<std::pair<void*, Sender>>> browsers_;  // call_id or user_id -> sockets
  std::map<std::string, std::string> call_user_;                            // call_id -> user_id
  std::map<std::string, std::vector<std::string>> pending_;                // call_id -> queued messages
  std::vector<nlohmann::json> sms_;
};

}  // namespace enro
