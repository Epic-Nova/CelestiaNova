#pragma once

#include "Utils/RateLimiter.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <mutex>

namespace Utils {

/**
 * Manages multiple rate limiters for different identities (Users, IPs, etc.)
 */
class RateLimitManager {
public:
    RateLimitManager(double defaultRate, double defaultBurst)
        : defaultRate_(defaultRate), defaultBurst_(defaultBurst) {}

    bool Allow(const std::string& key, double tokens = 1.0) {
        std::lock_guard<std::mutex> lock(mapMutex_);
        
        auto it = limiters_.find(key);
        if (it == limiters_.end()) {
            it = limiters_.emplace(key, std::make_unique<RateLimiter>(defaultRate_, defaultBurst_)).first;
        }

        return it->second->Allow(tokens);
    }

    void SetDefaultRate(double rate, double burst) {
        std::lock_guard<std::mutex> lock(mapMutex_);
        defaultRate_ = rate;
        defaultBurst_ = burst;
    }

    void CleanupIdleLimiters(std::chrono::seconds maxIdleTime) {
        // Implementation for cleaning up old entries could go here
    }

private:
    double defaultRate_;
    double defaultBurst_;
    std::unordered_map<std::string, std::unique_ptr<RateLimiter>> limiters_;
    std::mutex mapMutex_;
};

} // namespace Utils
