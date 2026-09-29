#include "haylen/2d/physics/Fluid.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "2d/physics/Box2DConverter.hpp"
#include "haylen/2d/physics/World.hpp"

namespace haylen::physics2d {

Fluid::Fluid(World& owner, const Options& settings) : world(owner), options(settings), particle{.density = settings.density, .friction = settings.friction, .restitution = settings.restitution, .filter = settings.filter} {
    if (settings.radius <= 0.0F || settings.smoothingRadius <= settings.radius) {
        throw std::invalid_argument("A fluid needs a positive particle radius and a larger smoothing radius.");
    }
    Box2DConverter::checkShapeOptions(particle);
}

Fluid::~Fluid() {
    clear();
}

bool Fluid::spawn(math::Vec2 position, math::Vec2 velocity) {
    if (bodies.size() >= options.maxParticles) {
        return false;
    }
    Body body = world.createBody({.position = position, .velocity = velocity, .fixedRotation = true});
    body.addCircle(options.radius, particle);
    bodies.push_back(body);
    return true;
}

std::size_t Fluid::fill(const math::Rect& area, math::Vec2 velocity) {
    const float spacing = options.radius * 2.0F;
    const auto columns = static_cast<int>(area.width / spacing);
    const auto rows = static_cast<int>(area.height / spacing);
    std::size_t added = 0;
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            if (!spawn(area.getMin() + math::Vec2{static_cast<float>(column) + 0.5F, static_cast<float>(row) + 0.5F} * spacing, velocity)) {
                return added;
            }
            ++added;
        }
    }
    return added;
}

void Fluid::remove(std::size_t index) {
    if (index >= bodies.size()) {
        throw std::out_of_range("The fluid has no such particle.");
    }
    bodies[index].destroy();
    bodies[index] = bodies.back();
    bodies.pop_back();
}

void Fluid::clear() {
    for (Body& body : bodies) {
        body.destroy();
    }
    bodies.clear();
}

std::size_t Fluid::size() {
    dropDestroyed();
    return bodies.size();
}

const std::vector<Body>& Fluid::getBodies() {
    dropDestroyed();
    return bodies;
}

void Fluid::dropDestroyed() {
    std::erase_if(bodies, [](const Body& body) { return !body.isValid(); });
}

std::uint64_t Fluid::cellKey(int column, int row) noexcept {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(column)) << 32U) | static_cast<std::uint32_t>(row);
}

// Sorts the particles by grid cell of the smoothing radius, then pairs each particle with the later ones in its own and the eight cells around it.
void Fluid::findPairs() {
    const float size = options.smoothingRadius;
    cells.clear();
    for (std::size_t index = 0; index < positions.size(); ++index) {
        cells.emplace_back(cellKey(static_cast<int>(std::floor(positions[index].x / size)), static_cast<int>(std::floor(positions[index].y / size))), static_cast<std::uint32_t>(index));
    }
    std::sort(cells.begin(), cells.end());

    pairs.clear();
    for (std::size_t first = 0; first < positions.size(); ++first) {
        const int column = static_cast<int>(std::floor(positions[first].x / size));
        const int row = static_cast<int>(std::floor(positions[first].y / size));
        for (int offsetY = -1; offsetY <= 1; ++offsetY) {
            for (int offsetX = -1; offsetX <= 1; ++offsetX) {
                const std::uint64_t key = cellKey(column + offsetX, row + offsetY);
                for (auto entry = std::lower_bound(cells.begin(), cells.end(), std::pair(key, std::uint32_t{0})); entry != cells.end() && entry->first == key; ++entry) {
                    const std::uint32_t second = entry->second;
                    if (second <= first) {
                        continue;
                    }
                    const math::Vec2 offset = positions[second] - positions[first];
                    const float distance = offset.getLength();
                    if (distance < size && distance > 0.0F) {
                        pairs.push_back({static_cast<std::uint32_t>(first), second, 1.0F - distance / size, offset / distance});
                    }
                }
            }
        }
    }
}

void Fluid::update(float deltaSeconds) {
    dropDestroyed();
    if (bodies.empty() || deltaSeconds <= 0.0F) {
        return;
    }

    positions.resize(bodies.size());
    velocities.resize(bodies.size());
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        positions[index] = bodies[index].getPosition();
        velocities[index] = bodies[index].getVelocity();
    }
    findPairs();

    densities.assign(bodies.size(), 0.0F);
    nearDensities.assign(bodies.size(), 0.0F);
    for (const Pair& pair : pairs) {
        const float squared = pair.weight * pair.weight;
        densities[pair.first] += squared;
        densities[pair.second] += squared;
        nearDensities[pair.first] += squared * pair.weight;
        nearDensities[pair.second] += squared * pair.weight;
    }

    // Relaxation displacements are measured in smoothing radii per 60 Hz frame and become velocity changes over the step.
    const float frames = deltaSeconds * 60.0F;
    const float toVelocity = options.smoothingRadius / deltaSeconds;
    for (const Pair& pair : pairs) {
        const float pressure = options.stiffness * ((densities[pair.first] + densities[pair.second]) * 0.5F - options.restDensity);
        const float nearPressure = options.nearStiffness * (nearDensities[pair.first] + nearDensities[pair.second]) * 0.5F;
        const float displacement = frames * frames * (pressure * pair.weight + nearPressure * pair.weight * pair.weight);
        const math::Vec2 push = pair.direction * (displacement * toVelocity * 0.5F);
        velocities[pair.first] -= push;
        velocities[pair.second] += push;

        // Viscosity only slows particles that approach each other.
        const float approach = math::Vec2::dot(velocities[pair.first] - velocities[pair.second], pair.direction);
        if (approach > 0.0F) {
            const math::Vec2 impulse = pair.direction * (std::min(1.0F, frames * pair.weight * options.viscosity) * approach * 0.5F);
            velocities[pair.first] -= impulse;
            velocities[pair.second] += impulse;
        }
    }

    for (std::size_t index = 0; index < bodies.size(); ++index) {
        bodies[index].setVelocity(velocities[index]);
    }
}

} // namespace haylen::physics2d
