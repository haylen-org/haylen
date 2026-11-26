#include "support/PhysicsShapeInputs.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <stdexcept>

namespace haylen::test {

std::optional<std::string> PhysicsShapeInputs::build(std::span<const std::uint8_t> input) {
    if (input.empty()) {
        return std::nullopt;
    }

    const std::uint8_t kind = input.front();
    const std::span<const std::uint8_t> body = input.subspan(1);
    physics3d::Shape::Settings settings;
    switch (kind % 3) {
    case 0:
        settings.geometry = physics3d::Shape::ConvexHull{.points = readPoints(body)};
        break;
    case 1:
        settings.geometry = readMesh(body);
        break;
    default:
        settings.geometry = readHeightField(body);
        break;
    }

    try {
        static_cast<void>(backend.createShape(settings));
    } catch (const std::invalid_argument& error) {
        return error.what();
    }

    return std::nullopt;
}

physics3d::Shape::Mesh PhysicsShapeInputs::readMesh(std::span<const std::uint8_t> body) {
    const std::vector<float> values = readFloats(body);
    const std::size_t half = values.size() / 2;
    physics3d::Shape::Mesh mesh;
    mesh.positions.assign(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(half));
    const std::span<const std::uint8_t> rest = body.subspan(std::min(body.size(), half * sizeof(float)));
    for (std::size_t offset = 0; offset + sizeof(std::uint32_t) <= rest.size(); offset += sizeof(std::uint32_t)) {
        std::uint32_t index = 0;
        std::memcpy(&index, rest.data() + offset, sizeof(index));
        mesh.indices.push_back(index);
    }

    return mesh;
}

physics3d::Shape::HeightField PhysicsShapeInputs::readHeightField(std::span<const std::uint8_t> body) {
    physics3d::Shape::HeightField field;
    if (body.empty()) {
        return field;
    }

    field.heights = readFloats(body.subspan(1));
    int side = 0;
    while (static_cast<std::size_t>(side + 1) * static_cast<std::size_t>(side + 1) <= field.heights.size()) {
        ++side;
    }

    field.sampleCount = body.front() % 2 == 0 ? side : body.front();
    field.heights.resize(static_cast<std::size_t>(side) * static_cast<std::size_t>(side));
    return field;
}

std::vector<math::Vec3> PhysicsShapeInputs::readPoints(std::span<const std::uint8_t> body) {
    const std::vector<float> values = readFloats(body);
    std::vector<math::Vec3> points;
    for (std::size_t base = 0; base + 3 <= values.size(); base += 3) {
        points.push_back({values[base], values[base + 1], values[base + 2]});
    }

    return points;
}

std::vector<float> PhysicsShapeInputs::readFloats(std::span<const std::uint8_t> body) {
    std::vector<float> values(body.size() / sizeof(float));
    if (!values.empty()) {
        std::memcpy(values.data(), body.data(), values.size() * sizeof(float));
    }

    return values;
}

} // namespace haylen::test
