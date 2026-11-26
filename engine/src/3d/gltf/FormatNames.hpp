#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/3d/gltf/Accessor.hpp"
#include "haylen/3d/gltf/Animation.hpp"
#include "haylen/3d/gltf/BufferView.hpp"
#include "haylen/3d/gltf/Camera.hpp"
#include "haylen/3d/gltf/Material.hpp"
#include "haylen/3d/gltf/Sampler.hpp"

namespace haylen::gltf {

// The values that the glTF format writes for its enums, the numbers of the graphics API it grew from and its own strings, and the bounds that the validation of the data takes from the official validator of glTF.
class FormatNames final {
  public:
    // How far a unit vector or a unit quaternion may be from a length of one, slightly more than the error that quantizing to signed bytes leaves.
    static constexpr double kUnitLengthThresholdVec3 = 0.00674;
    static constexpr double kUnitLengthThresholdVec4 = 0.00769;

    [[nodiscard]] static std::optional<Accessor::ComponentType> findComponentType(std::uint64_t value) noexcept;
    [[nodiscard]] static std::optional<Accessor::Type> findAccessorType(std::string_view name) noexcept;
    [[nodiscard]] static std::optional<BufferView::Target> findTarget(std::uint64_t value) noexcept;
    [[nodiscard]] static std::optional<Sampler::Filter> findFilter(std::uint64_t value) noexcept;
    [[nodiscard]] static std::optional<Sampler::Wrap> findWrap(std::uint64_t value) noexcept;
    [[nodiscard]] static std::optional<Material::AlphaMode> findAlphaMode(std::string_view name) noexcept;
    [[nodiscard]] static std::optional<Camera::Type> findCameraType(std::string_view name) noexcept;
    [[nodiscard]] static std::optional<Animation::Interpolation> findInterpolation(std::string_view name) noexcept;
    [[nodiscard]] static std::optional<Animation::Path> findPath(std::string_view name) noexcept;

    // Names a type of accessor and a type of its components as the format writes them, such as `VEC3` of unsigned shorts, for messages.
    [[nodiscard]] static std::string describeFormat(Accessor::Type type, Accessor::ComponentType componentType, bool normalized);

    [[nodiscard]] static double getLowest(Accessor::ComponentType componentType) noexcept;
    [[nodiscard]] static double getHighest(Accessor::ComponentType componentType) noexcept;

    // Tells whether a file may hold an image of a media type: PNG and JPEG always, and the formats of an extension that the file uses.
    [[nodiscard]] static bool isImageTypeEnabled(std::string_view mimeType, const std::vector<std::string>& extensionsUsed);

  private:
    static const std::array<std::pair<std::uint64_t, Accessor::ComponentType>, 6> kComponentTypes;
    static const std::array<std::pair<std::string_view, Accessor::Type>, 7> kAccessorTypes;
    static const std::array<std::pair<std::uint64_t, Sampler::Filter>, 6> kFilters;
    static const std::array<std::pair<std::uint64_t, Sampler::Wrap>, 3> kWraps;
    static const std::array<std::pair<std::string_view, Material::AlphaMode>, 3> kAlphaModes;
    static const std::array<std::pair<std::string_view, Camera::Type>, 2> kCameraTypes;
    static const std::array<std::pair<std::string_view, Animation::Interpolation>, 3> kInterpolations;
    static const std::array<std::pair<std::string_view, Animation::Path>, 4> kPaths;
    static const std::array<std::pair<std::string_view, std::string_view>, 3> kImageExtensions;

    template <typename Key, typename Value, std::size_t Size> [[nodiscard]] static std::optional<Value> lookUp(const std::array<std::pair<Key, Value>, Size>& table, Key key) noexcept {
        for (const auto& [name, value] : table) {
            if (name == key) {
                return value;
            }
        }

        return std::nullopt;
    }
};

} // namespace haylen::gltf
