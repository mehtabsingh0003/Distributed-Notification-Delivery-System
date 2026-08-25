#include "notification/DeliveryAttemptRepository.hpp"

namespace notification {

void DeliveryAttemptRepository::record(const DeliveryAttempt& attempt) {
    auto handle = pool_.acquire();
    handle.get()
        .sql("INSERT INTO delivery_attempts "
             "(notification_id, attempt_number, channel, status, provider_ref, error_message) "
             "VALUES (?, ?, ?, ?, ?, ?) "
             "ON DUPLICATE KEY UPDATE status = VALUES(status), "
             "provider_ref = VALUES(provider_ref), error_message = VALUES(error_message)")
        .bind(attempt.notificationId, attempt.attemptNumber, toString(attempt.channel),
              toString(attempt.status), attempt.providerRef ? *attempt.providerRef : mysqlx::nullvalue,
              attempt.errorMessage ? *attempt.errorMessage : mysqlx::nullvalue)
        .execute();
}

std::vector<DeliveryAttempt> DeliveryAttemptRepository::findByNotificationId(const std::string& notificationId) {
    auto handle = pool_.acquire();
    auto result = handle.get()
                      .sql("SELECT id, notification_id, attempt_number, channel, status, provider_ref, "
                           "error_message, attempted_at FROM delivery_attempts "
                           "WHERE notification_id = ? ORDER BY attempt_number ASC")
                      .bind(notificationId)
                      .execute();

    std::vector<DeliveryAttempt> out;
    for (auto row : result.fetchAll()) {
        DeliveryAttempt a;
        a.id = row[0].get<int64_t>();
        a.notificationId = row[1].get<std::string>();
        a.attemptNumber = static_cast<int>(row[2].get<int64_t>());
        a.channel = channelFromString(row[3].get<std::string>());
        std::string status = row[4].get<std::string>();
        a.status = status == "SUCCESS" ? AttemptStatus::Success
                   : status == "TRANSIENT_FAILURE" ? AttemptStatus::TransientFailure
                                                    : AttemptStatus::PermanentFailure;
        if (!row[5].isNull()) a.providerRef = row[5].get<std::string>();
        if (!row[6].isNull()) a.errorMessage = row[6].get<std::string>();
        out.push_back(std::move(a));
    }
    return out;
}

}  // namespace notification
