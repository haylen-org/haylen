#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "haylen/3d/nodes/ComponentType3D.hpp"

namespace haylen::nodes3d {

// The component types that apps find by name, such as from Lua or a saved scene. The 3D nodes plugin holds the registry of the engine, where the built-in types and the types of plugins register.
class ComponentRegistry3D final {
  public:
    // Registers a type. Throws `std::invalid_argument` for a type without a name or with the name of a registered type.
    void add(std::shared_ptr<const ComponentType3D> type);

    // Returns the type with the name, or null.
    [[nodiscard]] std::shared_ptr<const ComponentType3D> find(std::string_view name) const;

    [[nodiscard]] const std::vector<std::shared_ptr<const ComponentType3D>>& getTypes() const noexcept {
        return types;
    }

  private:
    std::vector<std::shared_ptr<const ComponentType3D>> types;
};

} // namespace haylen::nodes3d
