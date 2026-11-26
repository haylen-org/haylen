#pragma once

#include <memory>
#include <string_view>

#include "haylen/3d/physics/Backend.hpp"

namespace haylen::physics3d::jolt {

// The built-in backend of 3D physics, which simulates worlds with Jolt Physics. This header holds nothing of Jolt, so the rest of the engine reaches the backend without the headers of Jolt.
class Backend final : public physics3d::Backend {
  public:
    static constexpr std::string_view kName = "jolt";

    [[nodiscard]] std::string_view getName() const noexcept override {
        return kName;
    }
    [[nodiscard]] std::unique_ptr<physics3d::Simulation> createSimulation(const WorldSettings& settings, core::JobSystem* jobs) override;

  protected:
    [[nodiscard]] std::shared_ptr<const physics3d::Shape> buildShape(const physics3d::Shape::Settings& settings) const override;
};

} // namespace haylen::physics3d::jolt
