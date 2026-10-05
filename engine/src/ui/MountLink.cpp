#include "ui/MountLink.hpp"

#include <utility>

#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Gui.hpp"

namespace haylen::ui {

MountLink::MountLink(plugins::UiPlugin& owner, std::weak_ptr<Gui> value) : plugin(owner), gui(std::move(value)) {}

void MountLink::disconnect() {
    if (const std::shared_ptr<Gui> mounted = gui.lock()) {
        plugin.unmount(*mounted);
    }
    gui.reset();
}

bool MountLink::isConnected() const noexcept {
    const std::shared_ptr<Gui> mounted = gui.lock();
    return mounted && plugin.isMounted(*mounted);
}

void MountLink::setBlocked(bool value) {
    if (const std::shared_ptr<Gui> mounted = gui.lock()) {
        mounted->setVisible(!value);
    }
}

bool MountLink::isBlocked() const noexcept {
    const std::shared_ptr<Gui> mounted = gui.lock();
    return mounted && !mounted->isVisible();
}

} // namespace haylen::ui
