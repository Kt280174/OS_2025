#pragma once

namespace GameConfig {
    constexpr size_t MAX_WOLF_NUMBER = 100;
    constexpr size_t MIN_WOLF_NUMBER = 1;

    constexpr size_t MAX_ALIVE_GOAT_NUMBER = 100;
    constexpr size_t MAX_DEAD_GOAT_NUMBER = 50;
    constexpr size_t MIN_GOAT_NUMBER = 1;

    constexpr size_t ALIVE_GOAT_THRESHOLD = 70;
    constexpr size_t DEAD_GOAT_THRESHOLD = 20;

    constexpr size_t MAX_CONSECUTIVE_DEAD_TURNS = 2;
    constexpr size_t TIMEOUT_SECONDS = 5;
}