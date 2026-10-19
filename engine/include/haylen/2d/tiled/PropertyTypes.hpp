#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "haylen/2d/tiled/Properties.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::tiled {

// The custom classes of a Tiled project file, with their members and defaults. Tiled writes only the members of a class that differ from their defaults, both in class values and in the properties of a map, layer, tileset, tile, Wang set or object whose class it is, so a map read with the classes of its project fills in the members it leaves out.
class PropertyTypes final {
  public:
    struct Member {
        std::string name;
        std::string type;
        std::string propertyType;
        core::Json value;
    };

    struct Class {
        std::string name;
        std::vector<Member> members;
    };

    // Reads the `propertyTypes` of a `.tiled-project` file, whose enums need no defaults and are left out. Throws `std::invalid_argument` for a class without a name or a member without one.
    [[nodiscard]] static PropertyTypes parse(const core::Json& project);

    [[nodiscard]] const std::vector<Class>& getClasses() const noexcept {
        return classes;
    }
    [[nodiscard]] const Class* findClass(std::string_view name) const noexcept;

    // Returns the properties with the members of the class they lack at their defaults, after the properties they have, and with the class values among them and in their lists completed the same way. An unknown class adds nothing. Throws `std::invalid_argument` for classes that hold themselves deeper than `kMaxDepth`.
    [[nodiscard]] Properties complete(const Properties& properties, std::string_view className) const;

    static constexpr int kMaxDepth = 32;

  private:
    [[nodiscard]] core::Json completeValue(std::string_view className, core::Json value, int depth) const;
    [[nodiscard]] core::Json completeItems(core::Json items, int depth) const;

    std::vector<Class> classes;
};

} // namespace haylen::tiled
