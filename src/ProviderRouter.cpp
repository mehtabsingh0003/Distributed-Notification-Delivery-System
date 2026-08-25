#include "notification/ProviderRouter.hpp"

#include "notification/MockProviders.hpp"

namespace notification {

ProviderRouter::ProviderRouter(const Config& config) {
    auto makeBreaker = [&] {
        return std::make_unique<CircuitBreaker>(config.cbFailureThreshold,
                                                 std::chrono::milliseconds(config.cbOpenDurationMs));
    };

    entries_[static_cast<int>(Channel::Email)] = Entry{std::make_unique<MockEmailProvider>(), makeBreaker()};
    entries_[static_cast<int>(Channel::Sms)] = Entry{std::make_unique<MockSmsProvider>(), makeBreaker()};
    entries_[static_cast<int>(Channel::Push)] = Entry{std::make_unique<MockPushProvider>(), makeBreaker()};
}

ProviderResult ProviderRouter::send(const Notification& n) {
    auto& entry = entries_.at(static_cast<int>(n.channel));

    if (!entry.breaker->allowRequest()) {
        throw ProviderException("circuit breaker open for channel " + toString(n.channel),
                                 /*transient=*/true);
    }

    try {
        auto result = entry.provider->send(n);
        entry.breaker->onSuccess();
        return result;
    } catch (const ProviderException& ex) {
        if (ex.isTransient()) {
            entry.breaker->onFailure();
        }
        throw;
    }
}

}  // namespace notification
