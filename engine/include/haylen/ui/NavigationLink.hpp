#pragma once

#include <memory>
#include <variant>

#include "haylen/core/Connection.hpp"

namespace haylen::ui {

class Message;
class NavigationSplit;
class NavigationStack;
class NavigationTabs;
class Presentation;
class Router;

// Ties a stack, tabs, a split, a presentation, a message or a router to the owner it was created with: disconnecting closes it, or dismisses a presentation or a message at once, and the link ends once it closed. An owner that blocks leaves the navigation as it is, since its screens keep their state on their own.
class NavigationLink final : public core::Connection::Link {
  public:
    using Target = std::variant<std::weak_ptr<NavigationStack>, std::weak_ptr<NavigationTabs>, std::weak_ptr<NavigationSplit>, std::weak_ptr<Presentation>, std::weak_ptr<Message>, std::weak_ptr<Router>>;

    explicit NavigationLink(Target value);

    void disconnect() override;
    [[nodiscard]] bool isConnected() const noexcept override;
    void setBlocked(bool value) override;
    [[nodiscard]] bool isBlocked() const noexcept override;

  private:
    Target target;
};

} // namespace haylen::ui
