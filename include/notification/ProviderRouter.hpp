#pragma once

#include <memory>
#include <unordered_map>

#include "notification/CircuitBreaker.hpp"
#include "notification/Config.hpp"
#include "notification/Provider.hpp"

namespace notification {

// Owns one NotificationProvider + one CircuitBreaker per channel and
// dispatches send() to the right pair. Shared across all worker threads
// for a channel (the breaker's whole job is to aggregate failures *across*
// threads, so it must not be per-worker).
class ProviderRouter {
public:
    explicit ProviderRouter(const Config& config);

    // Throws ProviderException(transient=true) if the breaker for this
    // channel is open, so callers can treat "circuit open" identically to
    // any other transient provider failure in their retry logic.
    ProviderResult send(const Notification& n);

private:
    struct Entry {
        std::unique_ptr<NotificationProvider> provider;
        std::unique_ptr<CircuitBreaker> breaker;
    };

    std::unordered_map<int, Entry> entries_;  // keyed by static_cast<int>(Channel)
};

}  // namespace notification
