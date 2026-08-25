#pragma once

#include "notification/Provider.hpp"

namespace notification {

// Simulate real-world provider behaviour (SES/Twilio/FCM-like) for local
// development and load testing without external dependencies: a small
// random failure rate split between transient and permanent errors, plus
// artificial latency. Swap for a real SmtpEmailProvider / Twilio client /
// FCM client in production by implementing NotificationProvider.
class MockEmailProvider : public NotificationProvider {
public:
    Channel channel() const override { return Channel::Email; }
    ProviderResult send(const Notification& n) override;
};

class MockSmsProvider : public NotificationProvider {
public:
    Channel channel() const override { return Channel::Sms; }
    ProviderResult send(const Notification& n) override;
};

class MockPushProvider : public NotificationProvider {
public:
    Channel channel() const override { return Channel::Push; }
    ProviderResult send(const Notification& n) override;
};

}  // namespace notification
