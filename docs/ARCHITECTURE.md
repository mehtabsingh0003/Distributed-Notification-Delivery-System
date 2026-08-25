# Architecture

```
                  POST /api/notifications
                          |
                          v
                 +--------------------+
                 |   HttpServer       |  Boost.Beast, blocking handlers
                 |   (Beast REST API) |  on a small acceptor thread pool
                 +--------------------+
                          |
              MySQL tx: INSERT notifications
                       + INSERT outbox_events
                          |
                          v
                 +--------------------+
                 | RabbitMQPublisher  |---> notifications.exchange (topic)
                 +--------------------+          |         |         |
                                          notification. notification. notification.
                                             email          sms          push
                                                |             |            |
                                                v             v            v
                                            q.email        q.sms        q.push
                                                |             |            |
                                        +-------+--+  +-------+--+  +------+---+
                                        | Worker x4|  | Worker x4|  | Worker x4|
                                        +----------+  +----------+  +----------+
                                                |
                                     ProviderRouter -> CircuitBreaker -> Provider
                                                |
                                success -> SENT           transient fail, attempts
                                record attempt              remaining -> publish to
                                                            q.<channel>.retry.N (TTL,
                                                            DLX back to exchange)
                                                                    |
                                                        exhausted / permanent fail
                                                                    v
                                                            q.<channel>.dlq
                                                       mark DEAD_LETTERED / FAILED
```

## Why these specific choices

**Transactional outbox, not "write DB then publish".** If the process
crashed between a plain MySQL commit and the RabbitMQ publish, the
notification would be accepted (HTTP 202 already sent) but silently never
delivered. Writing an `outbox_events` row in the *same* transaction as the
`notifications` row means the fact "this needs to be published" is
durable. `NotificationRepository::insertWithOutbox` does this write; a
production deployment adds a poller (`OutboxPublisher`, not included in
this reference build) that scans `outbox_events WHERE status='PENDING'`
and republishes anything the API's direct publish failed to send.

**Retry via TTL + dead-letter-exchange ladder, not `nack` + requeue.**
RabbitMQ has no native "redeliver in N seconds". `nack`-with-requeue
retries *immediately*, which turns a struggling provider into a tight
retry storm across every worker thread. Each `q.<channel>.retry.N` queue
has a fixed `x-message-ttl` and a `x-dead-letter-exchange` pointing back
at the main topic exchange; messages sit there until the TTL expires, then
get redelivered to the main queue automatically. This is the standard
RabbitMQ delayed-retry pattern (see `RabbitMQTopology.hpp`).

**One AMQP connection per thread, never shared.** AMQP-CPP's connection
and channel objects are not thread-safe. Every worker thread and the
publisher used by the HTTP layer each own a dedicated
`boost::asio::io_context` + `AMQP::TcpConnection` running on their own
thread. This is more connections against RabbitMQ than a pooled-channel
design, but it's the only correct option given AMQP-CPP's threading model,
and RabbitMQ comfortably handles hundreds of connections.

**Idempotent attempt logging, not exactly-once delivery.** RabbitMQ (and
this system) guarantee at-least-once processing. A worker can send to the
provider, then crash before acking -- the message gets redelivered and
processed again. `delivery_attempts` has a `UNIQUE(notification_id,
attempt_number)` key and `record()` uses `INSERT ... ON DUPLICATE KEY
UPDATE`, so a redelivered attempt overwrites the same row instead of
creating a duplicate audit entry. Real providers should additionally use
their own idempotency key where available (Twilio, SES, FCM all support
this) to avoid double-sending to the end user.

**Circuit breaker is per-channel, shared across that channel's worker
threads.** A single struggling provider (SMTP relay down) shouldn't be
diagnosed independently by 4 worker threads each tripping their own
breaker after 5 failures -- that's 20 failed calls before anything opens.
`ProviderRouter` owns one `CircuitBreaker` per channel shared by all of
that channel's `WorkerThread`s.

## Known simplifications (documented, not hidden)

- The HTTP layer publishes to RabbitMQ directly after the DB commit
  instead of relying purely on an outbox poller — the poller is described
  above but not wired into `main.cpp`. If the direct publish fails, the
  notification is durably recorded as `PENDING` with a matching
  `outbox_events` row and can be recovered by a poller; this build does
  not include that recovery loop.
- Providers are mocked (random latency + ~10% transient / ~2% permanent
  failure) so the whole pipeline runs without external accounts. Swap in
  a real SMTP/Twilio/FCM client by implementing `NotificationProvider`.
- No API-key auth / rate limiting on the HTTP layer (the Java reference
  project has this; omitted here to keep the C++ surface focused on the
  messaging/DB reliability mechanics the resume bullet is about).
