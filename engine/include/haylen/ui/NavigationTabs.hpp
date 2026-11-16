#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Signal.hpp"
#include "haylen/ui/Region.hpp"

namespace haylen::ui {

class Navigation;
class NavigationStack;
class Screen;

// Tabs that each keep a stack of their own in one area, such as the middle above a tab bar. Only the stack of the selected tab shows, and the others keep their screens with their scroll, focus and fields until their tab shows again. A tab builds its root screen the first time it shows. Selecting the tab that shows pops its stack to the root, and the system back on the root of another tab goes to the first tab.
class NavigationTabs final : public std::enable_shared_from_this<NavigationTabs> {
  public:
    using Factory = std::function<std::shared_ptr<Screen>()>;

    struct Tab {
        std::string id;

        // Makes the root screen of the tab the first time the tab shows.
        Factory root;
    };

    // Navigation creates tabs, which register with it. Throws `std::invalid_argument` for no tabs, an empty or repeated id, a tab without a root or a selected id that no tab has.
    NavigationTabs(Navigation& owner, Region region, std::vector<Tab> list, std::string_view initial);
    ~NavigationTabs();

    NavigationTabs(const NavigationTabs&) = delete;
    NavigationTabs& operator=(const NavigationTabs&) = delete;

    // Shows the stack of a tab and hides the stack that showed, or pops the stack of the tab that shows to its root. Throws `std::invalid_argument` for an id that no tab has.
    void select(std::string_view id);

    [[nodiscard]] const std::string& getSelected() const noexcept {
        return selected;
    }

    // Returns the stack of a tab, which holds its root screen from the first time the tab shows or this call asks for it.
    [[nodiscard]] const std::shared_ptr<NavigationStack>& getStack(std::string_view id);
    [[nodiscard]] const std::shared_ptr<NavigationStack>& getCurrent();

    // Closes the stack of every tab, and the tabs take no changes anymore.
    void close();
    [[nodiscard]] bool isClosed() const noexcept {
        return closed;
    }

    [[nodiscard]] const Region& getRegion() const noexcept {
        return region;
    }

    // Tells the selected tab and the tab that showed before, once the selection changed.
    core::Signal<const std::string&, const std::string&> selectionChanged;

  private:
    friend class Navigation;

    struct Entry {
        Tab tab;
        std::shared_ptr<NavigationStack> stack;
    };

    [[nodiscard]] Entry& find(std::string_view id);
    [[nodiscard]] const std::shared_ptr<NavigationStack>& open(Entry& entry);
    void checkOpen() const;

    // Selects the first tab while another one shows, which the system back does from the root of that tab.
    [[nodiscard]] bool goBackToFirst();

    Navigation& navigation;
    Region region;
    std::vector<Entry> entries;
    std::string selected;
    bool closed = false;
};

} // namespace haylen::ui
