#pragma once

#include <memory>
#include <string>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/core/ScopedConnection.hpp"
#include "haylen/ui/Navigation.hpp"
#include "haylen/ui/Screen.hpp"

namespace haylen::plugins {
class UiPlugin;
}

namespace haylen::ui {

// The screen of an alert or an action sheet that the navigation presents. An alert shows a dialog card with its actions as its answers, and an action sheet a column of buttons across its width with the cancel action last. Choosing an action dismisses the presentation with the id of the action as its result, and a person who dismisses it another way chooses its cancel action.
class AlertScreen final : public Screen {
  public:
    // Throws `std::invalid_argument` for no actions, an action without an id, a repeated id and more than one cancel action.
    AlertScreen(plugins::UiPlugin& plugin, Navigation::Alert shown);

    [[nodiscard]] const Navigation::Alert& getAlert() const noexcept {
        return alert;
    }

    [[nodiscard]] std::shared_ptr<Gui> build(core::Engine& engine, Placement placement) override;
    [[nodiscard]] bool back(core::Engine& engine) override;

  private:
    [[nodiscard]] core::Json buildAlertTree() const;
    [[nodiscard]] core::Json buildSheetTree() const;

    // The actions in the order they show: the cancel action first and then the others as the alert lists them.
    [[nodiscard]] std::vector<const Navigation::Alert::Action*> getOrderedActions() const;
    void choose(const std::string& id);

    plugins::UiPlugin& ui;
    Navigation::Alert alert;
    core::ScopedConnection events;
};

} // namespace haylen::ui
