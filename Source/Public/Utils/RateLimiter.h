#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Utils {

/**
 * Simple token bucket rate limiter.
 */
class RateLimiter {
public:
    RateLimiter(double tokensPerSecond, double maxBurst)
        : tokensPerSecond_(tokensPerSecond), maxBurst_(maxBurst), tokens_(maxBurst) {
        lastUpdate_ = std::chrono::steady_clock::now();
    }

    bool Allow(double tokens = 1.0) {
        std::lock_guard<std::mutex> lock(mutex_);
        UpdateTokens();
        
        if (tokens_ >= tokens) {
            tokens_ -= tokens;
            return true;
        }
        return false;
    }

    void SetRate(double tokensPerSecond, double maxBurst) {
        std::lock_guard<std::mutex> lock(mutex_);
        tokensPerSecond_ = tokensPerSecond;
        maxBurst_ = maxBurst;
        UpdateTokens();
    }

private:
    void UpdateTokens() {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = now - lastUpdate_;
        
        tokens_ += elapsed.count() * tokensPerSecond_;
        if (tokens_ > maxBurst_) {
            tokens_ = maxBurst_;
        }
        
        lastUpdate_ = now;
    }

    double tokensPerSecond_;
    double maxBurst_;
    double tokens_;
    std::chrono::steady_clock::time_point lastUpdate_;
    std::mutex mutex_;
};

} // namespace Utils
