#include "notification/HttpServer.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

#include "notification/IdGenerator.hpp"
#include "notification/Logger.hpp"
#include "notification/RabbitMQTopology.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

namespace notification {

namespace {

http::response<http::string_body> jsonResponse(http::status status, const nlohmann::json& body,
                                                 unsigned version) {
    http::response<http::string_body> res{status, version};
    res.set(http::field::server, "notification-service");
    res.set(http::field::content_type, "application/json");
    res.body() = body.dump();
    res.prepare_payload();
    return res;
}

nlohmann::json notificationToJson(const Notification& n) {
    return {
        {"id", n.id},
        {"idempotencyKey", n.idempotencyKey},
        {"channel", toString(n.channel)},
        {"priority", toString(n.priority)},
        {"recipient", n.recipient},
        {"subject", n.subject},
        {"status", toString(n.status)},
        {"attemptCount", n.attemptCount},
    };
}

nlohmann::json attemptToJson(const DeliveryAttempt& a) {
    nlohmann::json result = {
        {"attemptNumber", a.attemptNumber},
        {"status", toString(a.status)}
    };

    if (a.providerRef) {
        result["providerRef"] = *a.providerRef;
    } else {
        result["providerRef"] = nullptr;
    }

    if (a.errorMessage) {
        result["errorMessage"] = *a.errorMessage;
    } else {
        result["errorMessage"] = nullptr;
    }

    return result;
}

}  // namespace

HttpServer::HttpServer(const Config& config, ConnectionPool& dbPool, RabbitMQPublisher& publisher)
    : config_(config),
      notificationRepo_(dbPool),
      attemptRepo_(dbPool),
      publisher_(publisher),
      ioContext_(config.httpThreads),
      acceptor_(ioContext_, tcp::endpoint(asio::ip::make_address(config.httpAddress), config.httpPort)) {}

HttpServer::~HttpServer() { stop(); }

void HttpServer::start() {
    for (int i = 0; i < config_.httpThreads; ++i) {
        threads_.emplace_back([this] { acceptLoop(); });
    }
    LOG_INFO("HttpServer", "listening on " + config_.httpAddress + ":" + std::to_string(config_.httpPort) +
                                " with " + std::to_string(config_.httpThreads) + " acceptor threads");
}

void HttpServer::stop() {
    boost::system::error_code ec;
    acceptor_.close(ec);
    ioContext_.stop();
    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
    threads_.clear();
}

void HttpServer::acceptLoop() {
    // Each thread runs its own blocking accept()/handle() loop against the
    // shared listening acceptor (SO_REUSEPORT-style fan-out via Asio's
    // io_context::run() on multiple threads sharing one acceptor). Simpler
    // to reason about than a fully async per-connection state machine, and
    // plenty for a control-plane API whose real work (MySQL write, AMQP
    // publish) already happens off this thread's hot path via the pool.
    for (;;) {
        boost::system::error_code ec;
        tcp::socket socket(ioContext_);
        acceptor_.accept(socket, ec);
        if (ec) {
            if (ec == asio::error::operation_aborted) return;  // acceptor closed during shutdown
            LOG_WARN("HttpServer", "accept error: " + ec.message());
            continue;
        }
        handleConnection(std::move(socket));
    }
}

void HttpServer::handleConnection(tcp::socket socket) {
    beast::error_code ec;
    beast::flat_buffer buffer;

    http::request<http::string_body> req;
    http::read(socket, buffer, req, ec);
    if (ec) return;

    http::response<http::string_body> res;
    const std::string target(req.target());

    try {
        if (req.method() == http::verb::get && target == "/healthz") {
            res = jsonResponse(http::status::ok, {{"status", "ok"}}, req.version());

        } else if (req.method() == http::verb::post && target == "/api/notifications") {
            auto j = nlohmann::json::parse(req.body());

            std::string idempotencyKey = j.value("idempotencyKey", IdGenerator::uuid4());

            Notification n;
            n.idempotencyKey = idempotencyKey;
            n.channel = channelFromString(j.at("channel").get<std::string>());
            n.priority = priorityFromString(j.value("priority", std::string("NORMAL")));
            n.recipient = j.at("recipient").get<std::string>();
            n.subject = j.value("subject", std::string());
            n.body = j.at("body").get<std::string>();

            Notification saved = notificationRepo_.insertWithOutbox(n);

            // In this reference implementation the API publishes directly
            // after the DB commit for simplicity; a production deployment
            // should instead run the OutboxPublisher poller described in
            // docs/ARCHITECTURE.md so publish failures can't strand a
            // notification in PENDING (see README "Known simplifications").
            nlohmann::json msg{{"notificationId", saved.id}, {"attemptNumber", 1},
                                {"priority", priorityWeight(saved.priority)}};
            publisher_.publishToExchange(RabbitMQTopology::exchangeName(),
                                          RabbitMQTopology::routingKey(saved.channel), msg.dump(),
                                          priorityWeight(saved.priority));

            res = jsonResponse(http::status::accepted, notificationToJson(saved), req.version());

        } else if (req.method() == http::verb::get && target.rfind("/api/notifications/", 0) == 0) {
            std::string id = target.substr(std::string("/api/notifications/").size());
            auto maybeN = notificationRepo_.findById(id);
            if (!maybeN) {
                res = jsonResponse(http::status::not_found, {{"error", "not found"}}, req.version());
            } else {
                auto attempts = attemptRepo_.findByNotificationId(id);
                nlohmann::json body = notificationToJson(*maybeN);
                nlohmann::json attemptsJson = nlohmann::json::array();
                for (auto& a : attempts) attemptsJson.push_back(attemptToJson(a));
                body["deliveryAttempts"] = attemptsJson;
                res = jsonResponse(http::status::ok, body, req.version());
            }

        } else if (req.method() == http::verb::get && target.rfind("/api/notifications", 0) == 0) {
            auto notifications = notificationRepo_.list(std::nullopt, std::nullopt, 50, 0);
            nlohmann::json arr = nlohmann::json::array();
            for (auto& n : notifications) arr.push_back(notificationToJson(n));
            res = jsonResponse(http::status::ok, {{"notifications", arr}}, req.version());

        } else {
            res = jsonResponse(http::status::not_found, {{"error", "no such route"}}, req.version());
        }

    } catch (const std::exception& ex) {
        LOG_WARN("HttpServer", std::string("request failed: ") + ex.what());
        res = jsonResponse(http::status::bad_request, {{"error", ex.what()}}, req.version());
    }

    res.keep_alive(req.keep_alive());
    http::write(socket, res, ec);
    socket.shutdown(tcp::socket::shutdown_send, ec);
}

}  // namespace notification
