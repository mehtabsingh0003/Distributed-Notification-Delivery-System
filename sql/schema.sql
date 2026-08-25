CREATE DATABASE IF NOT EXISTS notification_db
    CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE notification_db;

CREATE TABLE IF NOT EXISTS notifications (
    id                CHAR(36)      NOT NULL PRIMARY KEY,
    idempotency_key   VARCHAR(128)  NOT NULL,
    channel           ENUM('EMAIL','SMS','PUSH')            NOT NULL,
    priority          ENUM('LOW','NORMAL','HIGH')           NOT NULL DEFAULT 'NORMAL',
    recipient         VARCHAR(255)  NOT NULL,
    subject           VARCHAR(255)  NULL,
    body              TEXT          NOT NULL,
    status            ENUM('PENDING','PROCESSING','SENT','FAILED','DEAD_LETTERED','CANCELLED')
                                    NOT NULL DEFAULT 'PENDING',
    attempt_count     INT UNSIGNED  NOT NULL DEFAULT 0,
    created_at        TIMESTAMP     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at        TIMESTAMP     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

    UNIQUE KEY uq_notifications_idempotency_key (idempotency_key),
    KEY idx_notifications_status (status),
    KEY idx_notifications_channel (channel),
    KEY idx_notifications_created_at (created_at)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS delivery_attempts (
    id                BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    notification_id   CHAR(36)      NOT NULL,
    attempt_number    INT UNSIGNED  NOT NULL,
    channel           ENUM('EMAIL','SMS','PUSH') NOT NULL,
    status            ENUM('SUCCESS','TRANSIENT_FAILURE','PERMANENT_FAILURE') NOT NULL,
    provider_ref      VARCHAR(255)  NULL,
    error_message     VARCHAR(1024) NULL,
    attempted_at      TIMESTAMP     NOT NULL DEFAULT CURRENT_TIMESTAMP,

    UNIQUE KEY uq_attempt_per_notification (notification_id, attempt_number),
    KEY idx_delivery_attempts_notification (notification_id),
    CONSTRAINT fk_delivery_attempts_notification
        FOREIGN KEY (notification_id) REFERENCES notifications(id)
        ON DELETE CASCADE
) ENGINE=InnoDB;

-- Transactional outbox: written in the same DB transaction as the
-- notification row so "accepted" and "queued for publish" can never
-- disagree. A background OutboxPublisher thread polls PENDING rows,
-- publishes to RabbitMQ, and marks them PUBLISHED.
CREATE TABLE IF NOT EXISTS outbox_events (
    id                BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    notification_id   CHAR(36)      NOT NULL,
    event_type        VARCHAR(64)   NOT NULL,
    payload           TEXT          NOT NULL,
    status            ENUM('PENDING','PUBLISHED','FAILED') NOT NULL DEFAULT 'PENDING',
    created_at        TIMESTAMP     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    published_at      TIMESTAMP     NULL,

    KEY idx_outbox_status (status, created_at),
    CONSTRAINT fk_outbox_notification
        FOREIGN KEY (notification_id) REFERENCES notifications(id)
        ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE USER IF NOT EXISTS 'notification_app'@'%' IDENTIFIED BY 'change-me';
GRANT SELECT, INSERT, UPDATE, DELETE ON notification_db.* TO 'notification_app'@'%';
FLUSH PRIVILEGES;
