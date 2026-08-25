#pragma once

#include <stdexcept>
#include <string>

namespace notification {

enum class Channel { Email, Sms, Push };
enum class Priority { Low, Normal, High };
enum class NotificationStatus { Pending, Processing, Sent, Failed, DeadLettered, Cancelled };
enum class AttemptStatus { Success, TransientFailure, PermanentFailure };

inline std::string toString(Channel c) {
    switch (c) {
        case Channel::Email: return "EMAIL";
        case Channel::Sms:   return "SMS";
        case Channel::Push:  return "PUSH";
    }
    throw std::invalid_argument("unknown channel");
}

inline Channel channelFromString(const std::string& s) {
    if (s == "EMAIL") return Channel::Email;
    if (s == "SMS")   return Channel::Sms;
    if (s == "PUSH")  return Channel::Push;
    throw std::invalid_argument("unknown channel: " + s);
}

inline std::string toString(Priority p) {
    switch (p) {
        case Priority::Low:    return "LOW";
        case Priority::Normal: return "NORMAL";
        case Priority::High:   return "HIGH";
    }
    throw std::invalid_argument("unknown priority");
}

inline Priority priorityFromString(const std::string& s) {
    if (s == "LOW") return Priority::Low;
    if (s == "HIGH") return Priority::High;
    return Priority::Normal;
}

// Higher number == higher RabbitMQ priority (queues are declared with x-max-priority: 5)
inline uint8_t priorityWeight(Priority p) {
    switch (p) {
        case Priority::Low:    return 1;
        case Priority::Normal: return 3;
        case Priority::High:   return 5;
    }
    return 3;
}

inline std::string toString(NotificationStatus s) {
    switch (s) {
        case NotificationStatus::Pending:      return "PENDING";
        case NotificationStatus::Processing:   return "PROCESSING";
        case NotificationStatus::Sent:         return "SENT";
        case NotificationStatus::Failed:       return "FAILED";
        case NotificationStatus::DeadLettered: return "DEAD_LETTERED";
        case NotificationStatus::Cancelled:    return "CANCELLED";
    }
    throw std::invalid_argument("unknown status");
}

inline std::string toString(AttemptStatus s) {
    switch (s) {
        case AttemptStatus::Success:          return "SUCCESS";
        case AttemptStatus::TransientFailure: return "TRANSIENT_FAILURE";
        case AttemptStatus::PermanentFailure: return "PERMANENT_FAILURE";
    }
    throw std::invalid_argument("unknown attempt status");
}

}  // namespace notification
