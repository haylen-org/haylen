#pragma once

#include <string>

#include "haylen/core/Json.hpp"

namespace haylen::ui {

// An event a component reported, such as a click, with the id of its node and its values.
struct Event {
    std::string id;
    std::string name;
    core::Json value = core::Json::object();
};

} // namespace haylen::ui
