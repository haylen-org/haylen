#include "content/SecureRandom.hpp"

#include <algorithm>
#include <stdexcept>

#if defined(_WIN32)
#include <windows.h>

#include <bcrypt.h>
#elif defined(__APPLE__)
#include <sys/random.h>
#else
#include <unistd.h>
#endif

namespace haylen::content {

void SecureRandom::fill(std::span<std::uint8_t> target) {
#if defined(_WIN32)
    if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, target.data(), static_cast<ULONG>(target.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
        throw std::runtime_error("The system could not provide random bytes for a key.");
    }
#else
    // The system hands out at most a fixed number of bytes per request.
    for (std::size_t offset = 0; offset < target.size(); offset += kMaximumRequest) {
        const std::size_t count = std::min(kMaximumRequest, target.size() - offset);
        if (getentropy(target.data() + offset, count) != 0) {
            throw std::runtime_error("The system could not provide random bytes for a key.");
        }
    }
#endif
}

} // namespace haylen::content
