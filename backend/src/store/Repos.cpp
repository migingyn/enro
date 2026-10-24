#include "store/Repos.hpp"

#include "models/Json.hpp"

namespace enro {

using nlohmann::json;

namespace {

template <typename T>
T fromData(const SQLite::Column& col) {
  return json::parse(col.getString()).get<T>();
}

}  // namespace

// ---------- users ----------

void Repos::upsertUser(const User& u) {
  db_.with([&](SQLite::Database& db) {
    SQLite::Statement q(db, "INSERT OR REPLACE INTO users(id, data) VALUES(?, ?)");
    q.bind(1, u.id);
    q.bind(2, json(u).dump());
    q.exec();
  });
}

std::optional<User> Repos::user(const std::string& id) {
  return db_.with([&](SQLite::Database& db) -> std::optional<User> {
    SQLite::Statement q(db, "SELECT data FROM users WHERE id = ?");
    q.bind(1, id);
    if (q.executeStep()) return fromData<User>(q.getColumn(0));
    return std::nullopt;
  });
}

std::vector<User> Repos::users() {
  return db_.with([&](SQLite::Database& db) {
    std::vector<User> out;
    SQLite::Statement q(db, "SELECT data FROM users ORDER BY id");
    while (q.executeStep()) out.push_back(fromData<User>(q.getColumn(0)));
    return out;
  });
}

// ---------- contacts ----------

void Repos::upsertContact(const EmergencyContact& c) {
  db_.with([&](SQLite::Database& db) {
    SQLite::Statement q(db, "INSERT OR REPLACE INTO contacts(id, user_id, consent, data) VALUES(?, ?, ?, ?)");
    q.bind(1, c.id);
    q.bind(2, c.user_id);
    q.bind(3, json(c.consent).get<std::string>());
    q.bind(4, json(c).dump());
    q.exec();
  });
}

std::optional<EmergencyContact> Repos::contact(const std::string& id) {
  return db_.with([&](SQLite::Database& db) -> std::optional<EmergencyContact> {
    SQLite::Statement q(db, "SELECT data FROM contacts WHERE id = ?");
    q.bind(1, id);
    if (q.executeStep()) return fromData<EmergencyContact>(q.getColumn(0));
    return std::nullopt;
  });
}

std::vector<EmergencyContact> Repos::contacts(const std::string& user_id) {
  return db_.with([&](SQLite::Database& db) {
    std::vector<EmergencyContact> out;
    SQLite::Statement q(db, "SELECT data FROM contacts WHERE user_id = ? ORDER BY id");
    q.bind(1, user_id);
    while (q.executeStep()) out.push_back(fromData<EmergencyContact>(q.getColumn(0)));
    return out;
  });
}

// ---------- events ----------

void Repos::upsertEvent(const Event& e) {
  db_.with([&](SQLite::Database& db) {
    SQLite::Statement q(db,
                        "INSERT OR REPLACE INTO events(id, user_id, start_ts, end_ts, canceled, data) "
                        "VALUES(?, ?, ?, ?, ?, ?)");
    q.bind(1, e.id);
    q.bind(2, e.user_id);
    q.bind(3, static_cast<int64_t>(e.start_ts));
    q.bind(4, static_cast<int64_t>(e.end_ts));
    q.bind(5, e.canceled ? 1 : 0);
    q.bind(6, json(e).dump());
    q.exec();
  });
}

std::optional<Event> Repos::event(const std::string& id) {
  return db_.with([&](SQLite::Database& db) -> std::optional<Event> {
    SQLite::Statement q(db, "SELECT data FROM events WHERE id = ?");
    q.bind(1, id);
    if (q.executeStep()) return fromData<Event>(q.getColumn(0));
    return std::nullopt;
  });
}

std::vector<Event> Repos::events(const std::string& user_id, long long from_ts, long long to_ts) {
  return db_.with([&](SQLite::Database& db) {
    std::vector<Event> out;
    SQLite::Statement q(db,
                        "SELECT data FROM events WHERE user_id = ? AND start_ts >= ? AND start_ts < ? "
                        "ORDER BY start_ts");
    q.bind(1, user_id);
    q.bind(2, static_cast<int64_t>(from_ts));
    q.bind(3, static_cast<int64_t>(to_ts));
    while (q.executeStep()) out.push_back(fromData<Event>(q.getColumn(0)));
    return out;
  });
}

// ---------- tasks ----------

long long Repos::insertTask(const AgentTask& t) {
  return db_.with([&](SQLite::Database& db) {
    SQLite::Statement q(db, "INSERT INTO tasks(due_at, user_id, kind, payload) VALUES(?, ?, ?, ?)");
    q.bind(1, static_cast<int64_t>(t.due_at));
    q.bind(2, t.user_id);
    q.bind(3, json(t.kind).get<std::string>());
    q.bind(4, t.payload.dump());
    q.exec();
    return static_cast<long long>(db.getLastInsertRowid());
  });
}

void Repos::deleteTask(long long id) {
  db_.with([&](SQLite::Database& db) {
    SQLite::Statement q(db, "DELETE FROM tasks WHERE id = ?");
    q.bind(1, static_cast<int64_t>(id));
    q.exec();
  });
}

std::vector<std::pair<long long, AgentTask>> Repos::tasks() {
  return db_.with([&](SQLite::Database& db) {
    std::vector<std::pair<long long, AgentTask>> out;
    SQLite::Statement q(db, "SELECT id, due_at, user_id, kind, payload FROM tasks ORDER BY due_at, id");
    while (q.executeStep()) {
      AgentTask t;
      t.due_at = q.getColumn(1).getInt64();
      t.user_id = q.getColumn(2).getString();
      t.kind = json(q.getColumn(3).getString()).get<TaskKind>();
      t.payload = json::parse(q.getColumn(4).getString());
      out.emplace_back(q.getColumn(0).getInt64(), std::move(t));
    }
    return out;
  });
}

// ---------- decisions ----------

void Repos::insertDecision(const Decision& d) {
  db_.with([&](SQLite::Database& db) {
    SQLite::Statement q(db,
                        "INSERT INTO decisions(user_id, kind, verdict, source, ts, data) VALUES(?, ?, ?, ?, ?, ?)");
    q.bind(1, d.user_id);
    q.bind(2, d.kind);
    q.bind(3, d.verdict);
    q.bind(4, d.source);
    q.bind(5, static_cast<int64_t>(d.ts));
    q.bind(6, json(d).dump());
    q.exec();
  });
}

std::vector<Decision> Repos::recentDecisions(int limit) {
  return db_.with([&](SQLite::Database& db) {
    std::vector<Decision> out;
    SQLite::Statement q(db, "SELECT data FROM decisions ORDER BY id DESC LIMIT ?");
    q.bind(1, limit);
    while (q.executeStep()) out.push_back(fromData<Decision>(q.getColumn(0)));
    return out;
  });
}

// ---------- outcomes ----------

void Repos::insertOutcome(const Outcome& o) {
  db_.with([&](SQLite::Database& db) {
    SQLite::Statement q(db,
                        "INSERT INTO outcomes(user_id, event_id, event_type, mode, planned_leave, actual_leave, arrival) "
                        "VALUES(?, ?, ?, ?, ?, ?, ?)");
    q.bind(1, o.user_id);
    q.bind(2, o.event_id);
    q.bind(3, o.event_type);
    q.bind(4, o.mode);
    q.bind(5, static_cast<int64_t>(o.planned_leave));
    q.bind(6, static_cast<int64_t>(o.actual_leave));
    q.bind(7, static_cast<int64_t>(o.arrival));
    q.exec();
  });
}

std::vector<Outcome> Repos::outcomes(const std::string& user_id) {
  return db_.with([&](SQLite::Database& db) {
    std::vector<Outcome> out;
    SQLite::Statement q(db,
                        "SELECT user_id, event_id, event_type, mode, planned_leave, actual_leave, arrival "
                        "FROM outcomes WHERE user_id = ? ORDER BY planned_leave");
    q.bind(1, user_id);
    while (q.executeStep()) {
      Outcome o;
      o.user_id = q.getColumn(0).getString();
      o.event_id = q.getColumn(1).getString();
      o.event_type = q.getColumn(2).getString();
      o.mode = q.getColumn(3).getString();
      o.planned_leave = q.getColumn(4).getInt64();
      o.actual_leave = q.getColumn(5).getInt64();
      o.arrival = q.getColumn(6).getInt64();
      out.push_back(std::move(o));
    }
    return out;
  });
}

// ---------- calls and turns ----------

void Repos::upsertCall(const Call& c) {
  db_.with([&](SQLite::Database& db) {
    SQLite::Statement q(db, "INSERT OR REPLACE INTO calls(id, user_id, data) VALUES(?, ?, ?)");
    q.bind(1, c.id);
    q.bind(2, c.user_id);
    q.bind(3, json(c).dump());
    q.exec();
  });
}

std::optional<Call> Repos::call(const std::string& id) {
  return db_.with([&](SQLite::Database& db) -> std::optional<Call> {
    SQLite::Statement q(db, "SELECT data FROM calls WHERE id = ?");
    q.bind(1, id);
    if (q.executeStep()) return fromData<Call>(q.getColumn(0));
    return std::nullopt;
  });
}

void Repos::insertTurn(const Turn& t) {
  db_.with([&](SQLite::Database& db) {
    SQLite::Statement q(db, "INSERT INTO turns(call_id, ts, data) VALUES(?, ?, ?)");
    q.bind(1, t.call_id);
    q.bind(2, static_cast<int64_t>(t.ts));
    q.bind(3, json(t).dump());
    q.exec();
  });
}

std::vector<Turn> Repos::turns(const std::string& call_id) {
  return db_.with([&](SQLite::Database& db) {
    std::vector<Turn> out;
    SQLite::Statement q(db, "SELECT data FROM turns WHERE call_id = ? ORDER BY ts, id");
    q.bind(1, call_id);
    while (q.executeStep()) out.push_back(fromData<Turn>(q.getColumn(0)));
    return out;
  });
}

// ---------- email facts ----------

void Repos::upsertEmailFact(const EmailFact& f) {
  db_.with([&](SQLite::Database& db) {
    SQLite::Statement q(db,
                        "INSERT OR REPLACE INTO email_facts(message_id, user_id, verdict, ts, data) VALUES(?, ?, ?, ?, ?)");
    q.bind(1, f.message_id);
    q.bind(2, f.user_id);
    q.bind(3, f.verdict);
    q.bind(4, static_cast<int64_t>(f.ts));
    q.bind(5, f.extracted.dump());
    q.exec();
  });
}

std::vector<EmailFact> Repos::emailFacts(const std::string& user_id) {
  return db_.with([&](SQLite::Database& db) {
    std::vector<EmailFact> out;
    SQLite::Statement q(db, "SELECT message_id, user_id, verdict, ts, data FROM email_facts WHERE user_id = ? ORDER BY ts");
    q.bind(1, user_id);
    while (q.executeStep()) {
      out.push_back({q.getColumn(0).getString(), q.getColumn(1).getString(), q.getColumn(2).getString(),
                     q.getColumn(3).getInt64(), json::parse(q.getColumn(4).getString())});
    }
    return out;
  });
}

// ---------- kv and tokens ----------

void Repos::setKv(const std::string& key, const std::string& value) {
  db_.with([&](SQLite::Database& db) {
    SQLite::Statement q(db, "INSERT OR REPLACE INTO kv(key, value) VALUES(?, ?)");
    q.bind(1, key);
    q.bind(2, value);
    q.exec();
  });
}

std::optional<std::string> Repos::kv(const std::string& key) {
  return db_.with([&](SQLite::Database& db) -> std::optional<std::string> {
    SQLite::Statement q(db, "SELECT value FROM kv WHERE key = ?");
    q.bind(1, key);
    if (q.executeStep()) return q.getColumn(0).getString();
    return std::nullopt;
  });
}

void Repos::saveRefreshToken(const std::string& user_id, const std::string& provider, const std::string& token) {
  db_.with([&](SQLite::Database& db) {
    SQLite::Statement q(db, "INSERT OR REPLACE INTO oauth_tokens(user_id, provider, refresh_token) VALUES(?, ?, ?)");
    q.bind(1, user_id);
    q.bind(2, provider);
    q.bind(3, token);
    q.exec();
  });
}

std::optional<std::string> Repos::refreshToken(const std::string& user_id, const std::string& provider) {
  return db_.with([&](SQLite::Database& db) -> std::optional<std::string> {
    SQLite::Statement q(db, "SELECT refresh_token FROM oauth_tokens WHERE user_id = ? AND provider = ?");
    q.bind(1, user_id);
    q.bind(2, provider);
    if (q.executeStep()) return q.getColumn(0).getString();
    return std::nullopt;
  });
}

int Repos::count(const std::string& table) {
  return db_.with([&](SQLite::Database& db) { return db.execAndGet("SELECT COUNT(*) FROM " + table).getInt(); });
}

}  // namespace enro
