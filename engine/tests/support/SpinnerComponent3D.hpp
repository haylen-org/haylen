#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/3d/nodes/ComponentPool3D.hpp"
#include "haylen/3d/nodes/ComponentType3D.hpp"
#include "haylen/3d/nodes/Node3D.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::test {

// A component type as a plugin writes one, for the tests of components: it turns its node around an axis at a speed in radians per second, in the update phase, for every component in one call.
class SpinnerComponent3D final : public nodes3d::ComponentType3D {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "spinner";
    }
    [[nodiscard]] std::span<const Phase> getPhases() const noexcept override {
        return kPhases;
    }
    [[nodiscard]] std::span<const Property> getProperties() const noexcept override {
        return properties;
    }
    [[nodiscard]] std::unique_ptr<nodes3d::ComponentPool3D> createPool(nodes3d::NodeTree3D&) const override {
        return std::make_unique<Pool>();
    }

  private:
    class Pool final : public nodes3d::ComponentPool3D {
      public:
        void add(const nodes3d::Node3D&, const core::Json& options) override {
            float speed = 1.0F;
            math::Vec3 axis = math::Vec3::up();
            for (const auto& [key, value] : options.items()) {
                if (key == "speed" && value.is_number()) {
                    speed = value.get<float>();
                } else if (key == "axis" && value.is_array() && value.size() == 3) {
                    axis = {value[0].get<float>(), value[1].get<float>(), value[2].get<float>()};
                } else {
                    throw std::invalid_argument("A spinner takes a number \"speed\" and a vector \"axis\".");
                }
            }

            speeds.push_back(speed);
            axes.push_back(axis);
        }
        void remove(std::size_t index) override {
            speeds[index] = speeds.back();
            axes[index] = axes.back();
            speeds.pop_back();
            axes.pop_back();
        }
        [[nodiscard]] core::Json get(std::size_t index, std::string_view property) const override {
            return save(index).at(std::string(property));
        }
        void set(std::size_t index, std::string_view property, const core::Json& value) override {
            if (property != "speed" || !value.is_number()) {
                throw std::invalid_argument("A spinner sets only its number \"speed\".");
            }

            speeds[index] = value.get<float>();
        }
        [[nodiscard]] core::Json save(std::size_t index) const override {
            return {{"speed", speeds[index]}, {"axis", {axes[index].x, axes[index].y, axes[index].z}}};
        }
        void run(Phase, const Batch& batch) override {
            for (std::size_t index = 0; index < batch.nodes.size(); ++index) {
                if (batch.active[index] == 0) {
                    continue;
                }

                nodes3d::Node3D node = batch.nodes[index];
                node.setRotation(math::Quat::fromAxisAngle(axes[index], speeds[index] * batch.deltaSeconds) * node.getRotation());
            }
        }

      private:
        std::vector<float> speeds;
        std::vector<math::Vec3> axes;
    };

    static constexpr std::array<Phase, 1> kPhases{Phase::Update};
    std::array<Property, 2> properties{Property{.name = "speed", .kind = Kind::Number, .defaultValue = 1.0F, .description = "Radians per second."}, Property{.name = "axis", .kind = Kind::Vec3, .defaultValue = {0.0F, 1.0F, 0.0F}, .description = "The axis it turns around."}};
};

} // namespace haylen::test
