#include "haylen/plugins/LocalizationPlugin.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "haylen/assets/Manager.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/platform/System.hpp"
#include "localization/LanguageTag.hpp"
#include "localization/LocalizationLua.hpp"

namespace haylen::plugins {

// The languages of the configuration load before the scripts, and the system decides the language unless the app names one.
void LocalizationPlugin::start(core::Engine& engine) {
    loadConfiguredLanguages(engine);
    catalog.setSystemLanguages(engine.getSystem().getLanguages());
    announced = catalog.getLanguage();
}

void LocalizationPlugin::loadConfiguredLanguages(core::Engine& engine) {
    const core::AppConfig::Localization& configured = engine.getConfig().localization;
    if (!configured.folder.empty()) {
        const std::vector<std::string> loaded = loadFolder(engine.getPackage(), configured.folder);
        if (loaded.empty()) {
            throw std::runtime_error("The folder \"" + configured.folder + "\" of \"localization.folder\" in \"app.json\" holds no language file. Add a file such as \"" + configured.folder + "/en.json\".");
        }
    }

    if (!configured.fallback.empty()) {
        const std::vector<std::string>& languages = catalog.getLanguages();
        if (std::ranges::find(languages, configured.fallback) == languages.end()) {
            throw std::runtime_error("The \"localization.fallback\" of \"app.json\" names \"" + configured.fallback + "\", whose file the folder \"" + configured.folder + "\" lacks.");
        }
        catalog.setFallback(configured.fallback);
    }

    if (!configured.language.empty()) {
        catalog.setLanguage(configured.language);
    }
}

// The list of the system is compared, not copied, on frames where nothing changed.
void LocalizationPlugin::beginFrame(core::Engine& engine, float) {
    const std::vector<std::string>& reported = engine.getSystem().getLanguages();
    if (reported != catalog.getSystemLanguages()) {
        catalog.setSystemLanguages(reported);
    }

    if (catalog.getLanguage() == announced) {
        return;
    }
    std::string previous = std::exchange(announced, catalog.getLanguage());
    engine.getEvents().emit(core::LifecycleEvent::kLanguageChanged, {{"language", announced}, {"previous", std::move(previous)}});
}

std::vector<std::string> LocalizationPlugin::loadFolder(const io::Package& package, std::string_view folder) {
    std::vector<std::string> languages;
    for (const std::string& file : package.listAssets(folder)) {
        if (std::filesystem::path(file).extension() != ".json") {
            continue;
        }
        std::string language = addFile(package, file);
        languages.push_back(std::move(language));
    }
    folders.insert(io::Path::normalize(folder));
    return languages;
}

std::string LocalizationPlugin::addFile(const io::Package& package, const std::string& file) {
    const core::Json table = core::Json::parse(package.readAssetText(file), nullptr, false);
    if (table.is_discarded()) {
        throw std::runtime_error("The localization file \"" + file + "\" is not valid JSON.");
    }
    const std::string name = std::filesystem::path(file).stem().string();
    const std::optional<std::string> language = localization::LanguageTag::parse(name);
    if (!language) {
        throw std::runtime_error("The localization file \"" + file + "\" is not named after a language tag. Name it after its language, such as \"en.json\", \"pt-BR.json\" or \"pt_BR.json\".");
    }
    catalog.add(*language, table);
    return *language;
}

std::string LocalizationPlugin::getAssetPath(const assets::Manager& assets, std::string_view path) const {
    for (std::string_view tag = catalog.getLanguage(); !tag.empty(); tag = localization::LanguageTag::getParent(tag)) {
        std::string localized = "i18n/" + std::string(tag) + "/" + std::string(path);
        if (assets.exists(localized)) {
            return localized;
        }
    }
    return std::string(path);
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
