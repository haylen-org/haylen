#include "ui/MountLink.hpp"

#include <utility>

#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Document.hpp"

namespace haylen::ui {

MountLink::MountLink(plugins::UiPlugin& owner, std::weak_ptr<Document> value) : plugin(owner), document(std::move(value)) {}

void MountLink::disconnect() {
    if (const std::shared_ptr<Document> mounted = document.lock()) {
        plugin.unmount(*mounted);
    }
    document.reset();
}

bool MountLink::isConnected() const noexcept {
    const std::shared_ptr<Document> mounted = document.lock();
    return mounted && plugin.isMounted(*mounted);
}

void MountLink::setBlocked(bool value) {
    if (const std::shared_ptr<Document> mounted = document.lock()) {
        mounted->setVisible(!value);
    }
}

bool MountLink::isBlocked() const noexcept {
    const std::shared_ptr<Document> mounted = document.lock();
    return mounted && !mounted->isVisible();
}

} // namespace haylen::ui
