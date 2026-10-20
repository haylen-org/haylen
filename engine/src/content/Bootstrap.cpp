#include "haylen/content/Bootstrap.hpp"

#include <utility>
#include <vector>

#include "haylen/content/CompositeKeyProvider.hpp"

namespace haylen::content {

std::unique_ptr<const Bootstrap>& Bootstrap::getInstalled() noexcept {
    static auto* installed = new std::unique_ptr<const Bootstrap>();
    return *installed;
}

bool Bootstrap::install(Bootstrap bootstrap) {
    getInstalled() = std::make_unique<const Bootstrap>(std::move(bootstrap));
    return true;
}

void Bootstrap::addKeyProvider(std::shared_ptr<const KeyProvider> provider) {
    std::unique_ptr<const Bootstrap>& installed = getInstalled();
    if (!installed) {
        return;
    }
    Bootstrap next = *installed;
    next.keys = std::make_shared<const CompositeKeyProvider>(std::vector{std::move(next.keys), std::move(provider)});
    installed = std::make_unique<const Bootstrap>(std::move(next));
}

const Bootstrap* Bootstrap::find() noexcept {
    return getInstalled().get();
}

} // namespace haylen::content
