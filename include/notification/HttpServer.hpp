#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <vector>

#include "notification/Config.hpp"
#include "notification/Database.hpp"
#include "notification/DeliveryAttemptRepository.hpp"
#include "notification/NotificationRepository.hpp"
#include "notification/RabbitMQPublisher.hpp"

namespace notification {

// Low-latency REST API on Boost.Beast (no Boost.Asio coroutine framework
// overhead, no reflection/serialization framework -- just Beast's
// synchronous HTTP parser over a small fixed thread pool of blocking
// acceptor loops, which is enough throughput for a control-plane API
// backed by a connection-pooled MySQL write and one AMQP publish).
//
// Endpoints:
//   POST   /api/notifications            submit a notification
//   GET    /api/notifications/{id}       fetch one, with delivery history
//   GET    /api/notifications            list (filter by status/channel, paginated)
//   GET    /healthz                      liveness probe
class HttpServer {
public:
    HttpServer(const Config& config, ConnectionPool& dbPool, RabbitMQPublisher& publisher);
    ~HttpServer();

    void start();
    void stop();

private:
    void acceptLoop();
    void handleConnection(boost::asio::ip::tcp::socket socket);

    Config config_;
    NotificationRepository notificationRepo_;
    DeliveryAttemptRepository attemptRepo_;
    RabbitMQPublisher& publisher_;

    boost::asio::io_context ioContext_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::vector<std::thread> threads_;
};

}  // namespace notification
