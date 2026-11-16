#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/Json.hpp"

#include "haylen/ui/NavigationSplit.hpp"
#include "haylen/ui/NavigationTabs.hpp"
#include "haylen/ui/NavigationTransition.hpp"
#include "haylen/ui/Presentation.hpp"
#include "haylen/ui/Region.hpp"

namespace haylen::core {
class Engine;
}

namespace haylen::plugins {
class UiPlugin;
}

namespace haylen::ui {

class Gui;
class NavigationStack;
class Screen;

// The navigation of the app: the stacks, tabs and splits that show screens in the areas of GUIs, and the presentations over everything, among them the alerts and action sheets that the engine builds. A stack, tabs or a split lives until it closes, its owner ends or the GUI of its area unmounts, and a presentation until it is dismissed. Screens may hold areas of their own, so a stack can live in a screen of another stack, such as a split inside a tab. The system back goes through the layers from the top, the presentations and then the app under them. In each layer it pops the innermost stack that shows and can pop, then goes to the first tab, and then a modal presentation is dismissed and keeps the back from the layers under it, while a sheet that is not modal goes down to its lowest detent. Only then does the back reach the app.
class Navigation final {
  public:
    Navigation(core::Engine& engine, plugins::UiPlugin& ui);
    ~Navigation();

    Navigation(const Navigation&) = delete;
    Navigation& operator=(const Navigation&) = delete;

    // An alert or an action sheet that the navigation builds and presents: a title, a message and the actions the person chooses from. An alert interrupts with a decision in a dialog, such as a confirmation before something is deleted. An action sheet offers the choices of one control, such as the ways to share a picture, next to that control on a wide screen and from the bottom of a compact one. The cancel action leads and lets the person dismiss the alert, a destructive action shows in the danger color, and the last default action is the preferred one.
    struct Alert {
        enum class Style : std::uint8_t {
            Alert,
            ActionSheet,
        };

        enum class Role : std::uint8_t {
            Default,
            Cancel,
            Destructive,
        };

        struct Action {
            std::string id;

            // The label of the action, a text value of the UI such as a string or a localized key.
            core::Json text;
            Role role = Role::Default;
        };

        // The width of an action sheet that points at its anchor, in UI units.
        static constexpr float kActionSheetWidth = 520.0F;

        static constexpr std::array<std::pair<std::string_view, Style>, 2> kStyleNames{{{"alert", Style::Alert}, {"actionSheet", Style::ActionSheet}}};
        static constexpr std::array<std::pair<std::string_view, Role>, 3> kRoleNames{{{"default", Role::Default}, {"cancel", Role::Cancel}, {"destructive", Role::Destructive}}};

        // The cancel action, which the person also chooses by dismissing the alert, or null for an alert that only its actions close.
        [[nodiscard]] const Action* findCancel() const noexcept;

        Style style = Style::Alert;
        core::Json title;
        core::Json message;
        std::vector<Action> actions;

        // The node that an action sheet points at on a wide screen.
        std::optional<Region> anchor;
    };

    // Creates a stack in the area of a node of a mounted GUI, with a first screen when one is given. Throws `std::invalid_argument` for a GUI that is not mounted or that has no node with the id.
    [[nodiscard]] std::shared_ptr<NavigationStack> createStack(Region region, std::shared_ptr<Screen> root = nullptr);
    [[nodiscard]] std::shared_ptr<NavigationTabs> createTabs(Region region, std::vector<NavigationTabs::Tab> tabs, std::string_view selected);
    [[nodiscard]] std::shared_ptr<NavigationSplit> createSplit(Region primary, Region detail, std::shared_ptr<Screen> root, NavigationSplit::Factory placeholder, bool collapsed);

    // Presents a screen over everything the app shows.
    [[nodiscard]] std::shared_ptr<Presentation> present(std::shared_ptr<Screen> screen, Presentation::Options options);

    // Presents an alert or an action sheet that the engine builds, whose result is the id of the action the person chose. Throws `std::invalid_argument` for no actions, an action without an id, a repeated id and more than one cancel action.
    [[nodiscard]] std::shared_ptr<Presentation> presentAlert(Alert alert);

    // The presentations that show or leave, from the bottom up.
    [[nodiscard]] const std::vector<std::shared_ptr<Presentation>>& getPresentations() const noexcept {
        return presentations;
    }

    // Whether the system back goes back somewhere in the app: a stack that pops, tabs that go to the first tab or a presentation that leaves.
    [[nodiscard]] bool canGoBack() const;

    // Takes the system back and returns whether it went back somewhere. The screen that a pop or a dismissal would take away hears it first and may keep its place.
    bool goBack();

    // The longest that a change waits for the GUI of the screen it shows to lay out with every picture, in seconds, after which it plays and a late picture shows when it arrives.
    static constexpr float kReadyWait = 0.5F;

  private:
    friend class plugins::UiPlugin;
    friend class NavigationStack;
    friend class NavigationTabs;
    friend class NavigationSplit;
    friend class Presentation;
    friend class PresentationView;

    // How a screen draws in a frame. A screen that is shown lays out in its area moved by the effect of its transition: a slide by the distance times the width of its area toward its side, which a right to left interface mirrors, a rise by the distance times the room from the top of its area to the bottom of the screen, and a fade to an opacity of one minus the distance. An inert screen takes no input and no focus.
    struct Look {
        bool shown = false;
        bool inert = false;
        NavigationTransition::Effect effect = NavigationTransition::Effect::None;
        float distance = 0.0F;

        // The side a slide moves toward, 1 for the end of the reading direction and -1 for its start.
        float side = 1.0F;
    };

    [[nodiscard]] Look getLook(const Screen& screen) const;

    // Moves the transitions on and runs the update hook of the screens that draw.
    void update(float deltaSeconds);

    // Runs the render hook of the screens that draw.
    void render();

    // Closes the stacks, tabs and splits whose area lies in the GUI, which unmounts.
    void closeHostedBy(const Gui& gui);

    // Ends everything at once, as the UI stops.
    void closeAll();

    // Whether the system asks for reduced motion now.
    [[nodiscard]] bool isMotionReduced() const;

    // Runs a hook of a screen and reports its failure to the error screen as a failure of the lifecycle.
    template <typename Hook> void notify(Screen& screen, Hook&& hook);
    [[nodiscard]] bool askBack(Screen& screen);

    // Whether the GUI of a screen laid out in an earlier frame with every picture it shows, or the screen has no GUI to wait for.
    [[nodiscard]] bool isReady(const Screen& screen) const;

    // A screen joins a stack or a presentation, or moves to another stack.
    void place(Screen& screen, const std::shared_ptr<NavigationStack>& stack);
    void place(Screen& screen, const std::shared_ptr<Presentation>& presentation);

    // The steps of a screen that shows and leaves. A screen that starts to show builds and mounts its GUI the first time, and the top screens of the stacks in its GUI show and leave with it.
    void beginAppear(Screen& screen);
    void endAppear(Screen& screen);
    void beginDisappear(Screen& screen);
    void endDisappear(Screen& screen);
    void showAtOnce(Screen& screen);
    void hideAtOnce(Screen& screen);
    void removeScreen(Screen& screen);

    void mountScreen(Screen& screen);
    void unmountScreen(Screen& screen);
    void checkRegion(const Region& region) const;
    void showHosted(const Screen& screen, bool value);

    // Whether the screen whose GUI holds the area of a stack shows, which a stack in a GUI of no screen always finds.
    [[nodiscard]] bool isHostShowing(const NavigationStack& stack) const;

    // Whether the top screen of a stack draws: the stack shows and its area lies in a GUI that shows, or in a screen that draws.
    [[nodiscard]] bool isDrawn(const NavigationStack& stack) const;
    [[nodiscard]] bool isDrawn(const Screen& screen) const;

    // The screen whose GUI a stack lies in, or null for a stack in a GUI of no screen.
    [[nodiscard]] const Screen* findHostScreen(const NavigationStack& stack) const;
    [[nodiscard]] std::size_t getDepth(const NavigationStack& stack) const;

    // The presentation a stack lies in through the screens that hold its area, or null for a stack under every presentation.
    [[nodiscard]] const Presentation* findPresentation(const NavigationStack& stack) const;

    // The topmost modal presentation that has not left, which takes the input of everything under it.
    [[nodiscard]] const Presentation* getTopModal() const;

    // Collects the stacks of a layer, a presentation or the app under every presentation, that draw, the innermost first.
    void collectActive(const Presentation* layer, std::vector<NavigationStack*>& found) const;

    // Whether the system back goes back in the stacks and tabs of a layer, and goes back there.
    [[nodiscard]] bool canGoBackIn(const Presentation* layer) const;
    [[nodiscard]] bool goBackIn(const Presentation* layer);
    void collectDrawn(std::vector<std::shared_ptr<Screen>>& found) const;

    void forget(const NavigationStack& stack);
    void forget(const NavigationTabs& closed);
    void forget(const NavigationSplit& split);
    void forget(const Presentation& presentation);

    core::Engine& engine;
    plugins::UiPlugin& ui;
    std::vector<std::shared_ptr<NavigationStack>> stacks;
    std::vector<std::shared_ptr<NavigationTabs>> tabs;
    std::vector<std::shared_ptr<NavigationSplit>> splits;
    std::vector<std::shared_ptr<Presentation>> presentations;

    // The containers and screens that an update or a render walks, kept between frames so they allocate nothing.
    std::vector<std::shared_ptr<NavigationStack>> walkedStacks;
    std::vector<std::shared_ptr<Presentation>> walkedPresentations;
    std::vector<std::shared_ptr<Screen>> walkedScreens;
    mutable std::vector<NavigationStack*> activeStacks;
};

} // namespace haylen::ui
