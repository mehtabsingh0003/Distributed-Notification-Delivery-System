#pragma once

#include <atomic>
#include <chrono>
#include <mutex>

namespace notification {

// Classic three-state circuit breaker (Closed / Open / Half-Open) guarding
// calls to a single provider. Wraps provider calls so a struggling
// downstream (SMTP relay down, SMS gateway rate-limiting hard) doesn't get
// hammered by every worker thread retrying in a tight loop -- once the
// failure threshold trips, calls fail fast for `openDuration` before a
// single probe call is allowed through to test recovery.
class CircuitBreaker {
public:
    CircuitBreaker(int failureThreshold, std::chrono::milliseconds openDuration)
        : failureThreshold_(failureThreshold), openDuration_(openDuration) {}

    // Returns false if the breaker is open and the caller should fail fast
    // without invoking the provider at all.
    bool allowRequest() {
        std::lock_guard<std::mutex> lock(mu_);
        if (state_ == State::Closed) return true;

        if (state_ == State::Open) {
            auto elapsed = std::chrono::steady_clock::now() - openedAt_;
            if (elapsed >= openDuration_) {
                state_ = State::HalfOpen;
                return true;  // allow exactly one probe through
            }
            return false;
        }

        // HalfOpen: only one probe in flight at a time.
        if (probeInFlight_) return false;
        probeInFlight_ = true;
        return true;
    }

    void onSuccess() {
        std::lock_guard<std::mutex> lock(mu_);
        failureCount_ = 0;
        probeInFlight_ = false;
        state_ = State::Closed;
    }

    void onFailure() {
        std::lock_guard<std::mutex> lock(mu_);
        probeInFlight_ = false;
        if (state_ == State::HalfOpen) {
            trip();
            return;
        }
        if (++failureCount_ >= failureThreshold_) {
            trip();
        }
    }

    bool isOpen() {
        std::lock_guard<std::mutex> lock(mu_);
        return state_ == State::Open;
    }

private:
    enum class State { Closed, Open, HalfOpen };

    void trip() {
        state_ = State::Open;
        openedAt_ = std::chrono::steady_clock::now();
        failureCount_ = 0;
    }

    std::mutex mu_;
    State state_{State::Closed};
    int failureThreshold_;
    std::chrono::milliseconds openDuration_;
    int failureCount_{0};
    bool probeInFlight_{false};
    std::chrono::steady_clock::time_point openedAt_{};
};

}  // namespace notification
