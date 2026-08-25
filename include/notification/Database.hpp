#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <mysqlx/xdevapi.h>
#include <queue>

#include "notification/Config.hpp"

namespace notification {

// RAII handle: returns the underlying Session to the pool when it goes
// out of scope, even on exception unwind (that's the whole point of this
// class -- callers never manually "release" a connection).
class PooledSession {
public:
    PooledSession(std::shared_ptr<mysqlx::Session> session,
                  std::function<void(std::shared_ptr<mysqlx::Session>)> releaser)
        : session_(std::move(session)), releaser_(std::move(releaser)) {}

    ~PooledSession() {
        if (session_) releaser_(session_);
    }

    PooledSession(const PooledSession&) = delete;
    PooledSession& operator=(const PooledSession&) = delete;
    PooledSession(PooledSession&&) = default;

    mysqlx::Session& get() { return *session_; }

private:
    std::shared_ptr<mysqlx::Session> session_;
    std::function<void(std::shared_ptr<mysqlx::Session>)> releaser_;
};

// Simple blocking connection pool. mysqlx::Session is not thread-safe, so
// every worker thread and every HTTP handler checks out its own session
// for the duration of a unit of work and returns it when done.
class ConnectionPool {
public:
    ConnectionPool(const Config& config, size_t size);

    PooledSession acquire();

private:
    void release(std::shared_ptr<mysqlx::Session> session);

    std::mutex mu_;
    std::condition_variable cv_;
    std::queue<std::shared_ptr<mysqlx::Session>> available_;
};

}  // namespace notification
