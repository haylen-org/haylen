#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "haylen/core/EnumNames.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::nodes3d {

class ComponentPool3D;
class NodeTree3D;

// A kind of component that the nodes of 3D trees hold, such as a body or an agent. A type describes its properties for inspectors and for saving, names the phases of the frame it runs in, and makes for every tree the pool that keeps the components of that tree in arrays, so one call runs every component of the type in a phase. Built-in types and the types of plugins register alike, in the registry of `Nodes3DPlugin`.
class ComponentType3D {
  public:
    // The phases of the frame that the 3D nodes plugin runs: with the fixed steps, before the scenes update and after them.
    enum class Phase : std::uint8_t {
        FixedUpdate,
        Update,
        LateUpdate,
    };

    // The kind of value of a property, as it reaches JSON and Lua.
    enum class Kind : std::uint8_t {
        Boolean,
        Number,
        Integer,
        String,
        Vec3,
        Quat,
        Json,
    };

    struct Property {
        std::string name{};
        Kind kind = Kind::Number;
        core::Json defaultValue{};
        std::string description{};
    };

    ComponentType3D() = default;
    virtual ~ComponentType3D() = default;

    ComponentType3D(const ComponentType3D&) = delete;
    ComponentType3D& operator=(const ComponentType3D&) = delete;

    [[nodiscard]] virtual std::string_view getName() const noexcept = 0;
    [[nodiscard]] virtual std::span<const Phase> getPhases() const noexcept = 0;
    [[nodiscard]] virtual std::span<const Property> getProperties() const noexcept = 0;

    // Makes the pool that keeps the components of this type in the tree, which lives as long as the tree.
    [[nodiscard]] virtual std::unique_ptr<ComponentPool3D> createPool(NodeTree3D& tree) const = 0;

    [[nodiscard]] bool runsIn(Phase phase) const noexcept;

    // Returns the property with the name, or null.
    [[nodiscard]] const Property* findProperty(std::string_view name) const noexcept;
};

} // namespace haylen::nodes3d

namespace haylen::core {

template <> struct EnumNames<haylen::nodes3d::ComponentType3D::Phase> {
    using Value = haylen::nodes3d::ComponentType3D::Phase;

    static constexpr std::array<std::pair<std::string_view, Value>, 3> kNames{{{"fixedUpdate", Value::FixedUpdate}, {"update", Value::Update}, {"lateUpdate", Value::LateUpdate}}};

    static std::optional<Value> fromName(std::string_view name) {
        const auto found = std::ranges::find(kNames, name, &std::pair<std::string_view, Value>::first);
        return found != kNames.end() ? std::optional(found->second) : std::nullopt;
    }
    static std::string_view name(Value value) {
        return std::ranges::find(kNames, value, &std::pair<std::string_view, Value>::second)->first;
    }
};

template <> struct EnumNames<haylen::nodes3d::ComponentType3D::Kind> {
    using Value = haylen::nodes3d::ComponentType3D::Kind;

    static constexpr std::array<std::pair<std::string_view, Value>, 7> kNames{{{"boolean", Value::Boolean}, {"number", Value::Number}, {"integer", Value::Integer}, {"string", Value::String}, {"vec3", Value::Vec3}, {"quat", Value::Quat}, {"json", Value::Json}}};

    static std::optional<Value> fromName(std::string_view name) {
        const auto found = std::ranges::find(kNames, name, &std::pair<std::string_view, Value>::first);
        return found != kNames.end() ? std::optional(found->second) : std::nullopt;
    }
    static std::string_view name(Value value) {
        return std::ranges::find(kNames, value, &std::pair<std::string_view, Value>::second)->first;
    }
};

} // namespace haylen::core
