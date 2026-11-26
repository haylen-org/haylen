#pragma once

#include <array>
#include <string_view>

#include "haylen/core/Json.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::physics3d {

// The settings of a 3D physics world, in meters, seconds and kilograms. The `physics3d` section of `haylen.json` gives the defaults of every world of the app, and each world may change them when it is made.
// A step advances the world by the time it receives in `collisionSteps` equal parts. Each part solves velocities `velocitySteps` times and positions `positionSteps` times. A body sleeps once every point of it moves slower than `sleepVelocity` for `timeBeforeSleep` seconds. Contacts start within `speculativeContactDistance` of each other, the solver leaves `penetrationSlop` of overlap to keep piles steady, and a body bounces only when it hits faster than `restitutionThreshold`.
// The limits stay for the life of the world: the bodies it holds, the pairs of bodies whose bounds touch, the contacts it solves in one step, the threads that step it and the megabytes of memory that one step works in.
struct WorldSettings {
    math::Vec3 gravity{0.0F, -math::Math::kStandardGravity, 0.0F};
    int collisionSteps = 1;
    int velocitySteps = 10;
    int positionSteps = 2;
    bool sleepEnabled = true;
    float timeBeforeSleep = 0.5F;
    float sleepVelocity = 0.03F;
    float speculativeContactDistance = 0.02F;
    float penetrationSlop = 0.02F;
    float restitutionThreshold = 1.0F;
    int maxBodies = 10240;
    int maxBodyPairs = 65536;
    int maxContactConstraints = 20480;
    int threads = 1;
    int temporaryMemory = 16;

    // The most of each limit, far beyond what a device holds, which keeps every count within what the backends index.
    static constexpr int kMaxSteps = 64;
    static constexpr int kMaxThreads = 64;
    static constexpr int kMaxCount = (1 << 23) - 1;
    static constexpr int kMaxTemporaryMemory = 1024;

    // The keys of the settings, as `haylen.json` and the options of a world name them.
    static constexpr std::array<std::string_view, 15> kKeys{"gravity", "collisionSteps", "velocitySteps", "positionSteps", "sleepEnabled", "timeBeforeSleep", "sleepVelocity", "speculativeContactDistance", "penetrationSlop", "restitutionThreshold", "maxBodies", "maxBodyPairs", "maxContactConstraints", "threads", "temporaryMemory"};

    // Reads the `physics3d` section of `haylen.json` over these settings and checks them. Throws `std::invalid_argument` with a sentence that names the key as `haylen.json` writes it, such as `physics3d.velocitySteps`.
    void readSection(const core::Json& section);

    // Throws `std::invalid_argument` for a value out of its range, with a sentence that names the setting with the prefix and the place, such as `physics3d.` and ` in "haylen.json"`, or neither.
    void validate(std::string_view prefix = {}, std::string_view place = {}) const;

  private:
    template <typename T> static void readValue(const core::Json& section, std::string_view key, T& target);
    static void readGravity(const core::Json& section, math::Vec3& target);
    static void requireRange(std::string_view prefix, std::string_view place, std::string_view key, double value, double lowest, double highest);
};

} // namespace haylen::physics3d
