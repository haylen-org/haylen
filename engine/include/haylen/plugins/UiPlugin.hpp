#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Signal.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/plugins/Plugin.hpp"
#include "haylen/text/Direction.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/ComponentRegistry.hpp"
#include "haylen/ui/Context.hpp"
#include "haylen/ui/Document.hpp"
#include "haylen/ui/Event.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "haylen/ui/NavigationInput.hpp"
#include "haylen/ui/Placement.hpp"
#include "haylen/ui/Theme.hpp"

namespace haylen::plugins {

// Runs the app UI: Dear ImGui over the app, the themes, the component kinds and the documents the app mounts. Lua sees it as `haylen.ui` and `haylen.imgui`.
class UiPlugin final : public Plugin {
  public:
    UiPlugin();
    ~UiPlugin() override;

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "ui";
    }
    void start(core::Engine& engine) override;
    void stop(core::Engine& engine) override;
    void event(core::Engine& engine, const platform::Event& event) override;
    void beginFrame(core::Engine& engine, float deltaSeconds) override;
    void update(core::Engine& engine, float deltaSeconds) override;
    void renderUi(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;
    [[nodiscard]] bool isCapturingBack() const override;

    [[nodiscard]] ui::Backend& getBackend();
    [[nodiscard]] ui::Context& getContext();
    [[nodiscard]] ui::FocusNavigator& getFocus() noexcept {
        return focus;
    }
    [[nodiscard]] const ui::NavigationInput& getNavigation() const noexcept {
        return navigation;
    }
    [[nodiscard]] ui::ComponentRegistry& getComponents() noexcept {
        return components;
    }

    [[nodiscard]] std::shared_ptr<ui::Document> createDocument(const core::Json& tree, ui::Placement placement = ui::Placement::Safe) const;

    // Documents draw in layer order, and documents on the same layer in the order they were mounted. Mounting and unmounting publish `uiDocumentMounted` and `uiDocumentUnmounted` with the shared pointer of the document.
    void mount(std::shared_ptr<ui::Document> document, int layer = 0);
    bool unmount(const ui::Document& document);
    [[nodiscard]] bool isMounted(const ui::Document& document) const;

    // Returns the mounted document at an address, such as the one the focus navigator names, or nothing when no mounted document lives there.
    [[nodiscard]] std::shared_ptr<ui::Document> findMounted(const ui::Document* document) const;

    // Hands out the events of every mounted document once per frame, before the app updates.
    core::Signal<ui::Document&, const ui::Event&> events;

    void addTheme(ui::Theme theme);
    void setTheme(std::string_view name);
    [[nodiscard]] const ui::Theme& getTheme() const;
    [[nodiscard]] std::vector<std::string> getThemes() const;

    // Registers a theme from its JSON document on top of the theme named `base`, registers the fonts it lists and returns the theme name.
    std::string addTheme(core::Engine& engine, const core::Json& document, std::string_view base = "dark");
    std::string loadTheme(core::Engine& engine, std::string_view path, std::string_view base = "dark");
    void addFont(core::Engine& engine, const std::string& name, std::string_view path);

    // Registers a font family under a name that themes and rich text use. Widgets drawn by ImGui use its TrueType faces, the regular one for roles without a style and the bold and italic ones for roles that ask for them, and draw the characters a face lacks from its TrueType fallbacks. A theme role can only name a family whose regular face is a TrueType font.
    void addFontFamily(const std::string& name, std::shared_ptr<text::FontFamily> family);

    // Returns the family of a UI font name: a registered family, the font file of a name, or the default font, and null for a name the UI does not know.
    [[nodiscard]] std::shared_ptr<text::FontFamily> getFontFamily(core::Engine& engine, std::string_view name);

    // The direction of the whole UI, which mirrors its layouts and sets the direction its text reads in when right to left. The direction `Auto` follows the direction the localization catalog declares for the current language, and nodes with a direction of their own keep it.
    void setDirection(text::Direction value) noexcept {
        direction = value;
    }
    [[nodiscard]] text::Direction getDirection() const noexcept {
        return direction;
    }

    // Shades the screen outside the safe area and outlines it over everything, to check layouts against notches and system bars. The `debug.showSafeArea` option of `app.json` turns it on at start.
    void setSafeAreaVisible(bool value) noexcept {
        safeAreaVisible = value;
    }
    [[nodiscard]] bool isSafeAreaVisible() const noexcept {
        return safeAreaVisible;
    }

    // Tell whether the interface uses the pointer or the keyboard this frame, which gameplay input then leaves alone.
    [[nodiscard]] bool isUsingPointer() const;
    [[nodiscard]] bool isUsingKeyboard() const;

  private:
    struct Mounted {
        std::shared_ptr<ui::Document> document;
        int layer = 0;
        std::uint64_t order = 0;
    };

    struct ImageEntry {
        graphics::Texture texture;
        std::string error;
        graphics::Texture::Filter filter = graphics::Texture::Filter::Nearest;
    };

    // Returns the texture of a UI image with the image filter of the theme, which loads in the background and is empty until it arrives.
    [[nodiscard]] graphics::Texture requestImage(core::Engine& engine, std::string_view path);
    void applyVirtualInput(core::Engine& engine);
    void drawSafeArea();
    [[nodiscard]] input::ActionMap::Capture getCapture() const;

    // Returns the file of a TrueType face, or nothing for a bitmap face or a style the family has no face for.
    [[nodiscard]] static std::vector<std::uint8_t> getTrueTypeData(const std::shared_ptr<text::Font>& face);

    ui::ComponentRegistry components;
    ui::FocusNavigator focus;
    ui::NavigationInput navigation;
    std::unique_ptr<ui::Backend> backend;
    std::unique_ptr<ui::Context> context;
    std::map<std::string, ui::Theme, std::less<>> themes;
    std::string themeName = "dark";
    std::vector<Mounted> documents;
    std::uint64_t nextOrder = 0;
    std::map<std::string, ImageEntry, std::less<>> images;
    std::map<std::string, std::string, std::less<>> fontPaths;
    std::map<std::string, std::shared_ptr<text::FontFamily>, std::less<>> fontFamilies;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    std::set<std::string, std::less<>> heldButtons;
    std::set<std::string, std::less<>> sticks;
    double elapsed = 0.0;
    text::Direction direction = text::Direction::LeftToRight;
    bool safeAreaVisible = false;
    core::Engine* owner = nullptr;
};

} // namespace haylen::plugins
