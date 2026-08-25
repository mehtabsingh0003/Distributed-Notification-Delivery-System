#include "notification/Database.hpp"

#include "notification/Logger.hpp"

namespace notification {

ConnectionPool::ConnectionPool(const Config& config, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        auto session = std::make_shared<mysqlx::Session>(
            mysqlx::SessionOption::HOST, config.dbHost,
            mysqlx::SessionOption::PORT, config.dbPort,
            mysqlx::SessionOption::USER, config.dbUser,
            mysqlx::SessionOption::PWD, config.dbPassword,
            mysqlx::SessionOption::DB, config.dbSchema);
        available_.push(session);
    }
    LOG_INFO("ConnectionPool", "initialized pool with " + std::to_string(size) + " sessions");
}

PooledSession ConnectionPool::acquire() {
    std::unique_lock<std::mutex> lock(mu_);
    cv_.wait(lock, [this] { return !available_.empty(); });
    auto session = available_.front();
    available_.pop();
    lock.unlock();

    return PooledSession(session, [this](std::shared_ptr<mysqlx::Session> s) { release(std::move(s)); });
}

void ConnectionPool::release(std::shared_ptr<mysqlx::Session> session) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        available_.push(std::move(session));
    }
    cv_.notify_one();
}

}  // namespace notification
