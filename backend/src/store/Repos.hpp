// Typed read/write access to the SQLite tables. Shared by every task.
// Add a method here when your task needs a new query; keep the SQL in Repos.cpp.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "models/Models.hpp"
#include "store/Database.hpp"

namespace enro {

// Facts extracted from a plan-changing email. Never the body (PRD 7.2).
struct EmailFact {
  std::string message_id, user_id, verdict;
  long long ts = 0;
  nlohmann::json extracted = nlohmann::json::object();
};

class Repos {
 public:
  explicit Repos(Database& db) : db_(db) {}

  // users
  void upsertUser(const User& u);
  std::optional<User> user(const std::string& id);
  std::vector<User> users();

  // emergency contacts
  void upsertContact(const EmergencyContact& c);
  std::optional<EmergencyContact> contact(const std::string& id);
  std::vector<EmergencyContact> contacts(const std::string& user_id);

  // events
  void upsertEvent(const Event& e);
  std::optional<Event> event(const std::string& id);
  std::vector<Event> events(const std::string& user_id, long long from_ts, long long to_ts);

  // scheduler tasks (T04 owns the semantics; this is just storage)
  long long insertTask(const AgentTask& t);
  void deleteTask(long long id);
  std::vector<std::pair<long long, AgentTask>> tasks();  // ordered by due_at

  // decisions (every agent action writes one, PRD 7.1)
  void insertDecision(const Decision& d);
  std::vector<Decision> recentDecisions(int limit = 50);

  // outcomes (punctuality model input, PRD 7.8)
  void insertOutcome(const Outcome& o);
  std::vector<Outcome> outcomes(const std::string& user_id);

  // calls and turns
  void upsertCall(const Call& c);
  std::optional<Call> call(const std::string& id);
  void insertTurn(const Turn& t);
  std::vector<Turn> turns(const std::string& call_id);

  // email facts
  void upsertEmailFact(const EmailFact& f);
  std::vector<EmailFact> emailFacts(const std::string& user_id);

  // key/value (sync tokens, Gmail historyId, ...)
  void setKv(const std::string& key, const std::string& value);
  std::optional<std::string> kv(const std::string& key);

  // OAuth refresh tokens, server side only (PRD 13)
  void saveRefreshToken(const std::string& user_id, const std::string& provider, const std::string& refresh_token);
  std::optional<std::string> refreshToken(const std::string& user_id, const std::string& provider);

  // row counts, for seeding and tests
  int count(const std::string& table);

 private:
  Database& db_;
};

}  // namespace enro
