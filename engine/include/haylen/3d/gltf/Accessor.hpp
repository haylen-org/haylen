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

// A typed view into the data of a buffer view: `count` elements of one type, such as a 3D vector, whose components share one component type. An accessor without a buffer view holds zeros, and a sparse accessor replaces some of its elements with the values that its sparse part lists.
struct Accessor {
    enum class ComponentType : std::uint8_t {
        Byte,
        UnsignedByte,
        Short,
        UnsignedShort,
        UnsignedInt,
        Float,
    };

    enum class Type : std::uint8_t {
        Scalar,
        Vec2,
        Vec3,
        Vec4,
        Mat2,
        Mat3,
        Mat4,
    };

    // The elements that a sparse accessor replaces: their indices, strictly increasing, in one buffer view, and their values in another.
    struct Sparse {
        std::uint64_t count = 0;
        std::size_t indicesBufferView = 0;
        std::uint64_t indicesByteOffset = 0;
        ComponentType indicesComponentType = ComponentType::UnsignedInt;
        std::size_t valuesBufferView = 0;
        std::uint64_t valuesByteOffset = 0;
        core::Json extras;
        core::Json extensions;
    };

    std::string name;
    std::optional<std::size_t> bufferView;
    std::uint64_t byteOffset = 0;
    ComponentType componentType = ComponentType::Float;
    bool normalized = false;
    std::uint64_t count = 0;
    Type type = Type::Scalar;
    std::vector<double> min;
    std::vector<double> max;
    std::optional<Sparse> sparse;
    core::Json extras;
    core::Json extensions;

    [[nodiscard]] static constexpr std::size_t getComponentSize(ComponentType value) noexcept {
        switch (value) {
        case ComponentType::Byte:
        case ComponentType::UnsignedByte:
            return 1;
        case ComponentType::Short:
        case ComponentType::UnsignedShort:
            return 2;
        case ComponentType::UnsignedInt:
        case ComponentType::Float:
            return 4;
        }
        return 4;
    }

    [[nodiscard]] static constexpr std::size_t getComponentCount(Type value) noexcept {
        switch (value) {
        case Type::Scalar:
            return 1;
        case Type::Vec2:
            return 2;
        case Type::Vec3:
            return 3;
        case Type::Vec4:
        case Type::Mat2:
            return 4;
        case Type::Mat3:
            return 9;
        case Type::Mat4:
            return 16;
        }
        return 1;
    }

    // The rows of a column of a matrix type, or the components of a vector, which the columns of a matrix start every four bytes apart from.
    [[nodiscard]] static constexpr std::size_t getColumnRows(Type value) noexcept {
        switch (value) {
        case Type::Mat2:
            return 2;
        case Type::Mat3:
            return 3;
        case Type::Mat4:
            return 4;
        default:
            return getComponentCount(value);
        }
    }

    // The bytes one element takes in its buffer view, with the padding that keeps every column of a matrix on a boundary of four bytes.
    [[nodiscard]] static constexpr std::size_t getElementSize(ComponentType componentType, Type type) noexcept {
        const std::size_t componentSize = getComponentSize(componentType);
        const std::size_t rows = getColumnRows(type);
        const std::size_t columns = getComponentCount(type) / rows;
        if (columns == 1) {
            return componentSize * rows;
        }

        const std::size_t columnSize = (componentSize * rows + 3) / 4 * 4;
        return columnSize * columns;
    }

    [[nodiscard]] std::size_t getComponentCount() const noexcept {
        return getComponentCount(type);
    }
    [[nodiscard]] std::size_t getElementSize() const noexcept {
        return getElementSize(componentType, type);
    }
};

} // namespace haylen::gltf

namespace haylen::core {

template <> struct EnumNames<haylen::gltf::Accessor::ComponentType> {
    using Value = haylen::gltf::Accessor::ComponentType;

    static constexpr haylen::gltf::NameTable::Entries<Value, 6> kNames{{{"byte", Value::Byte}, {"unsignedByte", Value::UnsignedByte}, {"short", Value::Short}, {"unsignedShort", Value::UnsignedShort}, {"unsignedInt", Value::UnsignedInt}, {"float", Value::Float}}};

    static std::optional<Value> fromName(std::string_view name) {
        return haylen::gltf::NameTable::find(kNames, name);
    }
    static std::string_view name(Value value) {
        return haylen::gltf::NameTable::name(kNames, value);
    }
};

template <> struct EnumNames<haylen::gltf::Accessor::Type> {
    using Value = haylen::gltf::Accessor::Type;

    static constexpr haylen::gltf::NameTable::Entries<Value, 7> kNames{{{"scalar", Value::Scalar}, {"vec2", Value::Vec2}, {"vec3", Value::Vec3}, {"vec4", Value::Vec4}, {"mat2", Value::Mat2}, {"mat3", Value::Mat3}, {"mat4", Value::Mat4}}};

    static std::optional<Value> fromName(std::string_view name) {
        return haylen::gltf::NameTable::find(kNames, name);
    }
    static std::string_view name(Value value) {
        return haylen::gltf::NameTable::name(kNames, value);
    }
};

} // namespace haylen::core
