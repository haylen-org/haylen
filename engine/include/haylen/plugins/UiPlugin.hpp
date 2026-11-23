#pragma once

#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Signal.hpp"
#include "haylen/platform/Accessibility.hpp"
#include "haylen/plugins/Plugin.hpp"
#include "haylen/text/Direction.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/ComponentRegistry.hpp"
#include "haylen/ui/Context.hpp"
#include "haylen/ui/Event.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Navigation.hpp"
#include "haylen/ui/NavigationInput.hpp"
#include "haylen/ui/Placement.hpp"
#include "haylen/ui/Region.hpp"
#include "haylen/ui/Scaling.hpp"
#include "haylen/ui/Theme.hpp"

namespace haylen::core {
class Scene;
struct SceneView;
} // namespace haylen::core

namespace haylen::ui {
class ImageLibrary;
class MessageView;
class PresentationView;
class SoundLibrary;
} // namespace haylen::ui

namespace haylen::plugins {

// Runs the app UI: Dear ImGui over the app, the themes, the component kinds and the GUIs the app mounts.
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
    void render(core::Engine& engine) override;
    void renderUi(core::Engine& engine, const core::SceneView& view) override;
    [[nodiscard]] bool isCapturingBack() const override;
    [[nodiscard]] bool isFrameComplete() const override;

    // Whether the UI still moves or waits for time to pass, as `ui::Context::requestFrame` asks, a transition or a presentation of the navigation moves, ImGui settles, or events of the GUIs and requests of the person wait for the next update.
    [[nodiscard]] bool isAnimating() const override;

    // Builds a theme that `loadTheme` read from a changed file again on its base, and applies it when it is the current theme, loads the sounds of the UI read from changed files again, and reads a changed font file of a UI font again, which reaches ImGui before its next frame.
    void packageChanged(core::Engine& engine, std::span<const std::string> paths) override;

    [[nodiscard]] ui::Backend& getBackend();
    [[nodiscard]] ui::Context& getContext();
    [[nodiscard]] ui::ImageLibrary& getImages();
    [[nodiscard]] ui::FocusNavigator& getFocus() noexcept {
        return focus;
    }
    [[nodiscard]] const ui::NavigationInput& getNavigationInput() const noexcept {
        return navigationInput;
    }

    // The stacks, tabs, splits, presentations and messages of the app.
    [[nodiscard]] ui::Navigation& getNavigation();
    [[nodiscard]] ui::ComponentRegistry& getComponents() noexcept {
        return components;
    }

    [[nodiscard]] std::shared_ptr<ui::Gui> createGui(const core::Json& tree, ui::Placement placement = ui::Placement::Safe) const;

    // GUIs draw in layer order, and GUIs on the same layer in the order they were mounted. A GUI of a scene draws only while its scene shows and goes into the image of its scene during a transition, so it travels with its scene, while a GUI of no scene draws above the scenes the app runs, on the screen above a transition too, where it takes no input until the transition ends. Mounting and unmounting publish `guiMounted` and `guiUnmounted` with the shared pointer of the GUI. A GUI whose collections bind cells refuses to unmount. Throws `std::invalid_argument` for a GUI of the region placement, which only the navigation mounts as the GUI of a screen.
    void mount(std::shared_ptr<ui::Gui> gui, int layer = 0, const std::shared_ptr<const core::Scene>& scene = nullptr);

    // Throws `std::invalid_argument` for a region whose GUI is not mounted or has no node with the id.
    void checkRegion(const ui::Region& region) const;
    bool unmount(const ui::Gui& gui);
    [[nodiscard]] bool isMounted(const ui::Gui& gui) const;

    // Returns the mounted GUI at an address, such as the one the focus navigator names, or nothing when no mounted GUI lives there.
    [[nodiscard]] std::shared_ptr<ui::Gui> findMounted(const ui::Gui* gui) const;

    // Hands out the events of every mounted GUI once per frame, before the app updates, to the listeners of the GUIs and then to the handlers of its nodes, which listen with the lowest priority.
    core::Signal<ui::Gui&, const ui::Event&> events;

    // Fires for every GUI that unmounted, once its last events reached the listeners, so what keeps state of the GUI lets go of it.
    core::Signal<const ui::Gui&> guiUnmounted;

    // The theme the app chose. While the system asks for higher contrast the UI draws the high contrast theme that the chosen theme names with the sounds of the chosen theme, and while it asks for reduced transparency it draws the surfaces of the theme opaque, so `getTheme` returns the theme the UI draws. Throws for a theme whose high contrast theme the UI does not know. A theme starts loading its sounds in the background once it is added.
    void addTheme(ui::Theme theme);
    void setTheme(std::string_view name);
    [[nodiscard]] const std::string& getThemeName() const noexcept {
        return themeName;
    }
    [[nodiscard]] const ui::Theme& getTheme() const noexcept {
        return shownTheme;
    }

    // Returns a registered theme, or null for a name the UI does not know.
    [[nodiscard]] const ui::Theme* findTheme(std::string_view name) const;
    [[nodiscard]] std::vector<std::string> getThemes() const;

    // Registers a theme from its JSON definition on top of the theme named `base`, registers the fonts it lists and returns the theme name. Throws for a sound file that the package does not hold.
    std::string addTheme(core::Engine& engine, const core::Json& definition, std::string_view base = "dark");
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

    // How large the interface draws from the next frame, which the `ui` section of `haylen.json` sets at start. The backend reports the design units one UI unit spans that it resolves to.
    void setScaling(const ui::Scaling& value) noexcept {
        scaling = value;
    }
    [[nodiscard]] const ui::Scaling& getScaling() const noexcept {
        return scaling;
    }

    // How much larger the text of the UI draws than the sizes of its themes, or nothing to follow the text scale of the system, which is how the UI starts.
    void setTextScale(std::optional<float> value) noexcept {
        textScale = value;
    }
    [[nodiscard]] const std::optional<float>& getTextScale() const noexcept {
        return textScale;
    }

    // Shades the screen outside the safe area and outlines it over everything, to check layouts against notches and system bars. The `debug.showSafeArea` option of `haylen.json` turns it on at start.
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
    friend class ui::Navigation;

    // The layer of the GUIs of presentations, above every GUI of the app.
    static constexpr int kPresentationLayer = std::numeric_limits<int>::max();

    struct Mounted {
        std::shared_ptr<ui::Gui> gui;
        std::weak_ptr<const core::Scene> scene;
        bool scened = false;
        int layer = 0;
        std::uint64_t order = 0;

        // The screen whose GUI this is, which the navigation mounts and places.
        const ui::Screen* screen = nullptr;

        // The GUI and the node of a region, and the place in the drawing order, which a GUI of a region takes from the GUI of its region.
        std::weak_ptr<ui::Gui> host;
        std::string hostNode;
        int drawLayer = 0;
        std::uint64_t drawOrder = 0;
    };

    // Builds the theme the UI draws from the chosen theme and the accessibility settings, and applies it.
    void applyTheme(bool highContrast, bool reducedTransparency);
    void collectGuis(core::Engine& engine, const core::SceneView& view, bool sceneGuis, bool freeGuis);
    [[nodiscard]] const Mounted& findRegionHost(const ui::Region& region) const;

    // Mounts the GUI of a screen in the area of a stack, or as the GUI of a presentation, which draws above every other GUI.
    void mountScreen(const ui::Screen& screen, const ui::Region* region);

    // Lays out the GUI of a screen in the area of another stack, such as when a split collapses.
    void moveScreen(const ui::Gui& gui, const ui::Region& region);

    void add(Mounted entry);
    void drawGui(const Mounted& entry, bool inert);
    void drawPresented(const Mounted& entry, ui::Presentation& presentation, const ui::Navigation::Look& look, bool inert);
    void drawScreenGui(const Mounted& entry, const ui::Navigation::Look& look, const math::Rect& area, math::Vec2 offset, const std::optional<math::Rect>& clip, bool inert);

    // Whether a GUI laid out in an earlier frame with every picture it shows, which the navigation waits for before a screen moves in.
    [[nodiscard]] bool isComplete(const ui::Gui& gui) const;
    [[nodiscard]] math::Vec2 getLookOffset(const ui::Navigation::Look& look, const math::Rect& area, float riseRoom) const;
    [[nodiscard]] std::optional<math::Rect> findArea(const Mounted& entry) const;
    [[nodiscard]] math::Rect getMessageArea() const;

    // Performs an action that a screen reader asked of an element of the last frame, and hands the elements of this frame to the platform.
    void perform(const platform::Accessibility::Request& request);
    void publishAccessibility(core::Engine& engine);
    void deliverEvents();
    void deliver(ui::Gui& gui, const ui::Event& event);
    void deliverAtOnce(ui::Gui& gui, const ui::Event& event);
    void prepareGuis();
    void beginWindow(const char* name, ImGuiWindowFlags flags);
    void drawGuis(std::size_t first, std::size_t last, bool inert);
    [[nodiscard]] std::size_t countUnderPresentation() const;
    void drawUnder(core::Engine& engine, std::size_t count);
    void drawLeaving(core::Engine& engine, const core::SceneView& view);
    void drawAbove(core::Engine& engine, const core::SceneView& view);
    void drawCurrent(core::Engine& engine, const core::SceneView& view);
    // The size of the text of the insets that the safe area overlay shows.
    static constexpr float kSafeAreaLabelSize = 24.0F;

    void drawSafeArea();
    static void drawSafeAreaLabel(ImDrawList& list, const math::Rect& display, const math::Rect& safe, ImU32 color);
    [[nodiscard]] input::ActionMap::Capture getCapture() const;
    static void addCapture(input::ActionMap::Capture& capture, const std::vector<input::ActionMap::Binding>& bindings);

    // Returns the file of a TrueType face, or nothing for a bitmap face or a style the family has no face for.
    [[nodiscard]] static std::vector<std::uint8_t> getTrueTypeData(const std::shared_ptr<text::Font>& face);

    // The TrueType files of the faces and fallbacks of a family that ImGui draws, without fallbacks for a family whose regular face is no TrueType font.
    [[nodiscard]] static ui::Backend::FontFiles readFamilyFiles(const text::FontFamily& family);

    // Hands ImGui the fonts that changed, between its frames, and applies the theme again for the faces that took their place.
    void replaceChangedFonts();

    ui::ComponentRegistry components;
    ui::FocusNavigator focus;
    ui::NavigationInput navigationInput;
    std::unique_ptr<ui::Navigation> navigation;

    // The system back that the focus handed to the navigation, which the next update takes, what draws around presentations and follows their gestures, and what draws the messages above everything.
    bool backRequested = false;
    std::unique_ptr<ui::PresentationView> presentationView;
    std::unique_ptr<ui::MessageView> messageView;
    std::unique_ptr<ui::Backend> backend;
    std::unique_ptr<ui::Context> context;
    std::map<std::string, ui::Theme, std::less<>> themes;
    std::string themeName = "dark";
    ui::Theme shownTheme = ui::Theme::dark();
    bool shownHighContrast = false;
    bool shownOpaque = false;
    std::optional<float> textScale;
    std::vector<Mounted> guis;

    // The GUIs whose events the update delivers and those a view draws, kept between frames so they allocate nothing.
    std::vector<Mounted> delivering;
    std::vector<const Mounted*> drawnGuis;
    std::vector<const ui::Gui*> shownGuis;

    // The GUIs that asked for a picture that had nothing to draw yet when they last drew, which a change of the navigation waits for.
    std::vector<const ui::Gui*> waitingGuis;
    std::vector<std::shared_ptr<ui::Gui>> preparing;
    std::vector<platform::Accessibility::Request> accessibilityRequests;
    std::uint64_t nextOrder = 0;
    std::unique_ptr<ui::ImageLibrary> images;
    std::unique_ptr<ui::SoundLibrary> sounds;
    std::map<std::string, std::string, std::less<>> fontPaths;

    // The base of every theme file that `loadTheme` read, by its asset path.
    std::map<std::string, std::string, std::less<>> themeFiles;
    std::map<std::string, std::shared_ptr<text::FontFamily>, std::less<>> fontFamilies;

    // The revisions of the families that `addFontFamily` gave ImGui, and the fonts whose files changed since the last frame of ImGui.
    std::map<std::string, std::uint64_t, std::less<>> familyRevisions;
    std::vector<std::pair<std::string, ui::Backend::FontFiles>> changedFonts;
    double elapsed = 0.0;
    text::Direction direction = text::Direction::LeftToRight;
    bool safeAreaVisible = false;
    ui::Scaling scaling;

    // The points per UI unit the ImGui style took its scroll bar gap at, so a change of scale styles it again.
    float styledPoints = 0.0F;
    bool drawBegun = false;
    bool guiAppeared = false;
    core::Engine* owner = nullptr;
};

} // namespace haylen::plugins
