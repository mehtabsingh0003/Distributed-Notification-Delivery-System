#include "notification/RabbitMQPublisher.hpp"

#include <chrono>
#include <thread>

#include "notification/Logger.hpp"
#include "notification/RabbitMQTopology.hpp"

namespace notification {

namespace {
std::string buildAmqpAddress(const Config& c) {
    return "amqp://" + c.amqpUser + ":" + c.amqpPassword + "@" + c.amqpHost + ":" +
           std::to_string(c.amqpPort) + c.amqpVhost;
}
}  // namespace

RabbitMQPublisher::RabbitMQPublisher(const Config& config) {
    handler_ = std::make_unique<AMQP::LibBoostAsioHandler>(ioContext_);

    AMQP::Address address(buildAmqpAddress(config));
    connection_ = std::make_unique<AMQP::TcpConnection>(handler_.get(), address);
    channel_ = std::make_unique<AMQP::TcpChannel>(connection_.get());

    channel_->onError([](const char* message) {
        LOG_ERROR("RabbitMQPublisher", std::string("channel error: ") + message);
    });

    // Publisher confirms give us a durability signal: onAck/onNack tell us
    // whether RabbitMQ actually persisted the message to disk before
    // replying, which is what "at-least-once" delivery is built on top of.
    channel_->confirmSelect();

    running_ = true;
    ioThread_ = std::thread([this] {
        LOG_INFO("RabbitMQPublisher", "io_context thread started");
        ioContext_.run();
    });

    // Give the handshake a moment to complete before topology declaration.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

RabbitMQPublisher::~RabbitMQPublisher() {
    running_ = false;
    ioContext_.stop();
    if (ioThread_.joinable()) ioThread_.join();
}

void RabbitMQPublisher::declareTopologyBlocking(const Config& config) {
    RabbitMQTopology::declareAll(*channel_, config);
    // Topology declaration is fire-and-forget from AMQP-CPP's perspective;
    // give the broker a beat to process it before publishers start using
    // the queues (avoids a race on first startup).
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

void RabbitMQPublisher::publishToExchange(const std::string& exchange, const std::string& routingKey,
                                           const std::string& body, uint8_t priority) {
    AMQP::Envelope envelope(body.data(), body.size());
    envelope.setPersistent(true);
    envelope.setPriority(priority);
    channel_->publish(exchange, routingKey, envelope);
}

void RabbitMQPublisher::publishToQueue(const std::string& queueName, const std::string& body,
                                        uint8_t priority, const AMQP::Table& headers) {
    AMQP::Envelope envelope(body.data(), body.size());
    envelope.setPersistent(true);
    envelope.setPriority(priority);
    envelope.setHeaders(headers);
    // Empty exchange name == the default exchange, which routes by queue
    // name directly -- exactly what we want for placing a message on a
    // specific retry-ladder queue.
    channel_->publish("", queueName, envelope);
}

}  // namespace notification
