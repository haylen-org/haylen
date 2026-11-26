#pragma once

#include <memory>
#include <string_view>
#include <utility>

#include "haylen/3d/nodes/ComponentType3D.hpp"
#include "haylen/3d/nodes/Node3D.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::nodes3d {

// Handle to the component of one type that a node holds. It reports `isValid()` while its node holds a component of its type, and every other method throws once the node is gone, with `std::logic_error`, or holds none, with `std::invalid_argument`.
class Component3D final {
  public:
    Component3D() = default;
    Component3D(Node3D owner, std::shared_ptr<const ComponentType3D> kind) noexcept : node(std::move(owner)), type(std::move(kind)) {}

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] const Node3D& getNode() const noexcept {
        return node;
    }
    [[nodiscard]] const std::shared_ptr<const ComponentType3D>& getType() const noexcept {
        return type;
    }

    [[nodiscard]] core::Json get(std::string_view property) const;
    void set(std::string_view property, const core::Json& value);
    [[nodiscard]] core::Json save() const;
    void remove();

    friend bool operator==(const Component3D& lhs, const Component3D& rhs) noexcept {
        return lhs.node == rhs.node && lhs.type == rhs.type;
    }

  private:
    Node3D node;
    std::shared_ptr<const ComponentType3D> type;
};

} // namespace haylen::nodes3d
