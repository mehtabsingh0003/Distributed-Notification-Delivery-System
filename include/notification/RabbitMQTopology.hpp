#pragma once

#include <amqpcpp.h>

#include "notification/Config.hpp"
#include "notification/Enums.hpp"

namespace notification {

// Declares the full exchange/queue topology used for the exponential
// backoff retry ladder:
//
//   notifications.exchange (topic)
//     --routing key notification.email--> q.email --------------------+
//     --routing key notification.sms  --> q.sms                       |
//     --routing key notification.push --> q.push                      |
//                                                                      |
//   on transient failure, the worker republishes to the retry queue    |
//   matching the current attempt number instead of nack+requeue:      |
//                                                                      |
//     q.email.retry.1  (TTL 5s,  DLX -> notifications.exchange) ------+
//     q.email.retry.2  (TTL 15s, DLX -> notifications.exchange)
//     q.email.retry.3  (TTL 60s, DLX -> notifications.exchange)
//
//   TTL expiry + dead-letter-exchange is the standard RabbitMQ pattern
//   for delayed redelivery (RabbitMQ has no native "publish in N ms").
//   After maxRetries is exhausted, the message goes to:
//
//     q.email.dlq  (no TTL -- parked for manual inspection / replay)
//
// The same ladder is declared per channel (email/sms/push).
class RabbitMQTopology {
public:
    static void declareAll(AMQP::Channel& channel, const Config& config);

    static std::string exchangeName();
    static std::string routingKey(Channel c);
    static std::string mainQueue(Channel c);
    static std::string retryQueue(Channel c, int attemptNumber);
    static std::string dlq(Channel c);
};

}  // namespace notification
