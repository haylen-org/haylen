#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/StateRecorder.h>

#include <cstddef>
#include <cstdint>

namespace haylen::physics3d::jolt {

// Hashes the bytes of the state that a world saves with FNV-1a as Jolt writes them, so a state hash allocates nothing and covers every body, contact and constraint the world advances.
class StateHasher final : public JPH::StateRecorder {
  public:
    void WriteBytes(const void* data, std::size_t size) noexcept override;

    // Saving never reads, so a read only marks the recorder failed.
    void ReadBytes(void* data, std::size_t size) noexcept override;
    [[nodiscard]] bool IsEOF() const noexcept override {
        return true;
    }
    [[nodiscard]] bool IsFailed() const noexcept override {
        return failed;
    }

    [[nodiscard]] std::uint64_t getHash() const noexcept {
        return hash;
    }

  private:
    static constexpr std::uint64_t kOffsetBasis = 14695981039346656037ULL;
    static constexpr std::uint64_t kPrime = 1099511628211ULL;

    std::uint64_t hash = kOffsetBasis;
    bool failed = false;
};

} // namespace haylen::physics3d::jolt
