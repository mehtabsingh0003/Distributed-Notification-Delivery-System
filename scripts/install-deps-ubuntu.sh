#!/usr/bin/env bash
# Installs build dependencies on Ubuntu 22.04/24.04.
# AMQP-CPP is not packaged by apt, so it's built from source.
set -euo pipefail

sudo apt-get update
sudo apt-get install -y \
    build-essential cmake git pkg-config \
    libboost-all-dev \
    libssl-dev \
    uuid-dev \
    nlohmann-json3-dev \
    libmysqlcppconn-dev \
    mysql-server \
    rabbitmq-server

# --- AMQP-CPP (build from source, no apt package on Debian/Ubuntu) ---
if [ ! -d /tmp/AMQP-CPP ]; then
    git clone --depth 1 https://github.com/CopernicaMarketingSoftware/AMQP-CPP.git /tmp/AMQP-CPP
fi
cmake -S /tmp/AMQP-CPP -B /tmp/AMQP-CPP/build \
    -DAMQP-CPP_BUILD_SHARED=ON -DAMQP-CPP_LINUX_TCP=ON -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/AMQP-CPP/build -j"$(nproc)"
sudo cmake --install /tmp/AMQP-CPP/build
sudo ldconfig

echo "Dependencies installed. Next: mysql < sql/schema.sql, then cmake -S . -B build && cmake --build build"
