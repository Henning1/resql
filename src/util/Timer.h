#pragma once

#include <chrono>

/**
  * @brief Class to time executions.
  */
class Timer {
public:
    std::chrono::time_point<std::chrono::high_resolution_clock> start;

    /**
      * @brief Take timestamp at a position in the code
      */
    Timer() {
        start = std::chrono::high_resolution_clock::now();
    }

    /**
      * @brief Get milliseconds from timestamp to now
      */
    double get() {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        return diff.count() * 1000;
    }
};
