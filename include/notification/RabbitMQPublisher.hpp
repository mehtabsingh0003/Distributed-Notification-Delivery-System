#pragma once

#include <amqpcpp.h>
#include <amqpcpp/libboostasio.h>
#include <atomic>
#include <boost/asio/io_context.hpp>
#include <memory>
#include <string>
#include <thread>

#include "notification/Config.hpp"

namespace notification {

// Owns a single dedicated AMQP connection + channel driven by its own
// boost::asio::io_context on a background thread. AMQP-CPP's connection
// objects are not thread-safe, so every component that needs to publish
// independently (API layer, outbox publisher, worker retry republish)
// gets its own RabbitMQPublisher rather than sharing one.
class RabbitMQPublisher {
public:
    explicit RabbitMQPublisher(const Config& config);
    ~RabbitMQPublisher();

    RabbitMQPublisher(const RabbitMQPublisher&) = delete;
    RabbitMQPublisher& operator=(const RabbitMQPublisher&) = delete;

    // Publishes to the topic exchange with a routing key (normal delivery path).
    void publishToExchange(const std::string& exchange, const std::string& routingKey,
                            const std::string& body, uint8_t priority);

    // Publishes directly to a named queue via the default exchange (used
    // to place a message onto a specific retry-ladder queue by name).
    void publishToQueue(const std::string& queueName, const std::string& body, uint8_t priority,
                         const AMQP::Table& headers);

    AMQP::Channel& channel() { return *channel_; }
    void declareTopologyBlocking(const Config& config);

private:
    boost::asio::io_context ioContext_;
    std::unique_ptr<AMQP::LibBoostAsioHandler> handler_;
    std::unique_ptr<AMQP::TcpConnection> connection_;
    std::unique_ptr<AMQP::TcpChannel> channel_;
    std::thread ioThread_;
    std::atomic<bool> running_{false};
};

}  // namespace notification
