#include "store/Database.hpp"

#include "util/Log.hpp"

namespace enro {

namespace {

// PRD 10 models. Rows keep the full model as JSON in `data` plus the columns we
// query or index on. Never store email bodies (PRD 7.2, 13).
const char* kSchema = R"SQL(
CREATE TABLE IF NOT EXISTS users (
  id   TEXT PRIMARY KEY,
  data TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS contacts (
  id      TEXT PRIMARY KEY,
  user_id TEXT NOT NULL,
  consent TEXT NOT NULL,
  data    TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS contacts_user ON contacts(user_id);
CREATE TABLE IF NOT EXISTS events (
  id       TEXT PRIMARY KEY,
  user_id  TEXT NOT NULL,
  start_ts INTEGER NOT NULL,
  end_ts   INTEGER NOT NULL,
  canceled INTEGER NOT NULL DEFAULT 0,
  data     TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS events_user_start ON events(user_id, start_ts);
CREATE TABLE IF NOT EXISTS tasks (
  id      INTEGER PRIMARY KEY AUTOINCREMENT,
  due_at  INTEGER NOT NULL,
  user_id TEXT NOT NULL,
  kind    TEXT NOT NULL,
  payload TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS tasks_due ON tasks(due_at);
CREATE TABLE IF NOT EXISTS decisions (
  id      INTEGER PRIMARY KEY AUTOINCREMENT,
  user_id TEXT NOT NULL,
  kind    TEXT NOT NULL,
  verdict TEXT NOT NULL,
  source  TEXT NOT NULL,
  ts      INTEGER NOT NULL,
  data    TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS decisions_ts ON decisions(ts);
CREATE TABLE IF NOT EXISTS outcomes (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  user_id       TEXT NOT NULL,
  event_id      TEXT NOT NULL,
  event_type    TEXT NOT NULL,
  mode          TEXT NOT NULL,
  planned_leave INTEGER NOT NULL,
  actual_leave  INTEGER NOT NULL,
  arrival       INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS outcomes_user ON outcomes(user_id, event_type);
CREATE TABLE IF NOT EXISTS calls (
  id      TEXT PRIMARY KEY,
  user_id TEXT NOT NULL,
  data    TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS turns (
  id      INTEGER PRIMARY KEY AUTOINCREMENT,
  call_id TEXT NOT NULL,
  ts      INTEGER NOT NULL,
  data    TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS turns_call ON turns(call_id, ts);
CREATE TABLE IF NOT EXISTS email_facts (
  message_id TEXT PRIMARY KEY,
  user_id    TEXT NOT NULL,
  verdict    TEXT NOT NULL,
  ts         INTEGER NOT NULL,
  data       TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS oauth_tokens (
  user_id       TEXT NOT NULL,
  provider      TEXT NOT NULL,
  refresh_token TEXT NOT NULL,
  access_token  TEXT NOT NULL DEFAULT '',
  expires_at    INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY (user_id, provider)
);
CREATE TABLE IF NOT EXISTS kv (
  key   TEXT PRIMARY KEY,
  value TEXT NOT NULL
);
)SQL";

}  // namespace

Database::Database(const std::string& path) : path_(path) {
  db_ = std::make_unique<SQLite::Database>(path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE | SQLite::OPEN_FULLMUTEX);
  db_->exec("PRAGMA journal_mode=WAL;");
  db_->exec("PRAGMA foreign_keys=ON;");
  LOG_INFO("db", "opened " << path);
}

void Database::createSchema() {
  with([](SQLite::Database& db) { db.exec(kSchema); });
}

}  // namespace enro
