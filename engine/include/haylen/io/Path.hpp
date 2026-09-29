#pragma once

#include <string>
#include <string_view>

namespace haylen::io {

// Package paths: relative, separated by forward slashes and never leaving their root folder.
class Path final {
  public:
    // A package holds app.json, the Lua modules under source, the assets under content and, for every plugin that app.json lists, plugins/<id>/plugin.json with the Lua modules of the plugin under plugins/<id>/source.
    static constexpr std::string_view kAppConfigFile = "app.json";
    static constexpr std::string_view kSourceDirectory = "source";
    static constexpr std::string_view kContentDirectory = "content";
    static constexpr std::string_view kPluginsDirectory = "plugins";
    static constexpr std::string_view kPluginManifestFile = "plugin.json";

    // Normalizes a relative package path to forward slashes without empty or "." segments. Throws std::invalid_argument for absolute paths and paths that leave the root.
    [[nodiscard]] static std::string normalize(std::string_view path);

    // Returns the package path of an asset, which always lives under the package content folder.
    [[nodiscard]] static std::string asset(std::string_view path);

    // Returns the package path of a file in the folder of a plugin, such as plugins/admob/plugin.json.
    [[nodiscard]] static std::string plugin(std::string_view id, std::string_view relative);

    [[nodiscard]] static std::string_view extension(std::string_view path) noexcept;
    [[nodiscard]] static std::string_view directory(std::string_view path) noexcept;
    [[nodiscard]] static std::string join(std::string_view folder, std::string_view relative);

    // Tells whether a normalized path lies under a normalized folder. Every path lies under the empty root folder.
    [[nodiscard]] static bool isInside(std::string_view path, std::string_view folder) noexcept;
};

} // namespace haylen::io
