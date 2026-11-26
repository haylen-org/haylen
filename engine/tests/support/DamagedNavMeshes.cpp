#include "support/DamagedNavMeshes.hpp"

#include <stdexcept>

#include "haylen/3d/navigation/NavMesh.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::test {

std::vector<std::uint8_t> DamagedNavMeshes::bake(bool obstacles) {
    navigation3d::SourceGeometry source;
    const std::vector<math::Vec3> corners{{-10.0F, 0.0F, -10.0F}, {-10.0F, 0.0F, 10.0F}, {10.0F, 0.0F, 10.0F}, {10.0F, 0.0F, -10.0F}};
    const std::vector<std::uint32_t> indices{0, 1, 2, 0, 2, 3};
    source.addMesh(corners, indices);
    source.addBox({.center = {0.0F, 1.5F, -5.0F}, .rotation = math::Quat::identity(), .halfExtents = {0.25F, 1.5F, 4.0F}});
    source.addVolume({.outline = {{-10.0F, 0.0F, 6.0F}, {-10.0F, 0.0F, 8.0F}, {10.0F, 0.0F, 8.0F}, {10.0F, 0.0F, 6.0F}}, .bottom = -1.0F, .top = 1.0F, .area = "road"});
    source.addLink({.start = {-1.0F, 0.0F, -5.0F}, .end = {1.0F, 0.0F, -5.0F}});

    navigation3d::BuildSettings settings;
    settings.tileSize = 6.0F;
    settings.obstacles = obstacles;
    return navigation3d::NavMesh::build(source, settings).encode();
}

const std::vector<std::uint8_t>& DamagedNavMeshes::getRoom(bool obstacles) {
    static const std::vector<std::uint8_t>& plain = *new std::vector<std::uint8_t>(bake(false));
    static const std::vector<std::uint8_t>& carved = *new std::vector<std::uint8_t>(bake(true));
    return obstacles ? carved : plain;
}

std::vector<std::uint8_t> DamagedNavMeshes::damage(std::span<const std::uint8_t> input) {
    if (input.size() < 4 || (input[0] & 1U) != 0) {
        return input.empty() ? std::vector<std::uint8_t>{} : std::vector<std::uint8_t>(input.begin() + 1, input.end());
    }

    std::vector<std::uint8_t> room = getRoom((input[1] & 1U) != 0);
    const std::size_t offset = (static_cast<std::size_t>(input[2]) | (static_cast<std::size_t>(input[3]) << 8U)) % room.size();
    for (std::size_t index = 4; index < input.size(); ++index) {
        room[(offset + index - 4) % room.size()] ^= input[index];
    }

    return room;
}

void DamagedNavMeshes::read(std::span<const std::uint8_t> bytes) {
    try {
        const navigation3d::NavMesh mesh = navigation3d::NavMesh::decode(bytes, "fuzz.navmesh");
        math::Random random(1);
        static_cast<void>(mesh.findPath({-8.0F, 0.0F, 4.0F}, {8.0F, 0.0F, 4.0F}));
        static_cast<void>(mesh.raycast({-8.0F, 0.0F, 4.0F}, {8.0F, 0.0F, 4.0F}));
        static_cast<void>(mesh.findRandomPoint(random));
        static_cast<void>(mesh.findRandomPointAround({0.0F, 0.0F, 0.0F}, 3.0F, random));
        static_cast<void>(mesh.getTriangles());
    } catch (const std::runtime_error&) {}
}

} // namespace haylen::test
