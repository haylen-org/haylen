#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <string_view>

#include "haylen/2d/tiled/Object.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::tiled {

class MapRenderer;

// Spawns entities from Tiled objects by their class. Objects whose class has no factory are left alone, so maps can also carry collision shapes and markers that the app reads another way.
class ObjectFactories final {
  public:
    using Factory = std::function<void(const Object& object, math::Vec2 position)>;

    // Registers the factory for one object class. Each class has at most one factory.
    void add(std::string type, Factory factory);
    void remove(std::string_view type);
    [[nodiscard]] bool has(std::string_view type) const noexcept;

    // Calls the factory of every object with a registered class in the named object layer, or in every object layer when the name is empty, and returns how many objects were spawned. The position is the object origin in world coordinates.
    std::size_t spawn(const MapRenderer& map, std::string_view layer = {}) const;

  private:
    std::map<std::string, Factory, std::less<>> factories;
};

} // namespace haylen::tiled
