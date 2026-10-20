#include "haylen/content/KeyProvider.hpp"

namespace haylen::content {

void KeyProvider::requestKey(const KeyId&, std::function<void(bool)> done) const {
    done(false);
}

} // namespace haylen::content
