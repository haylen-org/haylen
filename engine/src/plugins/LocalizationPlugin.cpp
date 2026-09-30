#include "haylen/plugins/LocalizationPlugin.hpp"

#include <filesystem>
#include <stdexcept>

#include "haylen/io/Package.hpp"
#include "localization/LocalizationLua.hpp"

namespace haylen::plugins {

std::vector<std::string> LocalizationPlugin::loadFolder(const io::Package& package, std::string_view folder) {
    std::vector<std::string> languages;
    for (const std::string& file : package.listAssets(folder)) {
        const std::filesystem::path path(file);
        if (path.extension() != ".json") {
            continue;
        }
        const core::Json table = core::Json::parse(package.readAssetText(file), nullptr, false);
        if (table.is_discarded()) {
            throw std::runtime_error("The localization file \"" + file + "\" is not valid JSON.");
        }
        const std::string language = path.stem().string();
        catalog.add(language, table);
        languages.push_back(language);
    }
    return languages;
}

void LocalizationPlugin::installLua(core::Engine&, lua_State* L) {
    localization::LocalizationLua::install(L);
}

} // namespace haylen::plugins
