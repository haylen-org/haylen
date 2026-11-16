#pragma once

#include <array>
#include <functional>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/core/Signal.hpp"
#include "haylen/ui/Message.hpp"

#include "haylen/ui/NavigationTransition.hpp"
#include "haylen/ui/Presentation.hpp"
#include "haylen/ui/Region.hpp"

struct lua_State;

namespace haylen::ui {

class Navigation;
class NavigationSplit;
class NavigationStack;
class NavigationTabs;
class Screen;

// Adds the navigation to `haylen.ui`: the `Screen` base class, the functions that create stacks, tabs and splits, present screens, alerts and action sheets, show messages, the system back, and the classes `NavigationStack`, `NavigationTabs`, `NavigationSplit`, `Presentation` and `Message`.
class NavigationLua final {
  public:
    static void install(lua_State* L);

    // Sets the navigation functions and the `Screen` class on the module table at the index.
    static void addTo(lua_State* L, int module);

    // Push the one userdata of a stack or a presentation, so the same object always compares equal, or `nil` for none.
    static void pushStack(lua_State* L, const std::shared_ptr<NavigationStack>& stack);
    static void pushPresentation(lua_State* L, const std::shared_ptr<Presentation>& presentation);

  private:
    // The userdata of every navigation object, by its address, with weak values, so an object pushes as the same userdata while Lua holds it.
    static constexpr const char* kObjects = "haylen.navigationObjects";
    static constexpr std::array<std::string_view, 5> kStackFields{"gui", "id", "root", "owner", "onChange"};
    static constexpr std::array<std::string_view, 6> kTabsFields{"gui", "id", "tabs", "selected", "owner", "onChange"};
    static constexpr std::array<std::string_view, 2> kTabFields{"id", "root"};
    static constexpr std::array<std::string_view, 6> kSplitFields{"primary", "detail", "root", "placeholder", "collapsed", "owner"};
    static constexpr std::array<std::string_view, 2> kRegionFields{"gui", "id"};
    static constexpr std::array<std::string_view, 16> kPresentFields{"style", "transition", "dismissible", "modal", "color", "maxWidth", "maxHeight", "size", "detents", "detent", "grabber", "width", "anchor", "owner", "onDismiss", "onDetentChange"};
    static constexpr std::array<std::string_view, 7> kAlertFields{"style", "title", "message", "actions", "anchor", "owner", "onDismiss"};
    static constexpr std::array<std::string_view, 3> kActionFields{"id", "text", "role"};
    static constexpr std::array<std::string_view, 2> kDismissFields{"result", "transition"};
    static constexpr std::array<std::string_view, 3> kTransitionFields{"effect", "duration", "ease"};
    static constexpr std::array<std::string_view, 5> kMessageHandlerFields{"owner", "onShow", "onAction", "onTap", "onDismiss"};

    [[nodiscard]] static Navigation& getNavigation(lua_State* L);
    template <typename T> static void pushObject(lua_State* L, const std::shared_ptr<T>& object);

    // Reads the GUI and the node id of an area from the fields `gui` and `id` of the table at the index, naming the option in errors.
    [[nodiscard]] static Region readRegion(lua_State* L, int index, std::string_view option);
    [[nodiscard]] static NavigationTransition readTransition(lua_State* L, int index, const NavigationTransition& fallback);
    [[nodiscard]] static std::shared_ptr<Screen> readScreen(lua_State* L, int index);

    // Turns the function at the index into a factory that calls it for a screen whenever the navigation needs one.
    [[nodiscard]] static std::function<std::shared_ptr<Screen>()> readFactory(lua_State* L, int index, std::string_view option);

    // Takes the owner of the options table at the index, or the default owner, and pushes it, returning its index or 0.
    [[nodiscard]] static int pushOwner(lua_State* L, int options);

    // Reads the options of `ui.present` from the table at the index.
    static void readPresentOptions(lua_State* L, int index, Presentation::Options& options);
    [[nodiscard]] static std::vector<Presentation::Detent> readDetents(lua_State* L, int index);

    // Reads the function of an option of the table at the index and leaves it on the stack, returning its index, or returns 0 and pushes nothing when the option is missing.
    [[nodiscard]] static int pushHandler(lua_State* L, int options, const char* name);

    // Ties a presentation that Lua made to its owner, connects its handlers, keeps the promise of its result in its userdata and pushes the userdata.
    static void finishPresentation(lua_State* L, const std::shared_ptr<Presentation>& presentation, int owner, int dismissHandler, int detentHandler);

    // Pushes the promise of the result of the presentation or message userdata at the index, which settles once it left, making it the first time it is asked for.
    template <typename T> static void pushResult(lua_State* L, int index);

    // Whether a presentation or a message left and told it already, and what its promise resolves with: the result of a presentation and the reason of a message.
    [[nodiscard]] static bool hasLeft(const Navigation& navigation, const Presentation& presentation);
    [[nodiscard]] static bool hasLeft(const Navigation& navigation, const Message& message);
    [[nodiscard]] static core::Json getOutcome(const Presentation& presentation);
    [[nodiscard]] static core::Json getOutcome(const Message& message);

    // Reads the keys of the table at the index that the list names into the properties of a message.
    [[nodiscard]] static core::Json readMessageProperties(lua_State* L, int index, std::span<const std::string_view> keys);

    // Connects the handler at the index to a signal of a message for as long as the owner at its index lives, calling it with `{message, reason}`.
    static void connectMessage(lua_State* L, core::Signal<Message&>& signal, int handler, int owner);

    static int navigationStack(lua_State* L);
    static int navigationTabs(lua_State* L);
    static int navigationSplit(lua_State* L);
    static int present(lua_State* L);
    static int presentAlert(lua_State* L);
    static int showMessage(lua_State* L);
    static int goBack(lua_State* L);
    static int canGoBack(lua_State* L);
    static int screenState(lua_State* L);

    static int stackPush(lua_State* L);
    static int stackPop(lua_State* L);
    static int stackPopTo(lua_State* L);
    static int stackPopToRoot(lua_State* L);
    static int stackReplace(lua_State* L);
    static int stackSetScreens(lua_State* L);
    static int stackClose(lua_State* L);
    static int stackScreens(lua_State* L);
    static int stackTop(lua_State* L);
    static int stackSize(lua_State* L);
    static int stackCanGoBack(lua_State* L);
    static int stackTransitioning(lua_State* L);
    static int stackShown(lua_State* L);
    static int stackSetShown(lua_State* L);
    static int stackClosed(lua_State* L);

    static int tabsSelect(lua_State* L);
    static int tabsStack(lua_State* L);
    static int tabsClose(lua_State* L);
    static int tabsSelected(lua_State* L);
    static int tabsCurrent(lua_State* L);
    static int tabsClosed(lua_State* L);

    static int splitShowDetail(lua_State* L);
    static int splitClearDetail(lua_State* L);
    static int splitClose(lua_State* L);
    static int splitCollapsed(lua_State* L);
    static int splitSetCollapsed(lua_State* L);
    static int splitHasDetail(lua_State* L);
    static int splitPrimary(lua_State* L);
    static int splitDetail(lua_State* L);
    static int splitClosed(lua_State* L);

    static int presentationDismiss(lua_State* L);
    static int presentationScreen(lua_State* L);
    static int presentationStyle(lua_State* L);
    static int presentationDismissible(lua_State* L);
    static int presentationDismissed(lua_State* L);
    static int presentationModal(lua_State* L);
    static int presentationResult(lua_State* L);
    static int presentationDetent(lua_State* L);
    static int presentationSetDetent(lua_State* L);

    static int messageUpdate(lua_State* L);
    static int messageDismiss(lua_State* L);
    static int messageStyle(lua_State* L);
    static int messageShown(lua_State* L);
    static int messageDismissed(lua_State* L);
    static int messageResult(lua_State* L);
};

} // namespace haylen::ui
