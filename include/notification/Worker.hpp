#pragma once

#include <amqpcpp.h>
#include <amqpcpp/libboostasio.h>
#include <atomic>
#include <boost/asio/io_context.hpp>
#include <memory>
#include <thread>
#include <vector>

#include "notification/Config.hpp"
#include "notification/Database.hpp"
#include "notification/DeliveryAttemptRepository.hpp"
#include "notification/Enums.hpp"
#include "notification/NotificationRepository.hpp"
#include "notification/ProviderRouter.hpp"
#include "notification/RabbitMQTopology.hpp"

namespace notification {

// One WorkerThread == one dedicated AMQP connection consuming a single
// channel's main queue with a bounded prefetch. AMQP-CPP connections are
// not thread-safe, so "N worker threads sharing one connection" is not an
// option -- each thread gets its own connection/channel/io_context,
// exactly like RabbitMQPublisher.
//
// On each message:
//   1. decode notificationId + attemptNumber from the JSON body/headers
//   2. call ProviderRouter::send()
//   3. on success: record SUCCESS attempt, mark notification SENT, ack
//   4. on transient failure with attempts remaining: record
//      TRANSIENT_FAILURE attempt, republish to the next retry-ladder
//      queue (see RabbitMQTopology), ack the original message
//   5. on transient failure with attempts exhausted, or any permanent
//      failure: record *_FAILURE attempt, publish to the channel DLQ,
//      mark notification DEAD_LETTERED / FAILED, ack the original message
//
// Acking (rather than nack+requeue) on every outcome is deliberate: the
// retry ladder / DLQ publish *is* the redelivery mechanism, so the
// original message must leave the main queue exactly once either way.
class WorkerThread {
public:
    WorkerThread(Channel channel, const Config& config, ConnectionPool& dbPool,
                 std::shared_ptr<ProviderRouter> router);
    ~WorkerThread();

    void start();
    void stop();

private:
    void run();
    void onMessage(const AMQP::Message& message, uint64_t deliveryTag, bool redelivered);

    Channel channel_;
    Config config_;
    ConnectionPool& dbPool_;
    std::shared_ptr<ProviderRouter> router_;
    NotificationRepository notificationRepo_;
    DeliveryAttemptRepository attemptRepo_;

    boost::asio::io_context ioContext_;
    std::unique_ptr<AMQP::LibBoostAsioHandler> handler_;
    std::unique_ptr<AMQP::TcpConnection> connection_;
    std::unique_ptr<AMQP::TcpChannel> amqpChannel_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};

// Owns `workersPerChannel` WorkerThreads for each of EMAIL/SMS/PUSH.
class WorkerPool {
public:
    WorkerPool(const Config& config, ConnectionPool& dbPool);
    ~WorkerPool();

    void start();
    void stop();

private:
    std::vector<std::unique_ptr<WorkerThread>> workers_;
};

}  // namespace notification
