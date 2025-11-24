#pragma once

#include "game_config.h"
#include <string>
#include <semaphore.h>
#include <ctime>

class SemaphoreManager {
public:
    static const std::string HOST_SEMAPHORE_PREFIX;
    static const std::string CLIENT_SEMAPHORE_PREFIX;

    struct Message {
        bool goat_status;
        size_t goat_number;
    };

    static bool setup_timeout(struct timespec* timeout) {
        if (clock_gettime(CLOCK_REALTIME, timeout) == -1) {
            return false;
        }
        timeout->tv_sec += GameConfig::TIMEOUT_SECONDS;
        return true;
    }
};

// Definitions
const std::string SemaphoreManager::HOST_SEMAPHORE_PREFIX = "HostSemaphore";
const std::string SemaphoreManager::CLIENT_SEMAPHORE_PREFIX = "WolfSemaphore";