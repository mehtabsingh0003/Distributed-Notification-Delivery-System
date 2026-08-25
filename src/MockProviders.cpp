#include "notification/MockProviders.hpp"

#include <chrono>
#include <random>
#include <thread>

#include "notification/IdGenerator.hpp"

namespace notification {

namespace {

// ~10% transient failure, ~2% permanent failure, rest succeed.
ProviderResult simulate(const Notification& n, const std::string& prefix, int minLatencyMs,
                         int maxLatencyMs) {
    thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> latency(minLatencyMs, maxLatencyMs);
    std::uniform_real_distribution<double> chance(0.0, 1.0);

    std::this_thread::sleep_for(std::chrono::milliseconds(latency(rng)));

    double roll = chance(rng);
    if (roll < 0.02) {
        throw ProviderException(prefix + " provider rejected recipient " + n.recipient, /*transient=*/false);
    }
    if (roll < 0.12) {
        throw ProviderException(prefix + " provider timed out", /*transient=*/true);
    }

    ProviderResult result;
    result.providerRef = prefix + "-" + IdGenerator::uuid4();
    return result;
}

}  // namespace

ProviderResult MockEmailProvider::send(const Notification& n) { return simulate(n, "email", 20, 80); }
ProviderResult MockSmsProvider::send(const Notification& n) { return simulate(n, "sms", 10, 50); }
ProviderResult MockPushProvider::send(const Notification& n) { return simulate(n, "push", 5, 30); }

}  // namespace notification
