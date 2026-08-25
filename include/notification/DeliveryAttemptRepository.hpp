#pragma once

#include <vector>

#include "notification/Database.hpp"
#include "notification/Models.hpp"

namespace notification {

// (notification_id, attempt_number) has a UNIQUE KEY in MySQL, so calling
// record() twice for the same attempt -- e.g. because a worker crashed
// after sending to the provider but before acking RabbitMQ, and the
// message was redelivered -- is a no-op the second time. That's what makes
// the audit trail idempotent under RabbitMQ's at-least-once guarantee.
class DeliveryAttemptRepository {
public:
    explicit DeliveryAttemptRepository(ConnectionPool& pool) : pool_(pool) {}

    void record(const DeliveryAttempt& attempt);
    std::vector<DeliveryAttempt> findByNotificationId(const std::string& notificationId);

private:
    ConnectionPool& pool_;
};

}  // namespace notification
