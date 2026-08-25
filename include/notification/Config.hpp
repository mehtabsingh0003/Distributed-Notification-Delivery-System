#pragma once

#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace notification {

struct Config {
    // MySQL
    std::string dbHost = "127.0.0.1";
    int dbPort = 33060;  // X Protocol port
    std::string dbUser = "notification_app";
    std::string dbPassword;
    std::string dbSchema = "notification_db";
    int dbPoolSize = 8;

    // RabbitMQ
    std::string amqpHost = "127.0.0.1";
    int amqpPort = 5672;
    std::string amqpUser = "guest";
    std::string amqpPassword = "guest";
    std::string amqpVhost = "/";

    // Workers
    int workersPerChannel = 4;
    int maxRetries = 3;
    std::vector<int> retryDelaysMs = {5000, 15000, 60000};

    // Circuit breaker
    int cbFailureThreshold = 5;
    int cbOpenDurationMs = 30000;

    // HTTP API
    std::string httpAddress = "0.0.0.0";
    unsigned short httpPort = 8080;
    int httpThreads = 4;

    static Config loadFromFile(const std::string& path) {
        std::ifstream in(path);
        if (!in) {
            throw std::runtime_error("could not open config file: " + path);
        }
        nlohmann::json j;
        in >> j;

        Config c;
        c.dbHost = j.value("dbHost", c.dbHost);
        c.dbPort = j.value("dbPort", c.dbPort);
        c.dbUser = j.value("dbUser", c.dbUser);
        c.dbPassword = j.value("dbPassword", c.dbPassword);
        c.dbSchema = j.value("dbSchema", c.dbSchema);
        c.dbPoolSize = j.value("dbPoolSize", c.dbPoolSize);

        c.amqpHost = j.value("amqpHost", c.amqpHost);
        c.amqpPort = j.value("amqpPort", c.amqpPort);
        c.amqpUser = j.value("amqpUser", c.amqpUser);
        c.amqpPassword = j.value("amqpPassword", c.amqpPassword);
        c.amqpVhost = j.value("amqpVhost", c.amqpVhost);

        c.workersPerChannel = j.value("workersPerChannel", c.workersPerChannel);
        c.maxRetries = j.value("maxRetries", c.maxRetries);
        if (j.contains("retryDelaysMs")) {
            c.retryDelaysMs = j.at("retryDelaysMs").get<std::vector<int>>();
        }

        c.cbFailureThreshold = j.value("cbFailureThreshold", c.cbFailureThreshold);
        c.cbOpenDurationMs = j.value("cbOpenDurationMs", c.cbOpenDurationMs);

        c.httpAddress = j.value("httpAddress", c.httpAddress);
        c.httpPort = j.value("httpPort", c.httpPort);
        c.httpThreads = j.value("httpThreads", c.httpThreads);

        return c;
    }
};

}  // namespace notification
