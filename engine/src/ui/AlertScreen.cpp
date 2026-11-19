#include "ui/AlertScreen.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Presentation.hpp"

namespace haylen::ui {

AlertScreen::AlertScreen(plugins::UiPlugin& plugin, Navigation::Alert shown) : ui(plugin), alert(std::move(shown)) {
    if (alert.actions.empty()) {
        throw std::invalid_argument("An alert needs at least one action.");
    }

    std::set<std::string, std::less<>> ids;
    for (const Navigation::Alert::Action& action : alert.actions) {
        if (action.id.empty()) {
            throw std::invalid_argument("Every action of an alert needs an id.");
        }
        if (!ids.insert(action.id).second) {
            throw std::invalid_argument("The alert uses the action id \"" + action.id + "\" more than once.");
        }
    }

    if (std::ranges::count(alert.actions, Navigation::Alert::Role::Cancel, &Navigation::Alert::Action::role) > 1) {
        throw std::invalid_argument("An alert has one cancel action at most.");
    }
}

std::vector<const Navigation::Alert::Action*> AlertScreen::getOrderedActions() const {
    std::vector<const Navigation::Alert::Action*> ordered;
    if (const Navigation::Alert::Action* cancel = alert.findCancel()) {
        ordered.push_back(cancel);
    }

    for (const Navigation::Alert::Action& action : alert.actions) {
        if (action.role != Navigation::Alert::Role::Cancel) {
            ordered.push_back(&action);
        }
    }

    return ordered;
}

// The answers of the dialog keep the cancel action first, so it sits at the start of a row and at the bottom of a stack, and the last default action is the preferred one, in the primary color.
core::Json AlertScreen::buildAlertTree() const {
    const auto preferred = std::ranges::find(alert.actions.rbegin(), alert.actions.rend(), Navigation::Alert::Role::Default, &Navigation::Alert::Action::role);
    core::Json buttons = core::Json::array();
    for (const Navigation::Alert::Action* action : getOrderedActions()) {
        std::string variant = "default";
        if (action->role == Navigation::Alert::Role::Destructive) {
            variant = "destructive";
        } else if (preferred != alert.actions.rend() && action == &*preferred) {
            variant = "primary";
        }

        buttons.push_back({{"id", action->id}, {"text", action->text}, {"variant", variant}});
    }

    core::Json tree = {{"kind", "dialog"}, {"buttons", std::move(buttons)}};
    if (!alert.title.is_null()) {
        tree["title"] = alert.title;
    }
    if (!alert.message.is_null()) {
        tree["message"] = alert.message;
    }

    return tree;
}

// The title and the message lead in muted text, and every action takes the whole width, with the cancel action last, under the others.
core::Json AlertScreen::buildSheetTree() const {
    core::Json children = core::Json::array();
    if (!alert.title.is_null()) {
        children.push_back({{"kind", "label"}, {"text", alert.title}, {"font", "button"}, {"color", "textMuted"}, {"textAlign", "center"}});
    }
    if (!alert.message.is_null()) {
        children.push_back({{"kind", "label"}, {"text", alert.message}, {"font", "caption"}, {"color", "textMuted"}, {"textAlign", "center"}});
    }

    const Navigation::Alert::Action* cancel = alert.findCancel();
    for (const Navigation::Alert::Action& action : alert.actions) {
        if (&action != cancel) {
            children.push_back({{"kind", "button"}, {"id", action.id}, {"text", action.text}, {"variant", action.role == Navigation::Alert::Role::Destructive ? "destructive" : "default"}, {"align", "stretch"}});
        }
    }

    if (cancel != nullptr) {
        children.push_back({{"kind", "button"}, {"id", cancel->id}, {"text", cancel->text}, {"variant", "default"}, {"align", "stretch"}});
    }

    return {{"kind", "column"}, {"padding", 28}, {"alignItems", "stretch"}, {"children", std::move(children)}};
}

// The screen hears the events of its own GUI, so the answer of the dialog or the press of a button chooses an action.
std::shared_ptr<Gui> AlertScreen::build(core::Engine&, Placement placement) {
    const core::Json tree = alert.style == Navigation::Alert::Style::Alert ? buildAlertTree() : buildSheetTree();
    std::shared_ptr<Gui> built = ui.createGui(tree, placement);
    const Gui* own = built.get();
    // clang-format off
    events = ui.events.connect([this, own](Gui& source, const Event& event) {
        if (&source != own) {
            return;
        }

        if (event.name == "answer") {
            choose(event.value.at("button").get<std::string>());
        } else if (event.name == "click") {
            choose(event.id);
        }
    });
    // clang-format on
    return built;
}

void AlertScreen::choose(const std::string& id) {
    if (const std::shared_ptr<Presentation> shown = getPresentation()) {
        shown->dismiss(id);
    }
}

// A dismissal by the person chooses the cancel action, so the presentation leaves with its id rather than with no result.
bool AlertScreen::back(core::Engine&) {
    const Navigation::Alert::Action* cancel = alert.findCancel();
    if (cancel == nullptr) {
        return true;
    }

    choose(cancel->id);
    return true;
}

} // namespace haylen::ui
