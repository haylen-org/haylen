#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "haylen/core/RingBuffer.hpp"

namespace haylen::debug {

// A value the app samples once per frame, such as the number of enemies or the size of a queue, which the debug overlay shows with a graph of its recent history.
class Monitor final {
  public:
    // Returns the value of this frame, or nothing to skip the frame, such as when the Lua function behind the monitor went away with its owner.
    using Sampler = std::function<std::optional<double>()>;

    Monitor(std::string monitorName, Sampler source, std::size_t historySize = 240);

    void sample();

    [[nodiscard]] const std::string& getName() const noexcept {
        return name;
    }
    [[nodiscard]] double getValue() const noexcept {
        return value;
    }

    // The sampled values, oldest first.
    [[nodiscard]] std::vector<float> getHistory() const;

  private:
    std::string name;
    Sampler sampler;
    core::RingBuffer<float> history;
    double value = 0.0;
};

} // namespace haylen::debug
