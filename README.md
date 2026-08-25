# SignalDesk

> A C++17 notification delivery platform with RabbitMQ, MySQL, retries, circuit breakers, dead-letter queues, and a responsive monitoring dashboard.

SignalDesk accepts notification requests through a REST API, stores them durably, routes delivery work through RabbitMQ, and processes Email, SMS, and Push notifications through independent workers. The included React dashboard provides a live operational view of delivery activity and attempt history.

## Project Status

SignalDesk is a reference implementation for learning and extending distributed notification infrastructure. It includes mock providers for local development. Review the [production considerations](#production-considerations) before exposing it to the public internet.

## Contents

- [Highlights](#highlights)
- [Architecture](#architecture)
- [Repository Structure](#repository-structure)
- [Requirements](#requirements)
- [Run Locally](#run-locally)
- [Configuration](#configuration)
- [API Reference](#api-reference)
- [Reliability](#reliability)
- [Testing and Validation](#testing-and-validation)
- [Production Considerations](#production-considerations)
- [Contributing](#contributing)
- [License](#license)

## Highlights

| Area | Included |
| --- | --- |
| Channels | Email, SMS, and Push |
| API | Boost.Beast HTTP REST API |
| Persistence | MySQL with transactional writes |
| Messaging | RabbitMQ with channel routing |
| Resilience | Retry queues, exponential backoff, circuit breakers, and DLQs |
| Observability | Delivery-attempt history and `/healthz` health check |
| Dashboard | React, TypeScript, Vite, Tailwind CSS, and Lucide icons |
| Local development | Mock providers with configurable failure behavior |

## Architecture

```text
                 +----------------------+
                 |  React Dashboard     |
                 |  or API Client        |
                 +----------+-----------+
                            |
                            v
                 +----------------------+
                 |  Boost.Beast REST API |
                 +----------+-----------+
                            |
              +-------------+-------------+
              |                           |
              v                           v
   +----------------------+      +----------------------+
   | MySQL                |      | RabbitMQ             |
   | Notifications        |      | Channel queues       |
   | Outbox               |      | Retry queues         |
   | Delivery attempts    |      | Dead-letter queues   |
   +----------------------+      +----------+-----------+
                                           |
                                           v
                              +--------------------------+
                              | C++ channel workers      |
                              | Email / SMS / Push       |
                              +------------+-------------+
                                           |
                                           v
                              +--------------------------+
                              | Mock provider adapters   |
                              +--------------------------+
```

For the detailed request flow, queue topology, retry behavior, and design decisions, see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Repository Structure

```text
.
├── include/notification/       C++ public headers
├── src/                        C++ implementations and service entry point
├── sql/schema.sql              MySQL schema and application user grant
├── config/config.json          Runtime configuration template
├── frontend/                   React/Vite monitoring dashboard
├── scripts/                    Dependency installation scripts
├── docs/                       Architecture documentation
├── CMakeLists.txt              Backend build configuration
└── README.md
```

## Requirements

### Backend

- Ubuntu 22.04 or 24.04 recommended
- CMake 3.16+
- A C++17-compatible compiler
- Boost.System and Boost.Thread
- OpenSSL
- libuuid
- nlohmann-json
- MySQL Connector/C++ X DevAPI
- AMQP-CPP
- MySQL 8+
- RabbitMQ 3+

### Frontend

- Node.js 18+
- npm
- A modern browser

The included installer prepares the Ubuntu dependencies and local MySQL/RabbitMQ services:

```bash
./scripts/install-deps-ubuntu.sh
```

## Run Locally

### 1. Initialize MySQL

Start MySQL and RabbitMQ, then create the database schema:

```bash
mysql -u root -p < sql/schema.sql
```

The schema creates the application database and user defined by the SQL file. Change the password before using this outside a local environment.

### 2. Configure the service

Copy or edit `config/config.json` with values for your local services. Do not commit real passwords or tokens. Keep private overrides in an ignored file such as `config/config.local.json` and pass that file to the service.

### 3. Build the backend

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

### 4. Start the backend

```bash
./build/notification_service config/config.json
```

The API listens on `http://localhost:8080` by default.

### 5. Start the dashboard

Open a second terminal:

```bash
cd frontend
npm install
npm run dev
```

Open the local Vite URL printed in the terminal. The Vite development server proxies `/api` and `/healthz` to `http://127.0.0.1:8080`.

Build the dashboard for deployment:

```bash
cd frontend
npm run build
```

The generated static files are written to `frontend/dist/`.

## Configuration

The runtime configuration is JSON. The default file is [config/config.json](config/config.json).

| Key | Purpose | Example |
| --- | --- | --- |
| `dbHost`, `dbPort` | MySQL connection | `127.0.0.1`, `33060` |
| `dbUser`, `dbPassword`, `dbSchema` | Database credentials and schema | `notification_app` |
| `dbPoolSize` | Database connection pool size | `8` |
| `amqpHost`, `amqpPort` | RabbitMQ connection | `127.0.0.1`, `5672` |
| `amqpUser`, `amqpPassword`, `amqpVhost` | RabbitMQ credentials | `guest`, `guest`, `/` |
| `workersPerChannel` | Workers per notification channel | `4` |
| `maxRetries` | Maximum retry count | `3` |
| `retryDelaysMs` | Retry delay ladder in milliseconds | `[5000, 15000, 60000]` |
| `cbFailureThreshold` | Failures before opening a circuit | `5` |
| `cbOpenDurationMs` | Circuit-open duration in milliseconds | `30000` |
| `httpAddress`, `httpPort` | REST API bind address and port | `0.0.0.0`, `8080` |
| `httpThreads` | HTTP worker threads | `4` |

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

The endpoint returns `202 Accepted` after the notification is persisted and published.

Supported values:

- `channel`: `EMAIL`, `SMS`, or `PUSH`
- `priority`: `LOW`, `NORMAL`, or `HIGH`

An optional `idempotencyKey` can be supplied to safely retry a submission:

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

### List recent notifications

```bash
curl http://localhost:8080/api/notifications
```

The list endpoint returns up to 50 recent notifications:

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

The detail endpoint includes `deliveryAttempts`, including attempt number, status, provider reference, and error message when available.

## Reliability

1. The API writes the notification and outbox event in one MySQL transaction.
2. The API publishes a message to the appropriate RabbitMQ channel route.
3. A channel worker consumes the message and calls the provider adapter.
4. Every attempt is recorded idempotently.
5. Transient failures move through the configured retry queues.
6. Messages that exhaust retries move to the channel dead-letter queue.
7. Circuit breakers pause delivery for a channel after repeated provider failures.

The current reference build publishes directly after the database commit. An outbox poller is documented as a production follow-up to recover from a failure between the database commit and RabbitMQ publish.

## Testing and Validation

Build the backend:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Build the frontend:

```bash
cd frontend
npm install
npm run build
```

Smoke-test the running API:

```bash
curl http://localhost:8080/healthz
curl http://localhost:8080/api/notifications
```

## Production Considerations

Before deploying publicly:

- Replace mock providers with authenticated provider integrations.
- Add API authentication, authorization, rate limiting, and TLS.
- Move all credentials to environment variables or a secret manager.
- Add an outbox publisher or recovery poller.
- Use managed or separately monitored MySQL and RabbitMQ instances.
- Add structured logs, metrics, traces, backups, and alerting.
- Restrict network access to the REST API and infrastructure services.
- Pin dependency versions and run security checks in CI.
- Add automated tests for provider failures, retries, idempotency, and database errors.

GitHub stores the source code and can host the static frontend with GitHub Pages. It does not run the C++ backend, MySQL, or RabbitMQ. The backend needs a VPS, container platform, or managed cloud deployment.

## Contributing

1. Create a feature branch.
2. Make a focused change.
3. Build both the backend and frontend.
4. Update documentation when behavior changes.
5. Open a pull request with a clear description and validation steps.

Please avoid committing generated build directories, dependency folders, credentials, or local configuration files.

## License

No license has been selected for this repository yet. Add a `LICENSE` file before distributing or accepting external contributions.
#
