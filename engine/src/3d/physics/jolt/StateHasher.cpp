#include "3d/physics/jolt/StateHasher.hpp"

#include <span>

namespace haylen::physics3d::jolt {

void StateHasher::WriteBytes(const void* data, std::size_t size) noexcept {
    const std::span<const std::byte> bytes(static_cast<const std::byte*>(data), size);
    for (const std::byte value : bytes) {
        hash ^= static_cast<std::uint64_t>(value);
        hash *= kPrime;
    }
}

void StateHasher::ReadBytes(void*, std::size_t) noexcept {
    failed = true;
}

} // namespace haylen::physics3d::jolt
