#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::io {
class Package;
}

namespace haylen::platform {

// The parts of the `plugin.json` of a plugin in the package that the runtime reads: the version, the plugins it depends on, the permissions it owns, the defaults of its parameters, the platforms where its native part runs and the class of its native part on each Apple platform. The tooling checks the whole file against its schema, and the runtime checks again what it reads, since the player also runs package folders that the tooling never saw.
class PluginManifest final {
  public:
    // Reads `plugins/<id>/plugin.json` of the package. Throws `std::runtime_error` when the manifest is missing, is not a JSON object, has no version, or holds a field that the runtime reads in a form other than the one of the schema.
    [[nodiscard]] static PluginManifest read(const io::Package& package, std::string_view id);

    [[nodiscard]] const std::string& getVersion() const noexcept;

    // The ids of the plugins that `dependencies` names, in alphabetical order.
    [[nodiscard]] const std::vector<std::string>& getDependencies() const noexcept;

    // The permissions that the plugin owns, distinct `camelCase` names other than the permissions of the engine.
    [[nodiscard]] const std::vector<std::string>& getPermissions() const noexcept;

    // The defaults of the parameters that declare one, by name.
    [[nodiscard]] const core::Json& getDefaults() const noexcept;

    // Whether `implementations` gives the plugin its native part on the platform, such as `ios` or `android`.
    [[nodiscard]] bool isNative(std::string_view platform) const;

    // The Objective-C class of the native part on an Apple platform, from the contribution of type `appleClass` that lists it, or nothing where the plugin has no native part or names no class.
    [[nodiscard]] std::optional<std::string> findAppleClass(std::string_view platform) const;

  private:
    struct AppleClass {
        std::string name;
        std::vector<std::string> platforms;
    };

    std::string version;
    std::vector<std::string> dependencies;
    std::vector<std::string> permissions;
    core::Json defaults = core::Json::object();
    std::vector<std::string> nativePlatforms;
    std::vector<AppleClass> appleClasses;

    [[nodiscard]] static std::vector<std::string> readDependencies(const core::Json& manifest, const std::string& path);
    [[nodiscard]] static std::vector<std::string> readPermissions(const core::Json& manifest, const std::string& path);
    [[nodiscard]] static core::Json readDefaults(const core::Json& manifest, const std::string& path);
    [[nodiscard]] static std::vector<std::string> readNativePlatforms(const core::Json& manifest, const std::string& path);
    [[nodiscard]] static std::vector<AppleClass> readAppleClasses(const core::Json& manifest, const std::string& path);
    [[nodiscard]] static std::vector<std::string> readPlatforms(const core::Json& contribution, const std::string& path);
};

} // namespace haylen::platform
