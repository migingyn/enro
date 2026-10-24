// SQLite connection + schema creation. One shared connection guarded by a mutex.
// Use ":memory:" as the path in tests.
#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include <SQLiteCpp/SQLiteCpp.h>

namespace enro {

class Database {
 public:
  explicit Database(const std::string& path);

  // Creates every table if missing. Safe to call on every boot.
  void createSchema();

  // Runs fn with exclusive access to the connection.
  template <typename Fn>
  auto with(Fn&& fn) {
    std::lock_guard<std::mutex> lock(mu_);
    return fn(*db_);
  }

  const std::string& path() const { return path_; }

 private:
  std::string path_;
  std::unique_ptr<SQLite::Database> db_;
  std::mutex mu_;
};

}  // namespace enro
