#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>

#include "haylen/core/ScopedConnection.hpp"
#include "haylen/ui/NavigationTransition.hpp"
#include "haylen/ui/Region.hpp"

namespace haylen::ui {

class Navigation;
class NavigationStack;
class Screen;

// A list and its detail, such as the conversations and the open chat. Expanded, as on a tablet, the primary stack shows the list in one area and the detail stack shows the detail in another, or a placeholder while nothing is picked. Collapsed, as on a phone, the detail screens move on top of the primary stack in its area, so the list and the detail work as one stack, and they move back to the detail area once it expands again. The app collapses the split when its layout leaves no room for the detail area.
class NavigationSplit final : public std::enable_shared_from_this<NavigationSplit> {
  public:
    using Factory = std::function<std::shared_ptr<Screen>()>;

    // Navigation creates splits, which register with it. The placeholder may be empty, and then the detail area stays empty while nothing is picked.
    NavigationSplit(Navigation& owner, Region primaryRegion, Region detailRegion, std::shared_ptr<Screen> root, Factory placeholderFactory, bool startCollapsed);
    ~NavigationSplit();

    NavigationSplit(const NavigationSplit&) = delete;
    NavigationSplit& operator=(const NavigationSplit&) = delete;

    // Shows a screen as the detail: in the detail area in place of the detail screens, at once unless a transition is given, or collapsed on top of the list with a slide, in place of the detail screens on it.
    void showDetail(std::shared_ptr<Screen> screen, const std::optional<NavigationTransition>& transition = std::nullopt);

    // Takes every detail screen away and shows the list, or the placeholder in the detail area.
    void clearDetail(const std::optional<NavigationTransition>& transition = std::nullopt);

    void setCollapsed(bool value);
    [[nodiscard]] bool isCollapsed() const noexcept {
        return collapsed;
    }

    // Whether a detail screen shows, other than the placeholder.
    [[nodiscard]] bool hasDetail() const noexcept;

    [[nodiscard]] const std::shared_ptr<NavigationStack>& getPrimary() const noexcept {
        return primary;
    }
    [[nodiscard]] const std::shared_ptr<NavigationStack>& getDetail() const noexcept {
        return detail;
    }

    // Closes both stacks, and the split takes no changes anymore.
    void close();
    [[nodiscard]] bool isClosed() const noexcept {
        return closed;
    }

  private:
    friend class Navigation;

    void checkOpen() const;
    void showPlaceholder();
    void dropPlaceholder();

    // Forgets where the detail starts on the primary stack once a change took the detail screens away.
    void followPrimary();

    Navigation& navigation;
    std::shared_ptr<NavigationStack> primary;
    std::shared_ptr<NavigationStack> detail;
    Factory placeholder;
    std::shared_ptr<Screen> placeholderScreen;
    core::ScopedConnection primaryChanged;

    // Collapsed, the index of the first detail screen on the primary stack.
    std::optional<std::size_t> detailStart;
    bool collapsed = false;
    bool closed = false;
};

} // namespace haylen::ui
