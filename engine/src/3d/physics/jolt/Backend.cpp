#include "3d/physics/jolt/Backend.hpp"

#include "3d/physics/jolt/Library.hpp"
#include "3d/physics/jolt/Shape.hpp"
#include "3d/physics/jolt/ShapeBuilder.hpp"
#include "3d/physics/jolt/Simulation.hpp"

namespace haylen::physics3d::jolt {

std::unique_ptr<physics3d::Simulation> Backend::createSimulation(const WorldSettings& settings, core::JobSystem* jobs) {
    Library::initialize();
    return std::make_unique<Simulation>(settings, jobs);
}

std::shared_ptr<const physics3d::Shape> Backend::buildShape(const physics3d::Shape::Settings& settings) const {
    Library::initialize();
    return ShapeBuilder::build(settings);
}

} // namespace haylen::physics3d::jolt
