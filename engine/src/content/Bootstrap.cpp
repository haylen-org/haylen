#include "haylen/content/Bootstrap.hpp"

#include <utility>

namespace haylen::content {

std::unique_ptr<const Bootstrap>& Bootstrap::getInstalled() noexcept {
    static auto* installed = new std::unique_ptr<const Bootstrap>();
    return *installed;
}

bool Bootstrap::install(Bootstrap bootstrap) {
    getInstalled() = std::make_unique<const Bootstrap>(std::move(bootstrap));
    return true;
}

const Bootstrap* Bootstrap::find() noexcept {
    return getInstalled().get();
}

} // namespace haylen::content
