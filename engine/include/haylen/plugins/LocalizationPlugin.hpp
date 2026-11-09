#pragma once

#include <functional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/localization/Catalog.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::assets {
class Manager;
}

namespace haylen::io {
class Package;
}

namespace haylen::plugins {

// Owns the translated text of the app and exposes it to Lua as `haylen.localization`. It loads the languages that the `localization` section of `app.json` names before any script runs, hands the catalog the languages of the system whenever they change, and publishes `languageChanged` at the start of the frame after the current language changed, whatever changed it.
class LocalizationPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "localization";
    }
    void start(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;
    void beginFrame(core::Engine& engine, float deltaSeconds) override;

    // Adds a changed language file of a loaded folder again, so `localization.text` returns its new texts at once. A text the file no longer has stays until the app restarts.
    void packageChanged(core::Engine& engine, std::span<const std::string> paths) override;

    [[nodiscard]] localization::Catalog& getCatalog() noexcept {
        return catalog;
    }

    // Adds every JSON file of a content folder as the language its file is named after, such as `locale/pt-BR.json` or `locale/pt_BR.json`, and returns those languages in their canonical form.
    std::vector<std::string> loadFolder(const io::Package& package, std::string_view folder);

    // Returns `i18n/<language>/<path>` for the first of the current language and the tags without its last subtags whose asset exists, such as `i18n/pt-BR/voice/intro.ogg` and then `i18n/pt/voice/intro.ogg`, and the path itself otherwise.
    [[nodiscard]] std::string getAssetPath(const assets::Manager& assets, std::string_view path) const;

  private:
    // Adds a language file and returns its language.
    std::string addFile(const io::Package& package, const std::string& file);
    void loadConfiguredLanguages(core::Engine& engine);

    localization::Catalog catalog;
    std::set<std::string, std::less<>> folders;

    // The current language that `languageChanged` last told the app about.
    std::string announced;
};

} // namespace haylen::plugins
