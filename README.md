# SignalDesk

> A fault-tolerant C++17 notification delivery platform with RabbitMQ, MySQL, retries, circuit breakers, dead-letter queues, and a React monitoring dashboard.

SignalDesk accepts notification requests through a REST API, persists them in MySQL, routes delivery work through RabbitMQ, and processes Email, SMS, and Push notifications using independent worker pools.

The project uses mock notification providers for local development and failure simulation. No real Email, SMS, or Push messages are sent by default.

## Contents

- [Features](#features)
- [Architecture](#architecture)
- [Technology Stack](#technology-stack)
- [Project Structure](#project-structure)
- [Requirements](#requirements)
- [Run Locally](#run-locally)
- [Configuration](#configuration)
- [API Reference](#api-reference)
- [RabbitMQ Management](#rabbitmq-management)
- [Database Schema](#database-schema)
- [Testing and Validation](#testing-and-validation)
- [Production Considerations](#production-considerations)
- [Future Improvements](#future-improvements)
- [Contributing](#contributing)
- [License](#license)

## Features

- Email, SMS, and Push notification channels
- C++17 backend with Boost.Beast REST API
- MySQL persistence with connection pooling
- Transactional notification and outbox writes
- RabbitMQ channel routing and worker pools
- Configurable retry queues with exponential backoff
- Per-channel circuit breakers
- Channel-specific dead-letter queues
- Idempotent notification submission
- Delivery-attempt history and provider references
- `/healthz` health-check endpoint
- React and TypeScript monitoring dashboard
- Mock providers for predictable local testing

## Architecture

```text
                         +-----------------------+
                         | React Dashboard       |
                         | or API Client          |
                         +-----------+-----------+
                                     |
                                     v
                         +-----------------------+
                         | Boost.Beast REST API   |
                         +-----------+-----------+
                                     |
                    +----------------+----------------+
                    |                                 |
                    v                                 v
          +-------------------+             +-------------------+
          | MySQL            |             | RabbitMQ          |
          | notifications    |             | channel queues    |
          | delivery_attempts|             | retry queues      |
          | outbox_events    |             | dead-letter queues|
          +-------------------+             +---------+---------+
                                                     |
                                                     v
                                      +--------------------------+
                                      | C++ workers              |
                                      | Email / SMS / Push       |
                                      +------------+-------------+
                                                   |
                                                   v
                                      +--------------------------+
                                      | Mock provider adapters   |
                                      +--------------------------+
```

### Notification flow

1. The client submits a notification to `POST /api/notifications`.
2. The API writes the notification and outbox event in one MySQL transaction.
3. The message is published to the RabbitMQ route for its channel.
4. A channel worker calls the provider adapter.
5. Each delivery attempt is recorded in MySQL.
6. Transient failures move through the retry queue ladder.
7. Messages that exhaust retries are sent to the channel dead-letter queue.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the detailed design and queue topology.

## Technology Stack

| Layer | Technology | Purpose |
| --- | --- | --- |
| Backend | C++17 | Core service and workers |
| HTTP | Boost.Beast | REST API server |
| Messaging | RabbitMQ and AMQP-CPP | Asynchronous delivery work |
| Database | MySQL and Connector/C++ | Durable state and audit history |
| JSON | nlohmann/json | Request and response serialization |
| Build | CMake | Native backend build |
| Frontend | React and TypeScript | Monitoring dashboard |
| Frontend tooling | Vite and Tailwind CSS | Development and styling |
| Icons | Lucide React | Dashboard iconography |

## Project Structure

```text
.
├── include/notification/       C++ headers
├── src/                        C++ implementations and service entry point
├── sql/schema.sql              MySQL schema and application user grant
├── config/config.json          Runtime configuration template
├── frontend/                   React/Vite dashboard
├── scripts/                    Dependency installation scripts
├── docs/ARCHITECTURE.md        Architecture documentation
├── CMakeLists.txt              Backend build configuration
├── .gitignore
└── README.md
```

## Requirements

### Backend

- Ubuntu 22.04 or 24.04 recommended
- CMake 3.16 or newer
- GCC or another C++17-compatible compiler
- Boost.System and Boost.Thread
- OpenSSL
- libuuid
- nlohmann-json
- MySQL 8+
- MySQL Connector/C++ 8
- RabbitMQ 3+
- AMQP-CPP

### Frontend

- Node.js 18 or newer
- npm
- A modern web browser

The included installer supports Ubuntu 22.04 and 24.04:

```bash
./scripts/install-deps-ubuntu.sh
```

## Run Locally

### 1. Start MySQL and RabbitMQ

Install the required services, then start them on Ubuntu:

```bash
sudo systemctl start mysql
sudo systemctl start rabbitmq-server
```

Optional service checks:

```bash
sudo systemctl status mysql
sudo systemctl status rabbitmq-server
```

### 2. Initialize the database

From the repository root:

```bash
sudo mysql < sql/schema.sql
```

The schema creates the `notification_db` database, its application tables, and the `notification_app` user. The sample password is `change-me`; change it before any shared or production deployment.

### 3. Configure the service

Edit [config/config.json](config/config.json) for your local database, RabbitMQ, worker, retry, and HTTP settings. Keep real credentials out of Git. The repository `.gitignore` allows local overrides such as `config/config.local.json`.

### 4. Build the backend

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j1
```

Use a larger build parallelism value, such as `-j4`, if the machine has enough memory.

### 5. Start SignalDesk

```bash
./build/notification_service config/config.json
```

The REST API listens on `http://localhost:8080` by default.

### 6. Start the dashboard

Open a second terminal:

```bash
cd frontend
npm install
npm run dev
```

Open the local Vite URL printed in the terminal. The development server proxies `/api` and `/healthz` to `http://127.0.0.1:8080`.

Build the static frontend for deployment:

```bash
cd frontend
npm run build
```

Generated files are written to `frontend/dist/`.

## Configuration

The default runtime configuration is in [config/config.json](config/config.json).

| Key | Description | Default |
| --- | --- | --- |
| `dbHost`, `dbPort` | MySQL connection | `127.0.0.1`, `33060` |
| `dbUser`, `dbPassword`, `dbSchema` | Database credentials and schema | `notification_app`, `change-me`, `notification_db` |
| `dbPoolSize` | Database pool size | `8` |
| `amqpHost`, `amqpPort` | RabbitMQ connection | `127.0.0.1`, `5672` |
| `amqpUser`, `amqpPassword`, `amqpVhost` | RabbitMQ credentials | `guest`, `guest`, `/` |
| `workersPerChannel` | Workers per channel | `4` |
| `maxRetries` | Maximum retry count | `3` |
| `retryDelaysMs` | Retry delays in milliseconds | `[5000, 15000, 60000]` |
| `cbFailureThreshold` | Failures before a circuit opens | `5` |
| `cbOpenDurationMs` | Circuit-open duration in milliseconds | `30000` |
| `httpAddress`, `httpPort` | REST API bind address and port | `0.0.0.0`, `8080` |
| `httpThreads` | HTTP worker threads | `4` |

### Local service ports

| Port | Service |
| ---: | --- |
| `33060` | MySQL X DevAPI |
| `5672` | RabbitMQ AMQP |
| `8080` | SignalDesk REST API |
| `15672` | RabbitMQ Management UI |

## API Reference

### Health check

```bash
curl http://localhost:8080/healthz
```

Response:

```json
{"status":"ok"}
```

### Create a notification

```bash
curl -X POST http://localhost:8080/api/notifications \
  -H "Content-Type: application/json" \
  -d '{
    "channel": "EMAIL",
    "priority": "HIGH",
    "recipient": "user@example.com",
    "subject": "Welcome",
    "body": "Your account is ready."
  }'
```

The endpoint returns `202 Accepted` after persistence and publish succeed.

Supported values:

- `channel`: `EMAIL`, `SMS`, or `PUSH`
- `priority`: `LOW`, `NORMAL`, or `HIGH`

Optional idempotency key:

```json
{
  "idempotencyKey": "order-123-welcome",
  "channel": "EMAIL",
  "priority": "NORMAL",
  "recipient": "user@example.com",
  "subject": "Welcome",
  "body": "Your account is ready."
}
```

Submitting the same idempotency key again prevents a duplicate notification.

### List recent notifications

```bash
curl http://localhost:8080/api/notifications
```

The endpoint returns up to 50 recent notifications:

```json
{
  "notifications": [
    {
      "id": "notification-id",
      "channel": "EMAIL",
      "priority": "HIGH",
      "recipient": "user@example.com",
      "subject": "Welcome",
      "status": "SENT",
      "attemptCount": 1
    }
  ]
}
```

### Get notification details

```bash
curl http://localhost:8080/api/notifications/<notification-id>
```

The detail response includes the notification and its `deliveryAttempts`:

```json
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
```

## RabbitMQ Management

Enable the RabbitMQ management plugin if it is not already enabled:

```bash
sudo rabbitmq-plugins enable rabbitmq_management
```

Then open `http://localhost:15672`. The default local credentials are `guest` / `guest`.

The management UI can be used to inspect exchanges, queues, consumers, retry queues, messages, and dead-letter queues.

## Database Schema

SignalDesk uses three primary tables:

| Table | Purpose |
| --- | --- |
| `notifications` | Original request, current status, recipient, and attempt count |
| `delivery_attempts` | Provider result for every delivery attempt |
| `outbox_events` | Events created with a notification for delivery publication |

Important fields include notification status, retry attempt number, provider reference, error message, creation time, update time, and publication time.

## Mock Providers

The local provider adapters are:

- `MockEmailProvider`
- `MockSmsProvider`
- `MockPushProvider`

They simulate successful delivery, transient failure, and permanent failure. The simulated failures make retry, dead-letter, and circuit-breaker behavior observable without external provider accounts. No real notification is sent by default.

## Testing and Validation

Build the backend:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j1
```

Build the frontend:

```bash
cd frontend
npm install
npm run build
```

Smoke-test a running service:

```bash
curl http://localhost:8080/healthz
curl http://localhost:8080/api/notifications
```

Create a test notification:

```bash
curl -X POST http://localhost:8080/api/notifications \
  -H "Content-Type: application/json" \
  -d '{
    "channel": "EMAIL",
    "priority": "HIGH",
    "recipient": "user@example.com",
    "subject": "SignalDesk Test",
    "body": "Testing notification delivery."
  }'
```

## Production Considerations

SignalDesk is primarily a learning and reference project. Before deploying publicly:

- Replace mock providers with authenticated Email, SMS, and Push integrations.
- Add API authentication, authorization, rate limiting, and TLS.
- Move credentials to environment variables or a secret manager.
- Add a dedicated outbox publisher or recovery worker.
- Use managed or separately monitored MySQL and RabbitMQ services.
- Add structured logs, metrics, tracing, backups, and alerting.
- Add readiness probes and restrict network access to infrastructure services.
- Pin dependency versions and add CI security checks.
- Add automated tests for retries, idempotency, provider failures, and database errors.

The current implementation writes the notification and outbox event transactionally, then publishes directly from the API path. A dedicated outbox publisher would remove the failure window between database commit and RabbitMQ publish.

GitHub can host this source repository and the built static dashboard through GitHub Pages. GitHub Pages does not run the C++ backend, MySQL, or RabbitMQ; the backend requires a VPS, container platform, or managed cloud deployment.

## Future Improvements

- Real provider adapters
- Dedicated outbox publisher
- JWT authentication and authorization
- API rate limiting
- Prometheus metrics and OpenTelemetry tracing
- Docker and Docker Compose support
- CI/CD pipeline
- Kubernetes or managed infrastructure deployment
- Automated integration and end-to-end tests

## Contributing

1. Create a focused feature branch.
2. Make the smallest change that solves the problem.
3. Build both the backend and frontend.
4. Update documentation when behavior changes.
5. Open a pull request with validation steps.

Do not commit generated build directories, dependency folders, credentials, or local configuration files.

## License

This project is currently for personal and educational use. A formal license will be added later.
