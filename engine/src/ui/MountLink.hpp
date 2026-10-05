#pragma once

#include <memory>

#include "haylen/core/Connection.hpp"

namespace haylen::plugins {
class UiPlugin;
}

namespace haylen::ui {

class Gui;

// Ties a mounted GUI to the owner it was mounted with: disconnecting unmounts the GUI and blocking hides it, and the link ends once the GUI is unmounted.
class MountLink final : public core::Connection::Link {
  public:
    MountLink(plugins::UiPlugin& owner, std::weak_ptr<Gui> value);

    void disconnect() override;
    [[nodiscard]] bool isConnected() const noexcept override;
    void setBlocked(bool value) override;
    [[nodiscard]] bool isBlocked() const noexcept override;

  private:
    plugins::UiPlugin& plugin;
    std::weak_ptr<Gui> gui;
};

} // namespace haylen::ui
