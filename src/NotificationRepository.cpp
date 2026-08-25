#include "notification/NotificationRepository.hpp"

#include "notification/IdGenerator.hpp"
#include "notification/Logger.hpp"

namespace notification {

namespace {

Notification rowToNotification(mysqlx::Row& row) {
    Notification n;
    n.id = row[0].get<std::string>();
    n.idempotencyKey = row[1].get<std::string>();
    n.channel = channelFromString(row[2].get<std::string>());
    n.priority = priorityFromString(row[3].get<std::string>());
    n.recipient = row[4].get<std::string>();
    n.subject = row[5].isNull() ? "" : row[5].get<std::string>();
    n.body = row[6].get<std::string>();
    n.status = [&] {
        std::string s = row[7].get<std::string>();
        if (s == "PENDING") return NotificationStatus::Pending;
        if (s == "PROCESSING") return NotificationStatus::Processing;
        if (s == "SENT") return NotificationStatus::Sent;
        if (s == "FAILED") return NotificationStatus::Failed;
        if (s == "DEAD_LETTERED") return NotificationStatus::DeadLettered;
        return NotificationStatus::Cancelled;
    }();
    n.attemptCount = static_cast<int>(row[8].get<int64_t>());
    return n;
}

}  // namespace

Notification NotificationRepository::insertWithOutbox(const Notification& in) {
    auto handle = pool_.acquire();
    mysqlx::Session& session = handle.get();

    // Idempotent submission: if a caller retries the same Idempotency-Key
    // (e.g. after a network timeout on their side), return the row that
    // already exists instead of creating a duplicate notification.
    if (auto existing = findByIdempotencyKey(in.idempotencyKey)) {
        return *existing;
    }

    Notification n = in;
    n.id = IdGenerator::uuid4();
    n.status = NotificationStatus::Pending;

    session.startTransaction();
    try {
        session
            .sql("INSERT INTO notifications "
                 "(id, idempotency_key, channel, priority, recipient, subject, body, status, attempt_count) "
                 "VALUES (?, ?, ?, ?, ?, ?, ?, 'PENDING', 0)")
            .bind(n.id, n.idempotencyKey, toString(n.channel), toString(n.priority), n.recipient, n.subject,
                  n.body)
            .execute();

        // Outbox row: the message a background publisher will hand to
        // RabbitMQ. Written in the *same* transaction as the notification
        // row above -- this is the transactional outbox pattern.
        session
            .sql("INSERT INTO outbox_events (notification_id, event_type, payload, status) "
                 "VALUES (?, 'NOTIFICATION_CREATED', ?, 'PENDING')")
            .bind(n.id, n.id)
            .execute();

        session.commit();
    } catch (...) {
        session.rollback();
        throw;
    }

    LOG_INFO("NotificationRepository", "inserted notification " + n.id + " with outbox event");
    return n;
}

std::optional<Notification> NotificationRepository::findById(const std::string& id) {
    auto handle = pool_.acquire();
    auto result = handle.get()
                      .sql("SELECT id, idempotency_key, channel, priority, recipient, subject, body, "
                           "status, attempt_count FROM notifications WHERE id = ?")
                      .bind(id)
                      .execute();
    auto row = result.fetchOne();
    if (!row) return std::nullopt;
    return rowToNotification(row);
}

std::optional<Notification> NotificationRepository::findByIdempotencyKey(const std::string& key) {
    auto handle = pool_.acquire();
    auto result = handle.get()
                      .sql("SELECT id, idempotency_key, channel, priority, recipient, subject, body, "
                           "status, attempt_count FROM notifications WHERE idempotency_key = ?")
                      .bind(key)
                      .execute();
    auto row = result.fetchOne();
    if (!row) return std::nullopt;
    return rowToNotification(row);
}

std::vector<Notification> NotificationRepository::list(std::optional<NotificationStatus> statusFilter,
                                                         std::optional<Channel> channelFilter, int limit,
                                                         int offset) {
    std::string query =
        "SELECT id, idempotency_key, channel, priority, recipient, subject, body, status, attempt_count "
        "FROM notifications WHERE 1=1";
    if (statusFilter) query += " AND status = ?";
    if (channelFilter) query += " AND channel = ?";
    query += " ORDER BY created_at DESC LIMIT ? OFFSET ?";

    auto handle = pool_.acquire();
    auto stmt = handle.get().sql(query);
    if (statusFilter) stmt.bind(toString(*statusFilter));
    if (channelFilter) stmt.bind(toString(*channelFilter));
    stmt.bind(limit).bind(offset);

    auto result = stmt.execute();
    std::vector<Notification> out;
    for (auto row : result.fetchAll()) {
        out.push_back(rowToNotification(row));
    }
    return out;
}

void NotificationRepository::markProcessing(const std::string& id) {
    pool_.acquire().get().sql("UPDATE notifications SET status='PROCESSING' WHERE id=?").bind(id).execute();
}

void NotificationRepository::markSent(const std::string& id) {
    pool_.acquire().get().sql("UPDATE notifications SET status='SENT' WHERE id=?").bind(id).execute();
}

void NotificationRepository::markFailed(const std::string& id) {
    pool_.acquire().get().sql("UPDATE notifications SET status='FAILED' WHERE id=?").bind(id).execute();
}

void NotificationRepository::markDeadLettered(const std::string& id) {
    pool_.acquire().get().sql("UPDATE notifications SET status='DEAD_LETTERED' WHERE id=?").bind(id).execute();
}

void NotificationRepository::incrementAttemptCount(const std::string& id) {
    pool_.acquire().get().sql("UPDATE notifications SET attempt_count = attempt_count + 1 WHERE id=?").bind(id).execute();
}

}  // namespace notification
