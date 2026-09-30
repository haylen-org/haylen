#pragma once

#include <string>
#include <string_view>

#include "haylen/core/Json.hpp"

namespace haylen::io {
class Package;
}

namespace haylen::platform {

// A plugin that `app.json` lists: a folder of the package with its `plugin.json` and its Lua modules, and a native part that each platform may provide. It is a distributable plugin of the app, not one of the engine plugins of `haylen::plugins`.
struct AppPlugin {
    std::string id;
    std::string version;

    // The parameter values of `app.json` over the defaults that `plugin.json` declares.
    core::Json config = core::Json::object();

    // Whether the native part of the plugin runs on this platform.
    bool native = false;

    // Reads the version and the parameter defaults of `plugins/<id>/plugin.json` in the package and applies the values of `app.json` over them. Throws `std::runtime_error` when the manifest is missing, is not JSON or has no version.
    [[nodiscard]] static AppPlugin read(const io::Package& package, std::string_view id, const core::Json& values);
};

} // namespace haylen::platform
