#include "haylen/content/CompositeKeyProvider.hpp"

#include <algorithm>
#include <utility>

namespace haylen::content {

CompositeKeyProvider::CompositeKeyProvider(std::vector<std::shared_ptr<const KeyProvider>> keyProviders) : providers(std::make_shared<const Providers>(std::move(keyProviders))) {}

std::vector<KeyProvider::KeyId> CompositeKeyProvider::getKeyIds() const {
    std::vector<KeyId> ids;
    for (const std::shared_ptr<const KeyProvider>& provider : *providers) {
        for (const KeyId& id : provider->getKeyIds()) {
            if (std::ranges::find(ids, id) == ids.end()) {
                ids.push_back(id);
            }
        }
    }
    return ids;
}

bool CompositeKeyProvider::getKey(const KeyId& id, Key& target) const {
    return std::ranges::any_of(*providers, [&id, &target](const std::shared_ptr<const KeyProvider>& provider) { return provider->getKey(id, target); });
}

void CompositeKeyProvider::requestKey(const KeyId& id, std::function<void(bool)> done) const {
    requestFrom(providers, 0, id, std::move(done));
}

// The providers answer in turn, and the first that gives the key ends the request.
void CompositeKeyProvider::requestFrom(std::shared_ptr<const Providers> list, std::size_t index, const KeyId& id, std::function<void(bool)> done) {
    if (index == list->size()) {
        done(false);
        return;
    }
    const std::shared_ptr<const KeyProvider>& provider = (*list)[index];
    // clang-format off
    provider->requestKey(id, [list, index, id, done = std::move(done)](bool given) mutable {
        if (given) {
            done(true);
            return;
        }
        requestFrom(std::move(list), index + 1, id, std::move(done));
    });
    // clang-format on
}

} // namespace haylen::content
