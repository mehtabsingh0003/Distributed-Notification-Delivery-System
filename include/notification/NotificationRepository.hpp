#pragma once

#include <optional>
#include <vector>

#include "notification/Database.hpp"
#include "notification/Models.hpp"

namespace notification {

// Persists notifications and owns the transactional-outbox write.
//
// insertWithOutbox() writes the notification row and its outbox_events row
// in a single MySQL transaction, so the "was it accepted" fact and the
// "does a message still need publishing" fact can never disagree, even if
// the process crashes between the DB commit and the RabbitMQ publish. A
// separate OutboxPublisher polls outbox_events and republishes anything
// still PENDING (see docs/ARCHITECTURE.md).
class NotificationRepository {
public:
    explicit NotificationRepository(ConnectionPool& pool) : pool_(pool) {}

    // Returns the existing notification if idempotencyKey was already used
    // (so callers can safely retry POST /notifications), otherwise inserts
    // a new row + outbox event and returns it.
    Notification insertWithOutbox(const Notification& n);

    std::optional<Notification> findById(const std::string& id);
    std::optional<Notification> findByIdempotencyKey(const std::string& key);

    std::vector<Notification> list(std::optional<NotificationStatus> statusFilter,
                                    std::optional<Channel> channelFilter, int limit, int offset);

    void markProcessing(const std::string& id);
    void markSent(const std::string& id);
    void markFailed(const std::string& id);
    void markDeadLettered(const std::string& id);
    void incrementAttemptCount(const std::string& id);

private:
    ConnectionPool& pool_;
};

}  // namespace notification
