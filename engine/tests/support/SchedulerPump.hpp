#pragma once

#include <chrono>
#include <functional>
#include <thread>

#include "varn/runtime/Scheduler.h"

namespace haylen::test {

// Pumps the event loop of a scheduler of Varn for tests the way the engine does once per frame, until a condition holds.
class SchedulerPump final {
  public:
    SchedulerPump() = delete;

    static bool pumpUntil(varn::runtime::Scheduler& scheduler, const std::function<bool()>& condition, std::chrono::milliseconds timeout = std::chrono::seconds(10)) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!condition()) {
            if (std::chrono::steady_clock::now() > deadline) {
                return false;
            }

            scheduler.poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return true;
    }
};

} // namespace haylen::test
