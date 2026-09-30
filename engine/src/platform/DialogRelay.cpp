#include "platform/DialogRelay.hpp"

#include <utility>

#include "haylen/platform/Dialogs.hpp"

namespace haylen::platform {

std::mutex& DialogRelay::mutex = *new std::mutex();
Dialogs* DialogRelay::dialogs = nullptr;

void DialogRelay::attach(Dialogs& value) noexcept {
    const std::scoped_lock lock(mutex);
    dialogs = &value;
}

void DialogRelay::detach(const Dialogs& value) noexcept {
    const std::scoped_lock lock(mutex);
    if (dialogs == &value) {
        dialogs = nullptr;
    }
}

void DialogRelay::resolve(std::uint64_t id, DialogResult result) {
    const std::scoped_lock lock(mutex);
    if (dialogs != nullptr) {
        dialogs->resolve(id, std::move(result));
    }
}

} // namespace haylen::platform
