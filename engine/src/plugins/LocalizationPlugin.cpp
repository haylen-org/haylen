#include "haylen/plugins/LocalizationPlugin.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <stdexcept>

#include "haylen/core/Engine.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "localization/LocalizationLua.hpp"

namespace haylen::plugins {

std::vector<std::string> LocalizationPlugin::loadFolder(const io::Package& package, std::string_view folder) {
    std::vector<std::string> languages;
    for (const std::string& file : package.listAssets(folder)) {
        const std::filesystem::path path(file);
        if (path.extension() != ".json") {
            continue;
        }
        addFile(package, file);
        languages.push_back(path.stem().string());
    }
    folders.insert(io::Path::normalize(folder));
    return languages;
}

void LocalizationPlugin::addFile(const io::Package& package, const std::string& file) {
    const core::Json table = core::Json::parse(package.readAssetText(file), nullptr, false);
    if (table.is_discarded()) {
        throw std::runtime_error("The localization file \"" + file + "\" is not valid JSON.");
    }
    catalog.add(std::filesystem::path(file).stem().string(), table);
}

// An editor may still be writing a file when it is seen, so a file that fails waits for the next save.
void LocalizationPlugin::packageChanged(core::Engine& engine, std::span<const std::string> paths) {
    for (const std::string& path : paths) {
        if (!io::Path::isInside(path, io::Path::kContentDirectory) || io::Path::extension(path) != ".json") {
            continue;
        }
        const std::string file = path.substr(io::Path::kContentDirectory.size() + 1);
        if (std::ranges::none_of(folders, [&file](const std::string& folder) { return io::Path::isInside(file, folder); }) || !engine.getPackage().assetExists(file)) {
            continue;
        }
        try {
            addFile(engine.getPackage(), file);
            core::Log::info("Reloaded the texts of \"{}\".", file);
        } catch (const std::exception& error) {
            core::Log::warning("The texts of \"{}\" could not be reloaded yet: {}", file, error.what());
        }
    }
}

void LocalizationPlugin::installLua(core::Engine&, lua_State* L) {
    localization::LocalizationLua::install(L);
}

} // namespace haylen::plugins
