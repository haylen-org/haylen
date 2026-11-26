#include "haylen/3d/physics/WorldSettings.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace haylen::physics3d {

template <typename T> void WorldSettings::readValue(const core::Json& section, std::string_view key, T& target) {
    const auto found = section.find(key);
    if (found == section.end()) {
        return;
    }

    const core::Json& value = *found;
    if constexpr (std::is_same_v<T, bool>) {
        if (!value.is_boolean()) {
            throw std::invalid_argument(std::format("The value of \"physics3d.{}\" in \"haylen.json\" has the wrong type.", key));
        }

        target = value.get<bool>();
    } else if constexpr (std::is_integral_v<T>) {
        if (!value.is_number_integer() || !std::in_range<T>(value.get<std::int64_t>())) {
            throw std::invalid_argument(std::format("The value of \"physics3d.{}\" in \"haylen.json\" has the wrong type.", key));
        }

        target = static_cast<T>(value.get<std::int64_t>());
    } else {
        if (!value.is_number() || !std::isfinite(value.get<double>()) || std::abs(value.get<double>()) > std::numeric_limits<T>::max()) {
            throw std::invalid_argument(std::format("The value of \"physics3d.{}\" in \"haylen.json\" has the wrong type.", key));
        }

        target = value.get<T>();
    }
}

void WorldSettings::readGravity(const core::Json& section, math::Vec3& target) {
    const auto found = section.find("gravity");
    if (found == section.end()) {
        return;
    }

    const core::Json& value = *found;
    const bool isVector = value.is_array() && value.size() == 3 && value[0].is_number() && value[1].is_number() && value[2].is_number();
    if (!isVector) {
        throw std::invalid_argument("The value of \"physics3d.gravity\" in \"haylen.json\" must be a list of three numbers, such as [0, -1.62, 0].");
    }

    target = {value[0].get<float>(), value[1].get<float>(), value[2].get<float>()};
}

void WorldSettings::readSection(const core::Json& section) {
    if (!section.is_object()) {
        throw std::invalid_argument("The \"physics3d\" section of \"haylen.json\" must be an object of settings, such as {\"velocitySteps\": 10}.");
    }

    for (const auto& [key, value] : section.items()) {
        if (std::ranges::find(kKeys, key) == kKeys.end()) {
            throw std::invalid_argument(std::format("Unknown key \"{}\" in the \"physics3d\" section of \"haylen.json\".", key));
        }
    }

    readGravity(section, gravity);
    readValue(section, "collisionSteps", collisionSteps);
    readValue(section, "velocitySteps", velocitySteps);
    readValue(section, "positionSteps", positionSteps);
    readValue(section, "sleepEnabled", sleepEnabled);
    readValue(section, "timeBeforeSleep", timeBeforeSleep);
    readValue(section, "sleepVelocity", sleepVelocity);
    readValue(section, "speculativeContactDistance", speculativeContactDistance);
    readValue(section, "penetrationSlop", penetrationSlop);
    readValue(section, "restitutionThreshold", restitutionThreshold);
    readValue(section, "maxBodies", maxBodies);
    readValue(section, "maxBodyPairs", maxBodyPairs);
    readValue(section, "maxContactConstraints", maxContactConstraints);
    readValue(section, "threads", threads);
    readValue(section, "temporaryMemory", temporaryMemory);
    validate("physics3d.", " in \"haylen.json\"");
}

void WorldSettings::requireRange(std::string_view prefix, std::string_view place, std::string_view key, double value, double lowest, double highest) {
    if (value < lowest || value > highest || std::isnan(value)) {
        throw std::invalid_argument(std::format("The value of \"{}{}\"{} must be from {:.10g} to {:.10g}.", prefix, key, place, lowest, highest));
    }
}

// Friction acts through the impulses of the velocity step before, so a world solves velocities at least twice in each step.
void WorldSettings::validate(std::string_view prefix, std::string_view place) const {
    const bool finiteGravity = std::isfinite(gravity.x) && std::isfinite(gravity.y) && std::isfinite(gravity.z);
    if (!finiteGravity) {
        throw std::invalid_argument(std::format("The value of \"{}gravity\"{} must be a vector of three finite numbers.", prefix, place));
    }

    constexpr double kLongest = 1.0e6;
    requireRange(prefix, place, "collisionSteps", collisionSteps, 1, kMaxSteps);
    requireRange(prefix, place, "velocitySteps", velocitySteps, 2, kMaxSteps);
    requireRange(prefix, place, "positionSteps", positionSteps, 0, kMaxSteps);
    requireRange(prefix, place, "timeBeforeSleep", timeBeforeSleep, 0, kLongest);
    requireRange(prefix, place, "sleepVelocity", sleepVelocity, 0, kLongest);
    requireRange(prefix, place, "speculativeContactDistance", speculativeContactDistance, 0, kLongest);
    requireRange(prefix, place, "penetrationSlop", penetrationSlop, 0, kLongest);
    requireRange(prefix, place, "restitutionThreshold", restitutionThreshold, 0, kLongest);
    requireRange(prefix, place, "maxBodies", maxBodies, 1, kMaxCount);
    requireRange(prefix, place, "maxBodyPairs", maxBodyPairs, 1, kMaxCount);
    requireRange(prefix, place, "maxContactConstraints", maxContactConstraints, 1, kMaxCount);
    requireRange(prefix, place, "threads", threads, 1, kMaxThreads);
    requireRange(prefix, place, "temporaryMemory", temporaryMemory, 1, kMaxTemporaryMemory);
}

} // namespace haylen::physics3d
