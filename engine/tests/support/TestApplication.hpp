#pragma once

#include <functional>
#include <utility>

#include "haylen/core/Application.hpp"

namespace haylen::test {

// Application whose start hook is supplied by the test.
class TestApplication final : public core::Application {
  public:
    explicit TestApplication(std::function<void(core::Engine&)> hook) : onStart(std::move(hook)) {}

    void start(core::Engine& engine) override {
        if (onStart) {
            onStart(engine);
        }
    }

  private:
    std::function<void(core::Engine&)> onStart;
};

} // namespace haylen::test
