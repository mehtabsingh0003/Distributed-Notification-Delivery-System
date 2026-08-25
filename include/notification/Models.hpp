#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "notification/Enums.hpp"

namespace notification {

using Clock = std::chrono::system_clock;
using TimePoint = Clock::time_point;

// A notification submitted by a caller. `id` is a UUIDv4 generated at
// creation time and doubles as the idempotency / correlation key that
// flows through MySQL, RabbitMQ and the provider layer.
struct Notification {
    std::string id;
    std::string idempotencyKey;
    Channel channel{};
    Priority priority{Priority::Normal};
    std::string recipient;
    std::string subject;
    std::string body;
    NotificationStatus status{NotificationStatus::Pending};
    int attemptCount{0};
    TimePoint createdAt{};
    TimePoint updatedAt{};
};

// One delivery attempt against a provider for a given notification.
// (notification_id, attempt_number) is unique in the DB, which is what
// makes attempt logging idempotent under at-least-once redelivery.
struct DeliveryAttempt {
    long long id{0};
    std::string notificationId;
    int attemptNumber{0};
    Channel channel{};
    AttemptStatus status{};
    std::optional<std::string> providerRef;
    std::optional<std::string> errorMessage;
    TimePoint attemptedAt{};
};

}  // namespace notification
