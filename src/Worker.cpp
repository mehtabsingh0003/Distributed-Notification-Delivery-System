#include "notification/Worker.hpp"

#include <chrono>
#include <nlohmann/json.hpp>
#include <thread>

#include "notification/Logger.hpp"

namespace notification {

namespace {
std::string buildAmqpAddress(const Config& c) {
    return "amqp://" + c.amqpUser + ":" + c.amqpPassword + "@" + c.amqpHost + ":" +
           std::to_string(c.amqpPort) + c.amqpVhost;
}
}  // namespace

WorkerThread::WorkerThread(Channel channel, const Config& config, ConnectionPool& dbPool,
                            std::shared_ptr<ProviderRouter> router)
    : channel_(channel),
      config_(config),
      dbPool_(dbPool),
      router_(std::move(router)),
      notificationRepo_(dbPool),
      attemptRepo_(dbPool) {}

WorkerThread::~WorkerThread() { stop(); }

void WorkerThread::start() {
    running_ = true;
    thread_ = std::thread([this] { run(); });
}

void WorkerThread::stop() {
    if (!running_) return;
    running_ = false;
    ioContext_.stop();
    if (thread_.joinable()) thread_.join();
}

void WorkerThread::run() {
    const std::string component = "Worker[" + toString(channel_) + "]";
    handler_ = std::make_unique<AMQP::LibBoostAsioHandler>(ioContext_);
    AMQP::Address address(buildAmqpAddress(config_));
    connection_ = std::make_unique<AMQP::TcpConnection>(handler_.get(), address);
    amqpChannel_ = std::make_unique<AMQP::TcpChannel>(connection_.get());

    amqpChannel_->onError(
        [component](const char* msg) { LOG_ERROR(component, std::string("channel error: ") + msg); });

    // Bounded prefetch: a slow provider call (network hiccup, circuit
    // breaker) shouldn't let this thread hoover up unbounded unacked
    // messages -- it can only ever be processing/holding a handful at once.
    amqpChannel_->setQos(10);

    amqpChannel_->consume(RabbitMQTopology::mainQueue(channel_))
        .onReceived([this](const AMQP::Message& message, uint64_t deliveryTag, bool redelivered) {
            onMessage(message, deliveryTag, redelivered);
        })
        .onError([component](const char* msg) {
            LOG_ERROR(component, std::string("consume error: ") + msg);
        });

    LOG_INFO(component, "consumer started on " + RabbitMQTopology::mainQueue(channel_));
    ioContext_.run();
    LOG_INFO(component, "consumer stopped");
}

void WorkerThread::onMessage(const AMQP::Message& message, uint64_t deliveryTag, bool /*redelivered*/) {
    const std::string component = "Worker[" + toString(channel_) + "]";
    std::string body(message.body(), message.bodySize());

    std::string notificationId;
    int attemptNumber = 1;
    uint8_t priority = 3;
    try {
        auto j = nlohmann::json::parse(body);
        notificationId = j.at("notificationId").get<std::string>();
        attemptNumber = j.value("attemptNumber", 1);
        priority = static_cast<uint8_t>(j.value("priority", 3));
    } catch (const std::exception& ex) {
        LOG_ERROR(component, "malformed message, acking to drop: " + std::string(ex.what()));
        amqpChannel_->ack(deliveryTag);
        return;
    }

    auto maybeNotification = notificationRepo_.findById(notificationId);
    if (!maybeNotification) {
        LOG_WARN(component, "notification " + notificationId + " not found, dropping message");
        amqpChannel_->ack(deliveryTag);
        return;
    }
    Notification n = *maybeNotification;

    if (n.status == NotificationStatus::Cancelled || n.status == NotificationStatus::Sent) {
        LOG_INFO(component, "notification " + notificationId + " already " + toString(n.status) +
                                 ", skipping");
        amqpChannel_->ack(deliveryTag);
        return;
    }

    notificationRepo_.markProcessing(notificationId);

    DeliveryAttempt attempt;
    attempt.notificationId = notificationId;
    attempt.attemptNumber = attemptNumber;
    attempt.channel = channel_;

    try {
        ProviderResult result = router_->send(n);

        attempt.status = AttemptStatus::Success;
        attempt.providerRef = result.providerRef;
        attemptRepo_.record(attempt);
        notificationRepo_.markSent(notificationId);
        notificationRepo_.incrementAttemptCount(notificationId);

        LOG_INFO(component, "delivered notification " + notificationId + " via provider ref " +
                                 result.providerRef);
        amqpChannel_->ack(deliveryTag);

    } catch (const ProviderException& ex) {
        notificationRepo_.incrementAttemptCount(notificationId);

        bool exhausted = attemptNumber > config_.maxRetries;

        if (ex.isTransient() && !exhausted) {
            attempt.status = AttemptStatus::TransientFailure;
            attempt.errorMessage = ex.what();
            attemptRepo_.record(attempt);

            nlohmann::json retryBody{
                {"notificationId", notificationId}, {"attemptNumber", attemptNumber + 1}, {"priority", priority}};
            std::string retryQueue = RabbitMQTopology::retryQueue(channel_, attemptNumber);

            AMQP::Envelope envelope(retryBody.dump());
            envelope.setPersistent(true);
            envelope.setPriority(priority);
            amqpChannel_->publish("", retryQueue, envelope);

            LOG_WARN(component, "transient failure for " + notificationId + " (attempt " +
                                     std::to_string(attemptNumber) + "), scheduled retry on " + retryQueue);
        } else {
            attempt.status = ex.isTransient() ? AttemptStatus::TransientFailure : AttemptStatus::PermanentFailure;
            attempt.errorMessage = ex.what();
            attemptRepo_.record(attempt);

            std::string dlq = RabbitMQTopology::dlq(channel_);
            AMQP::Envelope envelope(body);
            envelope.setPersistent(true);
            amqpChannel_->publish("", dlq, envelope);

            notificationRepo_.markDeadLettered(notificationId);
            LOG_ERROR(component, "permanent/exhausted failure for " + notificationId +
                                      ", dead-lettered to " + dlq + ": " + ex.what());
        }

        amqpChannel_->ack(deliveryTag);
    }
}

WorkerPool::WorkerPool(const Config& config, ConnectionPool& dbPool) {
    auto router = std::make_shared<ProviderRouter>(config);
    for (Channel ch : {Channel::Email, Channel::Sms, Channel::Push}) {
        for (int i = 0; i < config.workersPerChannel; ++i) {
            workers_.push_back(std::make_unique<WorkerThread>(ch, config, dbPool, router));
        }
    }
}

WorkerPool::~WorkerPool() { stop(); }

void WorkerPool::start() {
    for (auto& w : workers_) w->start();
    LOG_INFO("WorkerPool", "started " + std::to_string(workers_.size()) + " worker threads");
}

void WorkerPool::stop() {
    for (auto& w : workers_) w->stop();
}

}  // namespace notification
