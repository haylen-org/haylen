#include "3d/physics/jolt/Backend.hpp"

#include "3d/physics/jolt/Library.hpp"
#include "3d/physics/jolt/Simulation.hpp"

namespace haylen::physics3d::jolt {

std::unique_ptr<physics3d::Simulation> Backend::createSimulation(const WorldSettings& settings, core::JobSystem* jobs) {
    Library::initialize();
    return std::make_unique<Simulation>(settings, jobs);
}

} // namespace haylen::physics3d::jolt
