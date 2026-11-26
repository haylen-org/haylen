#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "haylen/3d/gltf/NameTable.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/math/Vec3.hpp"
#include "haylen/math/Vec4.hpp"

namespace haylen::gltf {

// The metallic-roughness material of glTF. Factors are linear, the base color and emissive textures are sRGB and the others linear. A default material is the material of the specification that a primitive without a material draws with.
struct Material {
    // How the alpha of the base color covers what lies behind: not at all, by the cutoff, or by blending.
    enum class AlphaMode : std::uint8_t {
        Opaque,
        Mask,
        Blend,
    };

    // A texture that a material reads, with the set of texture coordinates it reads at, `TEXCOORD_<texCoord>` of the primitive.
    struct TextureInfo {
        std::size_t index = 0;
        std::uint32_t texCoord = 0;
        core::Json extras;
        core::Json extensions;
    };

    std::string name;
    math::Vec4 baseColorFactor{1.0F, 1.0F, 1.0F, 1.0F};
    std::optional<TextureInfo> baseColorTexture;
    float metallicFactor = 1.0F;
    float roughnessFactor = 1.0F;

    // Roughness in the green channel and metalness in the blue channel.
    std::optional<TextureInfo> metallicRoughnessTexture;
    std::optional<TextureInfo> normalTexture;
    float normalScale = 1.0F;

    // Occlusion in the red channel, which `occlusionStrength` blends in.
    std::optional<TextureInfo> occlusionTexture;
    float occlusionStrength = 1.0F;
    std::optional<TextureInfo> emissiveTexture;
    math::Vec3 emissiveFactor{0.0F, 0.0F, 0.0F};
    AlphaMode alphaMode = AlphaMode::Opaque;

    // The alpha under which a masked material draws nothing, which only the mode `Mask` uses.
    float alphaCutoff = 0.5F;
    bool doubleSided = false;

    // The extras and extensions of the part `pbrMetallicRoughness`, apart from those of the material.
    core::Json pbrExtras;
    core::Json pbrExtensions;
    core::Json extras;
    core::Json extensions;
};

} // namespace haylen::gltf

namespace haylen::core {

template <> struct EnumNames<haylen::gltf::Material::AlphaMode> {
    using Value = haylen::gltf::Material::AlphaMode;

    static constexpr haylen::gltf::NameTable::Entries<Value, 3> kNames{{{"opaque", Value::Opaque}, {"mask", Value::Mask}, {"blend", Value::Blend}}};

    static std::optional<Value> fromName(std::string_view name) {
        return haylen::gltf::NameTable::find(kNames, name);
    }
    static std::string_view name(Value value) {
        return haylen::gltf::NameTable::name(kNames, value);
    }
};

} // namespace haylen::core
