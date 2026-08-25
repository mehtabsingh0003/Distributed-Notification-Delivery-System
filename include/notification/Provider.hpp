#pragma once

#include <stdexcept>
#include <string>

#include "notification/Models.hpp"

namespace notification {

// Distinguishes failures a retry can plausibly fix (rate limited, provider
// timeout, 5xx) from failures a retry never will (invalid recipient
// address, permanently rejected). The worker only walks the retry ladder
// for transient failures; permanent ones go straight to FAILED.
class ProviderException : public std::runtime_error {
public:
    ProviderException(std::string message, bool transient)
        : std::runtime_error(std::move(message)), transient_(transient) {}
    bool isTransient() const { return transient_; }

private:
    bool transient_;
};

struct ProviderResult {
    std::string providerRef;  // provider-assigned message id, for audit trail
};

class NotificationProvider {
public:
    virtual ~NotificationProvider() = default;
    virtual Channel channel() const = 0;
    // Throws ProviderException on failure.
    virtual ProviderResult send(const Notification& n) = 0;
};

}  // namespace notification
