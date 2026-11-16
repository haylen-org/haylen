#pragma once

#include <memory>
#include <variant>

#include "haylen/core/Connection.hpp"

namespace haylen::ui {

class NavigationSplit;
class NavigationStack;
class NavigationTabs;
class Presentation;

// Ties a stack, tabs, a split or a presentation to the owner it was created with: disconnecting closes it, or dismisses a presentation at once, and the link ends once it closed. An owner that blocks leaves the navigation as it is, since its screens keep their state on their own.
class NavigationLink final : public core::Connection::Link {
  public:
    using Target = std::variant<std::weak_ptr<NavigationStack>, std::weak_ptr<NavigationTabs>, std::weak_ptr<NavigationSplit>, std::weak_ptr<Presentation>>;

    explicit NavigationLink(Target value);

    void disconnect() override;
    [[nodiscard]] bool isConnected() const noexcept override;
    void setBlocked(bool value) override;
    [[nodiscard]] bool isBlocked() const noexcept override;

  private:
    Target target;
};

} // namespace haylen::ui
