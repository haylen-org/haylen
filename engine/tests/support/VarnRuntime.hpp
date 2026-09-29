#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <thread>

#include "varn/runtime/Runtime.h"

namespace haylen::test {

// Owns a Varn runtime for tests and pumps its event loop the way the engine does once per frame.
class VarnRuntime final {
  public:
    VarnRuntime() : runtime(std::make_unique<varn::runtime::Runtime>(std::vector<std::string>{"haylen_tests"}, 0)) {}

    [[nodiscard]] varn::runtime::Runtime& getRuntime() noexcept {
        return *runtime;
    }

    bool pumpUntil(const std::function<bool()>& condition, std::chrono::milliseconds timeout = std::chrono::seconds(10)) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!condition()) {
            if (std::chrono::steady_clock::now() > deadline) {
                return false;
            }
            runtime->poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }

  private:
    std::unique_ptr<varn::runtime::Runtime> runtime;
};

} // namespace haylen::test
