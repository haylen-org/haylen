#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>

#include "3d/physics/jolt/Backend.hpp"
#include "haylen/3d/physics/Shape.hpp"

namespace haylen::physics3d {

// Turns the bytes of an input into the data of a convex hull, a triangle mesh or a heightfield, by the first byte, so the shapes that take data of any size meet every value a file or a script could hand them.
class PhysicsShapeFuzzer final {
  public:
    static void run(std::span<const std::uint8_t> input) {
        if (input.empty()) {
            return;
        }

        const std::uint8_t kind = input.front();
        const std::span<const std::uint8_t> body = input.subspan(1);
        Shape::Settings settings;
        switch (kind % 3) {
        case 0:
            settings.geometry = Shape::ConvexHull{.points = readPoints(body)};
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
        } catch (const std::invalid_argument&) {}
    }

  private:
    // The first half of the floats are positions and the rest are indices, so meshes of every size and every index meet the checks.
    [[nodiscard]] static Shape::Mesh readMesh(std::span<const std::uint8_t> body) {
        const std::vector<float> values = readFloats(body);
        const std::size_t half = values.size() / 2;
        Shape::Mesh mesh;
        mesh.positions.assign(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(half));
        const std::span<const std::uint8_t> rest = body.subspan(std::min(body.size(), half * sizeof(float)));
        for (std::size_t offset = 0; offset + sizeof(std::uint32_t) <= rest.size(); offset += sizeof(std::uint32_t)) {
            std::uint32_t index = 0;
            std::memcpy(&index, rest.data() + offset, sizeof(index));
            mesh.indices.push_back(index);
        }

        return mesh;
    }

    // Every float after the first byte is a height. An even first byte makes the largest grid that the heights fill, and an odd one names the side of the grid itself, so both grids that fill and grids that do not meet the checks.
    [[nodiscard]] static Shape::HeightField readHeightField(std::span<const std::uint8_t> body) {
        Shape::HeightField field;
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

    [[nodiscard]] static std::vector<math::Vec3> readPoints(std::span<const std::uint8_t> body) {
        const std::vector<float> values = readFloats(body);
        std::vector<math::Vec3> points;
        for (std::size_t base = 0; base + 3 <= values.size(); base += 3) {
            points.push_back({values[base], values[base + 1], values[base + 2]});
        }

        return points;
    }

    [[nodiscard]] static std::vector<float> readFloats(std::span<const std::uint8_t> body) {
        std::vector<float> values(body.size() / sizeof(float));
        if (!values.empty()) {
            std::memcpy(values.data(), body.data(), values.size() * sizeof(float));
        }

        return values;
    }

    static inline jolt::Backend backend;
};

} // namespace haylen::physics3d

// The entry point libFuzzer calls with each input it generates, which must either make a shape or fail with the sentence of data that makes none.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    haylen::physics3d::PhysicsShapeFuzzer::run(std::span(data, size));
    return 0;
}
