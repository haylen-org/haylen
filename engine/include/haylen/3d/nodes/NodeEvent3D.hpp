#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/core/EnumNames.hpp"

namespace haylen::nodes3d {

// What a listener of one node hears. The listener receives the node itself, except for `ChildAdded` and `ChildRemoved`, which hand it the child that joined or left. A child leaves by a new parent or by its destruction, and `EnabledChanged` fires whenever the node turns on or off in the hierarchy, by its own flag, the flag of an ancestor or a new parent.
enum class NodeEvent3D : std::uint8_t {
    Destroyed,
    ParentChanged,
    ChildAdded,
    ChildRemoved,
    EnabledChanged,
};

} // namespace haylen::nodes3d

namespace haylen::core {

template <> struct EnumNames<haylen::nodes3d::NodeEvent3D> {
    using Value = haylen::nodes3d::NodeEvent3D;

    static constexpr std::array<std::pair<std::string_view, Value>, 5> kNames{{{"destroyed", Value::Destroyed}, {"parentChanged", Value::ParentChanged}, {"childAdded", Value::ChildAdded}, {"childRemoved", Value::ChildRemoved}, {"enabledChanged", Value::EnabledChanged}}};

    static std::optional<Value> fromName(std::string_view name) {
        const auto found = std::ranges::find(kNames, name, &std::pair<std::string_view, Value>::first);
        return found != kNames.end() ? std::optional(found->second) : std::nullopt;
    }
    static std::string_view name(Value value) {
        return std::ranges::find(kNames, value, &std::pair<std::string_view, Value>::second)->first;
    }
};

} // namespace haylen::core
