#pragma once

#include <cstdint>

#include "haylen/2d/navigation/SteeringAgent.hpp"
#include "haylen/math/Random.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::navigation2d {

// Keeps the heading of a wandering agent between frames. The target circles a point ahead of the agent and drifts by a random amount each step, which gives smooth, unpredictable motion.
class Wanderer final {
  public:
    struct Settings {
        float distance = 60.0F;
        float radius = 30.0F;
        float jitter = 4.0F;
    };

    explicit Wanderer(std::uint64_t seed = 1, const Settings& value = kDefaultSettings);

    [[nodiscard]] math::Vec2 steer(const SteeringAgent& agent, float deltaSeconds) noexcept;

    [[nodiscard]] const Settings& getSettings() const noexcept {
        return settings;
    }
    void setSettings(const Settings& value) noexcept {
        settings = value;
    }

  private:
    static const Settings kDefaultSettings;

    math::Random random;
    Settings settings;
    float angle = 0.0F;
};

} // namespace haylen::navigation2d
