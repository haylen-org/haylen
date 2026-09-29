#pragma once

#include <string>

#include "haylen/core/Json.hpp"

namespace haylen::tiled {

// A custom property with its Tiled type. Colors keep their text form, files are resolved against the package content folder, and class values hold their members as JSON. List values keep the items Tiled writes, objects with a type, a value and an optional propertytype, with file items resolved too.
struct Property {
    std::string name;
    std::string type;
    std::string propertyType;
    core::Json value;
};

} // namespace haylen::tiled
