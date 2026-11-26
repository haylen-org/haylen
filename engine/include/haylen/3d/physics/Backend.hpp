#pragma once

#include <memory>
#include <string_view>

#include "haylen/3d/physics/Shape.hpp"
#include "haylen/3d/physics/Simulation.hpp"
#include "haylen/3d/physics/WorldSettings.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::physics3d {

// A physics engine that simulates the 3D worlds of the app, which `BackendRegistry` holds by name. The built-in backend `jolt` registers the same way a plugin registers one of its own.
class Backend {
  public:
    Backend() = default;
    virtual ~Backend() = default;

    Backend(const Backend&) = delete;
    Backend& operator=(const Backend&) = delete;

    [[nodiscard]] virtual std::string_view getName() const noexcept = 0;

    // Makes the simulation of a new world with settings that the world checked, which steps on the job system with the threads of the settings, or on the thread that steps it alone without a job system. Throws `std::runtime_error` with a sentence that says what is missing when the device cannot run the backend.
    [[nodiscard]] virtual std::unique_ptr<Simulation> createSimulation(const WorldSettings& settings, core::JobSystem* jobs) = 0;

    // Checks the settings and makes a shape that the worlds of this backend share. A backend makes shapes on any thread, since a shape belongs to no world, so heavy shapes such as meshes build on the task pool. Throws `std::invalid_argument` as `Shape::Settings::validate` does, and with a sentence that says what to change when the backend cannot hold the shape, such as a mesh too large for it.
    [[nodiscard]] std::shared_ptr<const Shape> createShape(const Shape::Settings& settings) const;

  protected:
    // Makes a shape from settings that `createShape` checked, on any thread.
    [[nodiscard]] virtual std::shared_ptr<const Shape> buildShape(const Shape::Settings& settings) const = 0;
};

} // namespace haylen::physics3d
