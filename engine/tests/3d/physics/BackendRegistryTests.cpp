#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "3d/physics/jolt/Backend.hpp"
#include "haylen/3d/physics/Backend.hpp"
#include "haylen/3d/physics/BackendRegistry.hpp"
#include "haylen/3d/physics/Simulation.hpp"
#include "haylen/3d/physics/World.hpp"

namespace haylen::physics3d {

class BackendRegistryTest : public ::testing::Test {
  protected:
    // What a backend of a plugin saw of the worlds that it simulates.
    struct Record {
        int simulations = 0;
        int steps = 0;
        int configurations = 0;
        WorldSettings settings;
    };

    // A backend that a plugin registers, which records every call that reaches it.
    class RecordingBackend final : public Backend {
      public:
        RecordingBackend(std::string_view backendName, std::shared_ptr<Record> recorded) : name(backendName), record(std::move(recorded)) {}

        [[nodiscard]] std::string_view getName() const noexcept override {
            return name;
        }

        [[nodiscard]] std::unique_ptr<Simulation> createSimulation(const WorldSettings& settings, core::JobSystem*) override {
            ++record->simulations;
            record->settings = settings;
            return std::make_unique<RecordingSimulation>(record);
        }

      protected:
        [[nodiscard]] std::shared_ptr<const Shape> buildShape(const Shape::Settings&) const override {
            throw std::logic_error("The recording backend makes no shapes.");
        }

      private:
        class RecordingSimulation final : public Simulation {
          public:
            explicit RecordingSimulation(std::shared_ptr<Record> recorded) : record(std::move(recorded)) {}

            [[nodiscard]] StepReport step(float) override {
                ++record->steps;
                return {.bodyPairsFull = true};
            }
            void configure(const WorldSettings& settings) override {
                ++record->configurations;
                record->settings = settings;
            }
            [[nodiscard]] WorldStats getStats() const override {
                return {.bodies = record->steps};
            }
            [[nodiscard]] std::uint64_t computeStateHash() const override {
                return static_cast<std::uint64_t>(record->steps);
            }

            // The worlds of these tests hold no bodies, so every call about one is a defect of the world.
            [[nodiscard]] std::optional<BodyId> createBody(const Body::Options&) override {
                throw std::logic_error(kNoBodies);
            }
            void destroyBody(BodyId) override {
                throw std::logic_error(kNoBodies);
            }
            [[nodiscard]] bool isBodyValid(BodyId) const noexcept override {
                return false;
            }
            [[nodiscard]] std::size_t getBodySlot(BodyId) const noexcept override {
                return 0;
            }
            [[nodiscard]] Body::Type getBodyType(BodyId) const override {
                throw std::logic_error(kNoBodies);
            }
            void setBodyType(BodyId, Body::Type) override {
                throw std::logic_error(kNoBodies);
            }
            [[nodiscard]] Body::MotionQuality getMotionQuality(BodyId) const override {
                throw std::logic_error(kNoBodies);
            }
            void setMotionQuality(BodyId, Body::MotionQuality) override {
                throw std::logic_error(kNoBodies);
            }
            [[nodiscard]] Body::MotionLocks getMotionLocks(BodyId) const override {
                throw std::logic_error(kNoBodies);
            }
            void setMotionLocks(BodyId, const Body::MotionLocks&) override {
                throw std::logic_error(kNoBodies);
            }
            [[nodiscard]] Body::Transform getBodyTransform(BodyId) const override {
                throw std::logic_error(kNoBodies);
            }
            void setBodyTransform(BodyId, const Body::Transform&) override {
                throw std::logic_error(kNoBodies);
            }
            void moveBody(BodyId, const Body::Transform&, float) override {
                throw std::logic_error(kNoBodies);
            }
            void readBodyTransforms(std::span<const BodyId>, std::span<float>) const override {
                throw std::logic_error(kNoBodies);
            }
            void writeBodyTransforms(std::span<const BodyId>, std::span<const float>) override {
                throw std::logic_error(kNoBodies);
            }
            [[nodiscard]] math::Vec3 getBodyVector(BodyId, Body::Vector) const override {
                throw std::logic_error(kNoBodies);
            }
            void setBodyVector(BodyId, Body::Vector, math::Vec3) override {
                throw std::logic_error(kNoBodies);
            }
            [[nodiscard]] float getBodyValue(BodyId, Body::Value) const override {
                throw std::logic_error(kNoBodies);
            }
            void setBodyValue(BodyId, Body::Value, float) override {
                throw std::logic_error(kNoBodies);
            }
            [[nodiscard]] bool getBodyFlag(BodyId, Body::Flag) const override {
                throw std::logic_error(kNoBodies);
            }
            void setBodyFlag(BodyId, Body::Flag, bool) override {
                throw std::logic_error(kNoBodies);
            }
            [[nodiscard]] Body::CombineRule getCombineRule(BodyId, Body::Material) const override {
                throw std::logic_error(kNoBodies);
            }
            void setCombineRule(BodyId, Body::Material, Body::CombineRule) override {
                throw std::logic_error(kNoBodies);
            }
            void resetMass(BodyId) override {
                throw std::logic_error(kNoBodies);
            }
            [[nodiscard]] math::Vec3 getVelocityAt(BodyId, math::Vec3) const override {
                throw std::logic_error(kNoBodies);
            }
            void pushBody(BodyId, Body::Push, math::Vec3, std::optional<math::Vec3>) override {
                throw std::logic_error(kNoBodies);
            }
            [[nodiscard]] math::Box getBodyBounds(BodyId) const override {
                throw std::logic_error(kNoBodies);
            }
            [[nodiscard]] std::shared_ptr<const Shape> getBodyShape(BodyId) const override {
                throw std::logic_error(kNoBodies);
            }
            void setBodyShape(BodyId, const std::shared_ptr<const Shape>&) override {
                throw std::logic_error(kNoBodies);
            }
            [[nodiscard]] std::size_t getChildCount(BodyId) const override {
                throw std::logic_error(kNoBodies);
            }
            std::size_t addChild(BodyId, const Shape::Child&) override {
                throw std::logic_error(kNoBodies);
            }
            void removeChild(BodyId, std::size_t) override {
                throw std::logic_error(kNoBodies);
            }
            void setChildTransform(BodyId, std::size_t, const Body::Transform&) override {
                throw std::logic_error(kNoBodies);
            }
            void collectActiveBodies(std::vector<BodyId>& ids) const override {
                ids.clear();
            }
            [[nodiscard]] CollisionFilter getBodyFilter(BodyId) const override {
                throw std::logic_error(kNoBodies);
            }
            void setBodyFilter(BodyId, const CollisionFilter&) override {
                throw std::logic_error(kNoBodies);
            }

          private:
            static constexpr const char* kNoBodies = "The recording backend holds no bodies.";

            std::shared_ptr<Record> record;
        };

        std::string name;
        std::shared_ptr<Record> record;
    };

    std::shared_ptr<Record> record = std::make_shared<Record>();
};

TEST_F(BackendRegistryTest, SelectsTheFirstBackendUntilAnotherIsSelected) {
    BackendRegistry registry;
    EXPECT_THROW(static_cast<void>(registry.getSelected()), std::logic_error);

    registry.add(std::make_unique<jolt::Backend>());
    registry.add(std::make_unique<RecordingBackend>("acme", record));
    EXPECT_EQ(registry.getSelected().getName(), "jolt");
    EXPECT_EQ(registry.getNames(), (std::vector<std::string>{"acme", "jolt"}));

    registry.select("acme");
    EXPECT_EQ(registry.getSelected().getName(), "acme");
    EXPECT_TRUE(registry.contains("jolt"));
}

TEST_F(BackendRegistryTest, RefusesASecondBackendOfOneNameAndAnUnknownName) {
    BackendRegistry registry;
    registry.add(std::make_unique<jolt::Backend>());
    try {
        registry.add(std::make_unique<jolt::Backend>());
        FAIL() << "A second backend of one name was registered.";
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "A 3D physics backend named \"jolt\" is registered already.");
    }

    try {
        registry.select("acme");
        FAIL() << "An unknown backend was selected.";
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "No 3D physics backend is named \"acme\". The registered backends are \"jolt\".");
    }
}

// The world keeps the same contract with every backend: it checks the values before they reach the backend and reports what the backend found.
TEST_F(BackendRegistryTest, GivesTheBackendOfAPluginOnlyCheckedValues) {
    RecordingBackend backend("acme", record);
    World world(WorldSettings{.velocitySteps = 6}, backend);
    EXPECT_EQ(world.getBackendName(), "acme");
    EXPECT_EQ(record->simulations, 1);
    EXPECT_EQ(record->settings.velocitySteps, 6);

    EXPECT_THROW(world.setVelocitySteps(0), std::invalid_argument);
    EXPECT_THROW(world.step(-1.0F), std::invalid_argument);
    EXPECT_EQ(record->configurations, 0);
    EXPECT_EQ(record->steps, 0);

    world.setRestitutionThreshold(2.0F);
    world.step(1.0F / 60.0F);
    world.step(1.0F / 60.0F);
    EXPECT_EQ(record->configurations, 1);
    EXPECT_EQ(record->settings.restitutionThreshold, 2.0F);
    EXPECT_EQ(world.getStats().bodies, 2);
    EXPECT_EQ(world.computeStateHash(), 2U);

    world.destroy();
    EXPECT_THROW(world.step(1.0F / 60.0F), std::logic_error);
    EXPECT_EQ(record->steps, 2);
}

} // namespace haylen::physics3d
