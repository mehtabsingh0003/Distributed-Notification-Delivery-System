# SignalDesk

> A fault-tolerant C++17 notification delivery platform built with RabbitMQ, MySQL, retries, circuit breakers, dead-letter queues, and a React monitoring dashboard.

SignalDesk is a distributed notification delivery system designed to demonstrate how reliable backend infrastructure can be built using modern C++.

It accepts notification requests through a REST API, persists them in MySQL, routes delivery work through RabbitMQ, and processes Email, SMS, and Push notifications using independent worker pools.

The project currently uses **mock notification providers**, making it suitable for local development and learning without requiring external Email, SMS, or Push provider accounts.

---

## 🚀 Features

- 📧 Email notification channel
- 📱 SMS notification channel
- 🔔 Push notification channel
- ⚡ C++17 backend
- 🌐 REST API using Boost.Beast
- 🗄️ MySQL persistence
- 🐇 RabbitMQ message broker
- 🔄 Retry queues with configurable delays
- 🛡️ Per-channel circuit breakers
- 💀 Dead-letter queues
- 🔑 Idempotent notification submission
- 📊 Delivery attempt history
- ❤️ Health-check endpoint
- 🧵 Multi-threaded worker pools
- 📦 Transactional notification + outbox writes
- 🖥️ React/Vite monitoring dashboard
- 🧪 Mock providers for failure simulation

---

# 🏗️ Architecture

```text
                         ┌───────────────────────┐
                         │     React Dashboard   │
                         │      / API Client     │
                         └───────────┬───────────┘
                                     │
                                     ▼
                         ┌───────────────────────┐
                         │   Boost.Beast REST    │
                         │         API           │
                         └───────────┬───────────┘
                                     │
                    ┌────────────────┴────────────────┐
                    │                                 │
                    ▼                                 ▼
          ┌──────────────────┐              ┌──────────────────┐
          │      MySQL       │              │     RabbitMQ     │
          │                  │              │                  │
          │ Notifications    │              │ Channel Queues   │
          │ Outbox Events    │              │ Retry Queues     │
          │ Delivery Attempts│              │ Dead Letter Queues│
          └──────────────────┘              └────────┬─────────┘
                                                     │
                                                     ▼
                                      ┌────────────────────────┐
                                      │      C++ Workers        │
                                      │                        │
                                      │ Email │ SMS │ Push     │
                                      └────────────┬───────────┘
                                                   │
                                                   ▼
                                      ┌────────────────────────┐
                                      │    Provider Adapters    │
                                      │      Mock Providers     │
                                      └────────────────────────┘
🔄 Notification Flow
Client
  │
  │ POST /api/notifications
  ▼
REST API
  │
  ├──► MySQL
  │      ├── notification
  │      └── outbox event
  │
  └──► RabbitMQ
           │
           ├── EMAIL
           ├── SMS
           └── PUSH
                │
                ▼
             Worker
                │
                ▼
          Provider Adapter
                │
        ┌───────┴────────┐
        │                │
      Success          Failure
        │                │
        ▼                ▼
      SENT        Retry / Circuit Breaker
                         │
                         ▼
                   Retry Queue
                         │
                         ▼
                    Worker Again
                         │
                         ▼
                 Maximum Retries?
                    /        \
                  No          Yes
                  │            │
                  ▼            ▼
                Retry          DLQ
✨ Reliability Features

SignalDesk demonstrates several concepts commonly used in distributed systems.

Transactional Persistence

The notification and its outbox event are written inside the same MySQL transaction.

BEGIN TRANSACTION

INSERT notification
INSERT outbox_event

COMMIT

This prevents the notification record from being created without its corresponding delivery event.

RabbitMQ Routing

Notifications are routed according to their channel:

EMAIL → q.email
SMS   → q.sms
PUSH  → q.push

Each channel has its own worker pool.

Retry Mechanism

Transient provider failures are routed through retry queues.

Default retry delays:

Attempt 1
   │
   └── 5 seconds
          │
          ▼
Attempt 2
   │
   └── 15 seconds
          │
          ▼
Attempt 3
   │
   └── 60 seconds
          │
          ▼
       Final failure
          │
          ▼
         DLQ

The retry behavior is configurable through config/config.json.

Circuit Breaker

Each notification channel has its own circuit breaker.

Example:

Provider failures
      │
      ▼
Failure threshold reached
      │
      ▼
Circuit OPEN
      │
      ▼
Requests temporarily blocked
      │
      ▼
Open duration expires
      │
      ▼
Circuit attempts recovery

This prevents repeated failures from continuously hitting an unhealthy provider.

Idempotency

Clients can provide an idempotencyKey.

Example:

{
  "idempotencyKey": "order-123-welcome",
  "channel": "EMAIL",
  "priority": "HIGH",
  "recipient": "user@example.com",
  "subject": "Welcome",
  "body": "Your account is ready."
}

Submitting the same idempotency key again does not create a duplicate notification.

Delivery Attempt Tracking

Every delivery attempt is stored in MySQL.

Example:

Notification
    │
    ├── Attempt 1 → TRANSIENT_FAILURE
    │
    ├── Attempt 2 → TRANSIENT_FAILURE
    │
    └── Attempt 3 → SUCCESS

This makes it possible to inspect the complete delivery history.

🛠️ Tech Stack
Backend
Technology	Purpose
C++17	Core backend
CMake	Build system
Boost.Beast	HTTP REST server
Boost	Threading and system utilities
MySQL	Persistent storage
MySQL Connector/C++	MySQL X DevAPI
RabbitMQ	Message broker
AMQP-CPP	RabbitMQ integration
OpenSSL	Secure communication support
nlohmann/json	JSON processing
Frontend
Technology	Purpose
React	UI
TypeScript	Frontend language
Vite	Development/build tooling
Tailwind CSS	Styling
Lucide	Icons
📁 Project Structure
SignalDesk/
│
├── include/
│   └── notification/
│       ├── Config.hpp
│       ├── Database.hpp
│       ├── DeliveryAttemptRepository.hpp
│       ├── Enums.hpp
│       ├── HttpServer.hpp
│       ├── IdGenerator.hpp
│       ├── Logger.hpp
│       ├── MockProviders.hpp
│       ├── Models.hpp
│       ├── NotificationRepository.hpp
│       ├── Provider.hpp
│       ├── ProviderRouter.hpp
│       ├── RabbitMQPublisher.hpp
│       ├── RabbitMQTopology.hpp
│       └── Worker.hpp
│
├── src/
│   ├── main.cpp
│   ├── Database.cpp
│   ├── DeliveryAttemptRepository.cpp
│   ├── HttpServer.cpp
│   ├── MockProviders.cpp
│   ├── NotificationRepository.cpp
│   ├── ProviderRouter.cpp
│   ├── RabbitMQPublisher.cpp
│   ├── RabbitMQTopology.cpp
│   └── Worker.cpp
│
├── sql/
│   └── schema.sql
│
├── config/
│   └── config.json
│
├── frontend/
│   ├── src/
│   ├── package.json
│   └── vite.config.*
│
├── scripts/
│   └── install-deps-ubuntu.sh
│
├── docs/
│   └── ARCHITECTURE.md
│
├── CMakeLists.txt
├── .gitignore
└── README.md
💻 Requirements
Backend

Recommended environment:

Ubuntu 22.04 / 24.04
CMake 3.16+
GCC with C++17 support
Boost.System
Boost.Thread
OpenSSL
libuuid
nlohmann-json
MySQL 8+
MySQL Connector/C++ 8
RabbitMQ 3+
AMQP-CPP
Frontend
Node.js 18+
npm
Modern web browser

The backend is designed primarily for Linux/WSL development because MySQL, RabbitMQ, and AMQP-CPP are used as native Linux dependencies.

🚀 Running Locally
1. Install Dependencies

From the project root:

./scripts/install-deps-ubuntu.sh

The script installs the required system dependencies and builds AMQP-CPP when necessary.

2. Start MySQL and RabbitMQ
sudo systemctl start mysql
sudo systemctl start rabbitmq-server

Check MySQL:

sudo systemctl status mysql

Check RabbitMQ:

sudo systemctl status rabbitmq-server
3. Initialize MySQL

Run:

sudo mysql < sql/schema.sql

The schema creates:

notification_db
│
├── notifications
├── delivery_attempts
└── outbox_events

It also creates the application user:

notification_app

For local development, the default password in the schema is:

change-me

For real deployments, change this password and never commit production credentials.

⚙️ Configuration

The default configuration is:

config/config.json

Example:

{
  "dbHost": "127.0.0.1",
  "dbPort": 33060,
  "dbUser": "notification_app",
  "dbPassword": "change-me",
  "dbSchema": "notification_db",
  "dbPoolSize": 8,

  "amqpHost": "127.0.0.1",
  "amqpPort": 5672,
  "amqpUser": "guest",
  "amqpPassword": "guest",
  "amqpVhost": "/",

  "workersPerChannel": 4,
  "maxRetries": 3,
  "retryDelaysMs": [5000, 15000, 60000],

  "cbFailureThreshold": 5,
  "cbOpenDurationMs": 30000,

  "httpAddress": "0.0.0.0",
  "httpPort": 8080,
  "httpThreads": 4
}
Important Ports
Port	Service
33060	MySQL X DevAPI
5672	RabbitMQ
8080	SignalDesk REST API
15672	RabbitMQ Management UI
🔨 Build Backend

From the project root:

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

Then:

cmake --build build -j1

-j1 is useful on machines with limited RAM.

After successful compilation:

build/notification_service

will be created.

▶️ Start SignalDesk
./build/notification_service config/config.json

A successful startup should show messages similar to:

ConnectionPool initialized
RabbitMQ topology declared
WorkerPool started
HttpServer listening on 0.0.0.0:8080
service ready
🌐 API
Health Check
curl http://localhost:8080/healthz

Response:

{
  "status": "ok"
}
📤 Create Notification
curl -X POST http://localhost:8080/api/notifications \
  -H "Content-Type: application/json" \
  -d '{
    "channel": "EMAIL",
    "priority": "HIGH",
    "recipient": "user@example.com",
    "subject": "Welcome",
    "body": "Your account is ready."
  }'

Supported channels:

EMAIL
SMS
PUSH

Supported priorities:

LOW
NORMAL
HIGH

The API returns a notification ID.

Example:

{
  "id": "f4bb11eb-09bd-4f69-b4e8-0df46f3002c9",
  "channel": "EMAIL",
  "priority": "HIGH",
  "recipient": "user@example.com",
  "status": "PENDING",
  "attemptCount": 0
}
🔍 List Notifications
curl http://localhost:8080/api/notifications

Example:

{
  "notifications": [
    {
      "id": "notification-id",
      "channel": "EMAIL",
      "priority": "HIGH",
      "recipient": "user@example.com",
      "status": "SENT",
      "attemptCount": 1
    }
  ]
}
🔎 Get Notification Details

Replace <notification-id> with the ID returned by the POST request:

curl http://localhost:8080/api/notifications/<notification-id>

The response includes:

Notification information
Current status
Attempt count
Provider reference
Delivery attempt history
Error information when applicable

Example:

{
  "id": "notification-id",
  "channel": "EMAIL",
  "status": "SENT",
  "attemptCount": 1,
  "deliveryAttempts": [
    {
      "attemptNumber": 1,
      "status": "SUCCESS",
      "providerRef": "email-provider-reference",
      "errorMessage": null
    }
  ]
}
🖥️ Run the Dashboard

Open another terminal:

cd frontend

Install dependencies:

npm install

Start the development server:

npm run dev

Vite will display the local dashboard URL.

The frontend proxies:

/api
/healthz

to the backend running on:

http://localhost:8080
📦 Production Frontend Build
cd frontend
npm install
npm run build

The generated files will be available in:

frontend/dist/
🐇 RabbitMQ Management

The RabbitMQ management plugin can be enabled with:

sudo rabbitmq-plugins enable rabbitmq_management

Then open:

http://localhost:15672

Default local credentials:

guest / guest

The management interface can be used to inspect:

Exchanges
Queues
Consumers
Messages
Retry queues
Dead-letter queues
🗄️ Database Schema

SignalDesk uses three primary tables:

notifications

Stores the notification request and current delivery state.

id
idempotency_key
channel
priority
recipient
subject
body
status
attempt_count
created_at
updated_at
delivery_attempts

Stores every provider delivery attempt.

id
notification_id
attempt_number
channel
status
provider_ref
error_message
attempted_at
outbox_events

Stores events associated with notification creation.

id
notification_id
event_type
payload
status
created_at
published_at
🧪 Mock Providers

The project intentionally uses mock providers for local development.

MockEmailProvider
MockSmsProvider
MockPushProvider

They simulate:

Successful delivery
       │
       ├── ~78% success
       │
       ├── ~10% transient failure
       │
       └── ~2% permanent failure

The simulated failures allow the retry and circuit-breaker mechanisms to be tested without external services.

No real Email, SMS, or Push message is sent by default.

🧠 What This Project Demonstrates

This project was designed to practice real backend and distributed-system concepts:

C++
Modern C++17
RAII
Smart pointers
Threads
Concurrency
Exception handling
Classes and interfaces
Backend
REST API design
HTTP servers
JSON APIs
Database connection pooling
Repository pattern
Provider abstraction
Distributed Systems
Message queues
Asynchronous processing
Retry strategies
Exponential backoff
Dead-letter queues
Idempotency
Circuit breakers
Transactional writes
At-least-once delivery concepts
DevOps / Infrastructure
Linux
WSL
CMake
MySQL
RabbitMQ
Dependency management
Service management
Frontend
React
TypeScript
Vite
Tailwind CSS
API integration
Monitoring dashboard
🧪 Testing and Validation

Backend build:

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j1

Frontend build:

cd frontend
npm install
npm run build

Health check:

curl http://localhost:8080/healthz

List notifications:

curl http://localhost:8080/api/notifications

Create a test notification:

curl -X POST http://localhost:8080/api/notifications \
  -H "Content-Type: application/json" \
  -d '{
    "channel": "EMAIL",
    "priority": "HIGH",
    "recipient": "user@example.com",
    "subject": "SignalDesk Test",
    "body": "Testing notification delivery."
  }'
⚠️ Project Status

SignalDesk is primarily a learning/reference project.

The current implementation uses:

Mock Email provider
Mock SMS provider
Mock Push provider

The infrastructure and reliability mechanisms are real, but external notification delivery is simulated.

The project is not production-hardened.

🔐 Production Improvements

Before using SignalDesk in a real production environment, consider adding:

Real Email provider integration
Real SMS provider integration
Real Push notification integration
API authentication
Authorization
Rate limiting
TLS
Secret management
Environment-based configuration
Production outbox publisher/recovery worker
Structured logging
Metrics
Distributed tracing
Automated tests
CI/CD
Database backups
RabbitMQ monitoring
Health/readiness probes
Containerization
Kubernetes or managed infrastructure
⚠️ Important Design Note

The current reference implementation writes the notification and outbox event transactionally, then publishes the RabbitMQ message directly from the API path.

Conceptually:

MySQL Transaction
       │
       ├── Notification
       └── Outbox Event
              │
              ▼
          Commit
              │
              ▼
        RabbitMQ Publish

A production implementation should use a dedicated Outbox Publisher that continuously reads pending outbox events and publishes them to RabbitMQ.

This removes the failure window between:

Database COMMIT
      │
      X
RabbitMQ PUBLISH
📈 Future Improvements

Possible future extensions:

 Real Email provider
 Real SMS provider
 Real Push provider
 Dedicated outbox publisher
 JWT authentication
 API rate limiting
 Prometheus metrics
 OpenTelemetry tracing
 Docker support
 Docker Compose
 CI/CD pipeline
 Kubernetes deployment
 Automated integration tests
 Load testing
 Authentication for RabbitMQ management
 Production secret management
🤝 Contributing

Contributions and improvements are welcome.

Basic workflow:

git checkout -b feature/my-feature

Make your changes, then build and test:

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j1

cd frontend
npm install
npm run build

Commit your changes:

git add .
git commit -m "Add my feature"
git push origin feature/my-feature

Then open a Pull Request.

🔒 Security

Do not commit:

Passwords
API keys
Provider credentials
Secret tokens
config.local.json
.env files
build directories
node_modules

Use environment variables or a secret manager for production credentials.

📄 License

No license has been selected for this project yet.

If you plan to distribute the project publicly, consider adding an appropriate LICENSE file.

👨‍💻 Author

Mehtab Singh

Computer Science & Engineering

Interested in:

Generative AI
AI Agents
Backend Engineering
Distributed Systems
C++
Full-Stack Development
Software Engineering

⭐ If you found this project useful or interesting, consider giving the repository a star.


This is the version I'd put on GitHub: **professional enough for a portfolio, but still honest that the providers are mocked and the system is a learning/reference implementation.** The underlying project description and architecture are consistent with the README you provided. :contentReference[oaicite:0]{index=0}