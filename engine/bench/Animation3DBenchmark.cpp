#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "haylen/3d/animation/PoseBatch3D.hpp"
#include "haylen/3d/animation/Skinning3D.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/math/Quat.hpp"

namespace haylen::bench {

// Times 3D animation on the CPU: a clip of 64 bones sampled into one buffer and into a pose, a crowd of 500 characters of 64 joints posed with their palettes on one thread and on the job system, and 10 thousand vertices of 4 influences skinned with matrices and with dual quaternions.
class Animation3DBenchmark final {
  public:
    static int run() {
        const std::shared_ptr<const animation3d::Skeleton3D> skeleton = buildSkeleton();
        const std::shared_ptr<const animation3d::Clip3D> clip = buildClip();
        auto skin = std::make_shared<const animation3d::Skin3D>(skeleton, buildJoints());
        std::printf("%-56s %12s %8s\n", "Scene", "Average ms", "Runs");

        std::vector<float> values(clip->getValueCount());
        std::vector<std::size_t> cursors(clip->getTracks().size());
        float time = 0.0F;
        // clang-format off
        measure("Sample a clip of 64 bones into one buffer", 2000, [&] {
            time = std::fmod(time + 1.0F / 60.0F, clip->getDuration());
            clip->sample(time, values, cursors);
        });
        // clang-format on

        const animation3d::PoseSampler3D sampler(clip, skeleton);
        animation3d::Pose3D pose(skeleton);
        std::vector<std::size_t> poseCursors(sampler.getBoundTrackCount());
        std::vector<math::Mat4> palette(skin->getJointCount());
        // clang-format off
        measure("Pose 64 bones and build their palette", 2000, [&] {
            time = std::fmod(time + 1.0F / 60.0F, clip->getDuration());
            sampler.sample(time, pose.getLocalTransforms(), poseCursors);
            pose.computeModelMatrices();
            skin->computePalette(pose, palette);
        });
        // clang-format on

        animation3d::PoseBatch3D crowd(skin);
        for (int character = 0; character < kCharacters; ++character) {
            (void)crowd.add(clip, {.time = static_cast<float>(character) * 0.013F, .speed = 0.8F + static_cast<float>(character % 5) * 0.1F});
        }

        core::JobSystem jobs([](const std::string& error) { std::fprintf(stderr, "%s\n", error.c_str()); });
        measure("Crowd of 500 characters of 64 joints, one thread", 100, [&crowd] { crowd.update(1.0F / 60.0F); });
        measure("Crowd of 500 characters of 64 joints, job system", 100, [&crowd, &jobs] { crowd.update(1.0F / 60.0F, &jobs); });

        const animation3d::SkinWeights3D weights = buildWeights();
        std::vector<math::Vec3> positions(kVertices);
        std::vector<math::Vec3> normals(kVertices, math::Vec3{0.0F, 0.0F, 1.0F});
        for (std::size_t vertex = 0; vertex < kVertices; ++vertex) {
            positions[vertex] = {static_cast<float>(vertex % 100) * 0.01F, static_cast<float>(vertex / 100) * 0.02F, 0.0F};
        }

        std::vector<math::Vec3> skinned(kVertices);
        std::vector<math::Vec3> skinnedNormals(kVertices);
        std::vector<math::DualQuat> dualQuats(skin->getJointCount());
        skin->computeDualQuatPalette(pose, dualQuats);
        const animation3d::Skinning3D::Vertices vertices{.positions = positions, .normals = normals, .skinnedPositions = skinned, .skinnedNormals = skinnedNormals};
        measure("Skin 10 thousand vertices with matrices", 200, [&] { animation3d::Skinning3D::applyLinear(weights, palette, vertices); });
        measure("Skin 10 thousand vertices with dual quaternions", 200, [&] { animation3d::Skinning3D::applyDualQuat(weights, dualQuats, vertices); });
        return 0;
    }

  private:
    static constexpr int kBones = 64;
    static constexpr int kCharacters = 500;
    static constexpr std::size_t kVertices = 10000;

    static void measure(const char* name, int runs, const std::function<void()>& work) {
        work();
        const auto start = std::chrono::steady_clock::now();
        for (int run = 0; run < runs; ++run) {
            work();
        }

        const double total = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        std::printf("%-56s %12.4f %8d\n", name, total / runs, runs);
    }

    // A spine of eight bones with seven limbs of eight bones each, the shape of a character of 64 joints.
    [[nodiscard]] static std::shared_ptr<const animation3d::Skeleton3D> buildSkeleton() {
        std::vector<animation3d::Skeleton3D::Bone> bones;
        for (int bone = 0; bone < kBones; ++bone) {
            const int chain = bone / 8;
            const int link = bone % 8;
            std::optional<std::size_t> parent;
            if (link > 0) {
                parent = static_cast<std::size_t>(bone - 1);
            } else if (chain > 0) {
                parent = static_cast<std::size_t>(chain - 1);
            }

            bones.push_back({.name = "Bone" + std::to_string(bone), .parent = parent, .rest = {.position = {0.0F, 0.1F, 0.0F}, .rotation = math::Quat::identity(), .scale = {1.0F, 1.0F, 1.0F}}});
        }

        return std::make_shared<const animation3d::Skeleton3D>(std::move(bones));
    }

    // A clip of two seconds with a rotation of every bone at 30 keys per second and a position of the root.
    [[nodiscard]] static std::shared_ptr<const animation3d::Clip3D> buildClip() {
        constexpr int kKeys = 61;
        std::vector<animation3d::Track3D> tracks;
        std::vector<float> times(kKeys);
        for (int key = 0; key < kKeys; ++key) {
            times[static_cast<std::size_t>(key)] = static_cast<float>(key) / 30.0F;
        }

        for (int bone = 0; bone < kBones; ++bone) {
            std::vector<float> values;
            for (int key = 0; key < kKeys; ++key) {
                const math::Quat rotation = math::Quat::fromAxisAngle({0.0F, 0.0F, 1.0F}, std::sin(static_cast<float>(key + bone) * 0.2F) * 0.5F);
                values.insert(values.end(), {rotation.x, rotation.y, rotation.z, rotation.w});
            }

            tracks.emplace_back(animation3d::Track3D::Description{.target = "Bone" + std::to_string(bone), .property = "rotation", .type = animation3d::Track3D::Type::Quat, .times = times, .values = std::move(values)});
        }

        std::vector<float> positions;
        for (int key = 0; key < kKeys; ++key) {
            positions.insert(positions.end(), {static_cast<float>(key) * 0.01F, 0.0F, 0.0F});
        }

        tracks.emplace_back(animation3d::Track3D::Description{.target = "Bone0", .property = "position", .type = animation3d::Track3D::Type::Vec3, .times = times, .values = std::move(positions)});
        return std::make_shared<const animation3d::Clip3D>(animation3d::Clip3D::Description{.name = "walk", .tracks = std::move(tracks)});
    }

    [[nodiscard]] static std::vector<animation3d::Skin3D::Joint> buildJoints() {
        std::vector<animation3d::Skin3D::Joint> joints;
        for (int bone = 0; bone < kBones; ++bone) {
            joints.push_back({.bone = static_cast<std::size_t>(bone)});
        }

        return joints;
    }

    // Each vertex follows four neighboring joints with weights that add up to one.
    [[nodiscard]] static animation3d::SkinWeights3D buildWeights() {
        std::vector<std::uint16_t> joints;
        std::vector<float> weights;
        for (std::size_t vertex = 0; vertex < kVertices; ++vertex) {
            const auto first = static_cast<std::uint16_t>(vertex % (kBones - 3));
            joints.insert(joints.end(), {first, static_cast<std::uint16_t>(first + 1), static_cast<std::uint16_t>(first + 2), static_cast<std::uint16_t>(first + 3)});
            weights.insert(weights.end(), {0.4F, 0.3F, 0.2F, 0.1F});
        }

        return animation3d::SkinWeights3D(4, std::move(joints), std::move(weights), kBones);
    }
};

} // namespace haylen::bench

int main() {
    return haylen::bench::Animation3DBenchmark::run();
}
