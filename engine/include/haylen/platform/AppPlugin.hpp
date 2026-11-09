#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/AppConfig.hpp"
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

    // The permissions that the plugin owns, which `system.permission` and `system.requestPermission` ask its native part for.
    std::vector<std::string> permissions;

    // Whether the native part of the plugin runs on this platform.
    bool native = false;

    // Reads the version, the permissions and the parameter defaults of `plugins/<id>/plugin.json` in the package and applies the `parameters` that `app.json` gives the plugin over them. Throws `std::runtime_error` when the manifest is missing, is not JSON, has no version or has permissions that are not a list of distinct `camelCase` names other than the permissions of the engine.
    [[nodiscard]] static AppPlugin read(const io::Package& package, std::string_view id, const core::Json& parameters);

    // Reads every plugin that `app.json` lists, as `read` does, in the order of its ids. Throws `std::runtime_error` as `read` does and when two plugins declare the same permission, since every permission has one owner.
    [[nodiscard]] static std::vector<AppPlugin> readAll(const io::Package& package, const std::map<std::string, core::AppConfig::Plugin>& plugins);

  private:
    [[nodiscard]] static std::vector<std::string> readPermissions(const core::Json& manifest, const std::string& path);
};

} // namespace haylen::platform
