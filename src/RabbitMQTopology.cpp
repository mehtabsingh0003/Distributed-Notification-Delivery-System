#include "notification/RabbitMQTopology.hpp"

#include "notification/Logger.hpp"

namespace notification {

std::string RabbitMQTopology::exchangeName() { return "notifications.exchange"; }

std::string RabbitMQTopology::routingKey(Channel c) {
    switch (c) {
        case Channel::Email: return "notification.email";
        case Channel::Sms:   return "notification.sms";
        case Channel::Push:  return "notification.push";
    }
    return "notification.unknown";
}

namespace {
std::string channelSlug(Channel c) {
    switch (c) {
        case Channel::Email: return "email";
        case Channel::Sms:   return "sms";
        case Channel::Push:  return "push";
    }
    return "unknown";
}
}  // namespace

std::string RabbitMQTopology::mainQueue(Channel c) { return "q." + channelSlug(c); }

std::string RabbitMQTopology::retryQueue(Channel c, int attemptNumber) {
    return "q." + channelSlug(c) + ".retry." + std::to_string(attemptNumber);
}

std::string RabbitMQTopology::dlq(Channel c) { return "q." + channelSlug(c) + ".dlq"; }

void RabbitMQTopology::declareAll(AMQP::Channel& channel, const Config& config) {
    channel.declareExchange(exchangeName(), AMQP::topic, AMQP::durable);

    for (Channel ch : {Channel::Email, Channel::Sms, Channel::Push}) {
        // Main work queue: priority-aware so HIGH priority notifications
        // jump the line ahead of LOW/NORMAL ones within the same channel.
        AMQP::Table mainArgs;
        mainArgs.set("x-max-priority", 5);
        channel.declareQueue(mainQueue(ch), AMQP::durable, mainArgs);
        channel.bindQueue(exchangeName(), mainQueue(ch), routingKey(ch));

        // Retry ladder: each rung has an increasing TTL; on expiry the
        // dead-letter-exchange config below routes the message straight
        // back to the main exchange with its original routing key, which
        // lands it back on the main queue for redelivery.
        for (size_t i = 0; i < config.retryDelaysMs.size(); ++i) {
            int attemptNumber = static_cast<int>(i) + 1;
            AMQP::Table retryArgs;
            retryArgs.set("x-message-ttl", config.retryDelaysMs[i]);
            retryArgs.set("x-dead-letter-exchange", exchangeName());
            retryArgs.set("x-dead-letter-routing-key", routingKey(ch));
            channel.declareQueue(retryQueue(ch, attemptNumber), AMQP::durable, retryArgs);
            // Retry queues are bound to nothing on the topic exchange --
            // the worker publishes into them directly by queue name via
            // the default exchange, so no binding is declared here.
        }

        // Dead-letter queue: terminal park for messages that exhausted
        // every retry. No TTL/DLX -- these sit until an operator
        // inspects/replays them.
        channel.declareQueue(dlq(ch), AMQP::durable);
    }

    LOG_INFO("RabbitMQTopology", "declared exchange, main/retry/DLQ queues for EMAIL, SMS, PUSH");
}

}  // namespace notification
