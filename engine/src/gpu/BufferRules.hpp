#pragma once

#include <cstddef>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu {

// The checks of buffers against the contract and the capabilities of a device, which every device runs: a buffer has a size and at least one usage, storage and indirect arguments only on a device that has them, a uniform buffer serves no other draw, an index buffer holds no vertices, an immutable buffer comes with its data and takes no copy, a stream buffer takes no copies, and writes, copies, readbacks and uniform ranges reach whole words inside buffers whose update and usages allow them.
class BufferRules final {
  public:
    // What a device keeps of the description of a buffer to check how the buffer is used.
    struct Description {
        std::size_t size = 0;
        Device::BufferUsage usage{};
        Device::BufferUpdate update = Device::BufferUpdate::Immutable;
    };

    BufferRules() = delete;

    // Throws `ContractError` of the rule `Buffer` for a description that breaks the contract or reaches past the capabilities, and returns what a device keeps of it.
    [[nodiscard]] static Description check(const Device::BufferDesc& desc, const Device::Capabilities& capabilities);

    // Throws `ContractError` of the rule `BufferWrite` for a write that the buffer does not take.
    static void checkWrite(const Description& buffer, std::size_t offset, std::size_t size);

    // Throws `ContractError` of the rule `BufferWrite` for a copy between ranges that the buffers do not hold, between a buffer and itself or between buffers whose usages do not allow it.
    static void checkCopy(Device::Buffer source, const Description& sourceBuffer, std::size_t sourceOffset, Device::Buffer destination, const Description& destinationBuffer, std::size_t destinationOffset, std::size_t size);

    // Throws `ContractError` of the rule `Readback` for a read of a range that the buffer does not hold or of a buffer that is no copy source.
    static void checkRead(const Description& buffer, std::size_t offset, std::size_t size);

    // Throws `ContractError` of the rule `Bindings` for a vertex or index buffer without that usage, and for a uniform buffer without that usage, at an offset that is no multiple of `Device::kUniformBufferAlignment` or that leaves no room for the block.
    static void checkVertex(const Description& buffer);
    static void checkIndex(const Description& buffer);
    static void checkUniform(const Description& buffer, std::size_t offset, std::size_t blockSize);

  private:
    [[nodiscard]] static bool holds(const Description& buffer, std::size_t offset, std::size_t size) noexcept;
    [[nodiscard]] static bool isWhole(std::size_t value) noexcept;
};

} // namespace haylen::gpu
