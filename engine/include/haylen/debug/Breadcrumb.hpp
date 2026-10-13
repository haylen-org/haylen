#pragma once

#include <string>

#include "haylen/core/Json.hpp"

namespace haylen::debug {

// A step of the app that the report of a crash and the services of plugins show, such as opening a screen, with the seconds since 1970 when it happened and the data of the app.
struct Breadcrumb {
    double time = 0.0;
    std::string message;
    core::Json data;
};

} // namespace haylen::debug
