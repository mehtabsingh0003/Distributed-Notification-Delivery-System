cmake_minimum_required(VERSION 3.16)
project(distributed_notification_system CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Boost 1.74 REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# amqpcpp does not ship a CMake config on most distros -> locate manually
find_path(AMQPCPP_INCLUDE_DIR amqpcpp.h PATH_SUFFIXES amqpcpp)
find_library(AMQPCPP_LIBRARY NAMES amqpcpp)
if(NOT AMQPCPP_INCLUDE_DIR OR NOT AMQPCPP_LIBRARY)
    message(FATAL_ERROR "AMQP-CPP not found. Install libamqpcpp-dev or build from "
                         "https://github.com/CopernicaMarketingSoftware/AMQP-CPP")
endif()

# mysql-connector-cpp (X DevAPI / JDBC style, 8.x)
find_path(MYSQLCPPCONN_INCLUDE_DIR mysqlx/xdevapi.h)
find_library(MYSQLCPPCONN_LIBRARY NAMES mysqlcppconn8 mysqlcppconn)
if(NOT MYSQLCPPCONN_INCLUDE_DIR OR NOT MYSQLCPPCONN_LIBRARY)
    message(FATAL_ERROR "mysql-connector-cpp not found. Install libmysqlcppconn-dev "
                         "(Debian/Ubuntu) or the MySQL Connector/C++ 8.x SDK.")
endif()

find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp)
if(NOT NLOHMANN_JSON_INCLUDE_DIR)
    message(FATAL_ERROR "nlohmann/json not found. Install nlohmann-json3-dev.")
endif()

add_executable(notification_service
    src/main.cpp
    src/Database.cpp
    src/NotificationRepository.cpp
    src/DeliveryAttemptRepository.cpp
    src/MockProviders.cpp
    src/ProviderRouter.cpp
    src/RabbitMQPublisher.cpp
    src/RabbitMQTopology.cpp
    src/Worker.cpp
    src/HttpServer.cpp
)

target_include_directories(notification_service PRIVATE
    include
    ${AMQPCPP_INCLUDE_DIR}
    ${MYSQLCPPCONN_INCLUDE_DIR}
    ${NLOHMANN_JSON_INCLUDE_DIR}
)

target_link_libraries(notification_service PRIVATE
    Boost::system
    Boost::thread
    Threads::Threads
    ${AMQPCPP_LIBRARY}
    ${MYSQLCPPCONN_LIBRARY}
    ssl
    crypto
    uuid
)

target_compile_options(notification_service PRIVATE -Wall -Wextra -O2)
