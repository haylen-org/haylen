#include "gpu/metal/PrecompiledLibraries.hpp"

#include <CommonCrypto/CommonDigest.h>

#include <algorithm>

namespace haylen::gpu::metal {

static_assert(PrecompiledLibraries::kKeySize == CC_SHA256_DIGEST_LENGTH);

const PrecompiledLibraries::Library* PrecompiledLibraries::find(std::string_view source) {
    std::array<std::uint8_t, kKeySize> key{};
    CC_SHA256(source.data(), static_cast<CC_LONG>(source.size()), key.data());

    const std::span<const Library> libraries = getLibraries();
    const auto found = std::ranges::lower_bound(libraries, key, {}, &Library::key);
    if (found == libraries.end() || found->key != key) {
        return nullptr;
    }
    return &*found;
}

} // namespace haylen::gpu::metal
