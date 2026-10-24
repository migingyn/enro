// Tiny thread-safe logger. Usage: LOG_INFO("scheduler", "popped " << n << " tasks");
// Level comes from LOG_LEVEL (debug | info | warn | error), default info.
#pragma once

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace enro::log {

enum class Level { Debug = 0, Info = 1, Warn = 2, Error = 3 };

inline Level threshold() {
  static const Level level = [] {
    const char* v = std::getenv("LOG_LEVEL");
    std::string s = v ? v : "info";
    if (s == "debug") return Level::Debug;
    if (s == "warn") return Level::Warn;
    if (s == "error") return Level::Error;
    return Level::Info;
  }();
  return level;
}

inline void write(Level level, const std::string& tag, const std::string& msg) {
  if (level < threshold()) return;
  static std::mutex mu;
  static const char* names[] = {"DEBUG", "INFO ", "WARN ", "ERROR"};
  auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm tm{};
  localtime_r(&now, &tm);
  std::lock_guard<std::mutex> lock(mu);
  std::ostream& out = level >= Level::Warn ? std::cerr : std::cout;
  out << std::put_time(&tm, "%H:%M:%S") << ' ' << names[static_cast<int>(level)] << " [" << tag << "] " << msg
      << std::endl;
}

}  // namespace enro::log

#define ENRO_LOG(level, tag, expr)                    \
  do {                                                \
    std::ostringstream enro_log_ss_;                  \
    enro_log_ss_ << expr;                             \
    ::enro::log::write(level, tag, enro_log_ss_.str()); \
  } while (0)

#define LOG_DEBUG(tag, expr) ENRO_LOG(::enro::log::Level::Debug, tag, expr)
#define LOG_INFO(tag, expr) ENRO_LOG(::enro::log::Level::Info, tag, expr)
#define LOG_WARN(tag, expr) ENRO_LOG(::enro::log::Level::Warn, tag, expr)
#define LOG_ERROR(tag, expr) ENRO_LOG(::enro::log::Level::Error, tag, expr)
