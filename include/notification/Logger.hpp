#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

namespace notification {

// Minimal structured-ish logger: timestamp, level, thread id, component, message.
// Swappable for spdlog later without touching call sites (same free functions).
class Logger {
public:
    enum class Level { Debug, Info, Warn, Error };

    static void log(Level level, const std::string& component, const std::string& msg) {
        static std::mutex mu;
        std::lock_guard<std::mutex> lock(mu);

        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream ts;
        ts << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");

        std::cout << "[" << ts.str() << "] [" << levelStr(level) << "] "
                  << "[thread=" << std::this_thread::get_id() << "] "
                  << "[" << component << "] " << msg << std::endl;
    }

private:
    static const char* levelStr(Level l) {
        switch (l) {
            case Level::Debug: return "DEBUG";
            case Level::Info:  return "INFO";
            case Level::Warn:  return "WARN";
            case Level::Error: return "ERROR";
        }
        return "INFO";
    }
};

#define LOG_INFO(component, msg) ::notification::Logger::log(::notification::Logger::Level::Info, component, msg)
#define LOG_WARN(component, msg) ::notification::Logger::log(::notification::Logger::Level::Warn, component, msg)
#define LOG_ERROR(component, msg) ::notification::Logger::log(::notification::Logger::Level::Error, component, msg)
#define LOG_DEBUG(component, msg) ::notification::Logger::log(::notification::Logger::Level::Debug, component, msg)

}  // namespace notification
