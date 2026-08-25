#include <csignal>
#include <iostream>

#include "notification/Config.hpp"
#include "notification/Database.hpp"
#include "notification/HttpServer.hpp"
#include "notification/Logger.hpp"
#include "notification/RabbitMQPublisher.hpp"
#include "notification/Worker.hpp"

namespace {
std::atomic<bool> g_shutdown{false};
void handleSignal(int) { g_shutdown = true; }
}  // namespace

int main(int argc, char** argv) {
    using namespace notification;

    std::string configPath = argc > 1 ? argv[1] : "config/config.json";

    Config config;
    try {
        config = Config::loadFromFile(configPath);
    } catch (const std::exception& ex) {
        std::cerr << "Failed to load config from " << configPath << ": " << ex.what() << std::endl;
        return 1;
    }

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    LOG_INFO("main", "starting distributed notification service");

    ConnectionPool dbPool(config, static_cast<size_t>(config.dbPoolSize));

    RabbitMQPublisher publisher(config);
    publisher.declareTopologyBlocking(config);

    WorkerPool workers(config, dbPool);
    workers.start();

    HttpServer httpServer(config, dbPool, publisher);
    httpServer.start();

    LOG_INFO("main", "service ready");

    while (!g_shutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    LOG_INFO("main", "shutting down");
    httpServer.stop();
    workers.stop();

    return 0;
}
