#pragma once

#include <functional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/localization/Catalog.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::io {
class Package;
}

namespace haylen::plugins {

// Owns the translated text of the app and exposes it to Lua as `haylen.localization`.
class LocalizationPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "localization";
    }
    void installLua(core::Engine& engine, lua_State* L) override;

    // Adds a changed language file of a loaded folder again, so `localization.text` returns its new texts at once. A text the file no longer has stays until the app restarts.
    void packageChanged(core::Engine& engine, std::span<const std::string> paths) override;

    [[nodiscard]] localization::Catalog& getCatalog() noexcept {
        return catalog;
    }

    // Adds every JSON file of a content folder as the language its file is named after, such as `i18n/pt-BR.json`, and returns those languages.
    std::vector<std::string> loadFolder(const io::Package& package, std::string_view folder);

  private:
    void addFile(const io::Package& package, const std::string& file);

    localization::Catalog catalog;
    std::set<std::string, std::less<>> folders;
};

} // namespace haylen::plugins
