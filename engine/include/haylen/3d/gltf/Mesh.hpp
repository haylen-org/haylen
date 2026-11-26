#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/3d/gltf/NameTable.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::gltf {

// Geometry that nodes place in a scene: a list of primitives that each draw with one material, with the default weights of its morph targets.
struct Mesh {
    // An attribute of the vertices, such as `POSITION` or `TEXCOORD_1`, and the accessor of its values.
    struct Attribute {
        std::string name;
        std::size_t accessor = 0;

        friend bool operator==(const Attribute&, const Attribute&) = default;
    };

    // One draw of a mesh: the vertices in attributes, the order that indices give them, how they join and the differences of each morph target.
    struct Primitive {
        enum class Mode : std::uint8_t {
            Points,
            Lines,
            LineLoop,
            LineStrip,
            Triangles,
            TriangleStrip,
            TriangleFan,
        };

        // The attributes in the order of their names.
        std::vector<Attribute> attributes;
        std::optional<std::size_t> indices;
        std::optional<std::size_t> material;
        Mode mode = Mode::Triangles;
        std::vector<std::vector<Attribute>> targets;
        core::Json extras;
        core::Json extensions;

        // Returns the accessor of an attribute, or nothing when the primitive lacks it.
        [[nodiscard]] std::optional<std::size_t> findAttribute(std::string_view attribute) const noexcept;
    };

    std::string name;
    std::vector<Primitive> primitives;
    std::vector<float> weights;
    core::Json extras;
    core::Json extensions;

    // The morph targets of the mesh, which every primitive that has targets holds as many of.
    [[nodiscard]] std::size_t getTargetCount() const noexcept;
};

} // namespace haylen::gltf

namespace haylen::core {

template <> struct EnumNames<haylen::gltf::Mesh::Primitive::Mode> {
    using Value = haylen::gltf::Mesh::Primitive::Mode;

    static constexpr haylen::gltf::NameTable::Entries<Value, 7> kNames{{{"points", Value::Points}, {"lines", Value::Lines}, {"lineLoop", Value::LineLoop}, {"lineStrip", Value::LineStrip}, {"triangles", Value::Triangles}, {"triangleStrip", Value::TriangleStrip}, {"triangleFan", Value::TriangleFan}}};

    static std::optional<Value> fromName(std::string_view name) {
        return haylen::gltf::NameTable::find(kNames, name);
    }
    static std::string_view name(Value value) {
        return haylen::gltf::NameTable::name(kNames, value);
    }
};

} // namespace haylen::core
