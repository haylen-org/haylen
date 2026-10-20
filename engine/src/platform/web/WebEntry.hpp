#pragma once

#include <exception>

#include "haylen/core/Log.hpp"

namespace haylen::platform {

// Runs the body of a function that the page calls, so no C++ exception crosses into JavaScript: a failure becomes an engine error that names the function, and the page goes on. The entry points that answer a number report their failures to the page through `WebPage::answer` instead.
class WebEntry final {
  public:
    template <typename Body> static void guard(const char* name, Body&& body) noexcept {
        try {
            body();
        } catch (const std::exception& error) {
            core::Log::error("The page call \"{}\" failed: {}", name, error.what());
        }
    }
};

} // namespace haylen::platform
