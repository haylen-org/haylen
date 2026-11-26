#include "gpu/BufferRules.hpp"

#include "haylen/gpu/ContractError.hpp"

namespace haylen::gpu {

BufferRules::Description BufferRules::check(const Device::BufferDesc& desc, const Device::Capabilities& capabilities) {
    const std::size_t size = desc.data.empty() ? desc.size : desc.data.size();
    const Device::BufferUsage& usage = desc.usage;
    const bool used = usage.vertex || usage.index || usage.uniform || usage.storage || usage.indirect || usage.copySource || usage.copyDestination;
    if (size == 0 || !used) {
        throw ContractError(ContractError::Rule::Buffer);
    }

    if ((usage.storage && !capabilities.storageBuffers) || (usage.indirect && !capabilities.indirectBuffers)) {
        throw ContractError(ContractError::Rule::Buffer);
    }

    // Direct3D 11 binds a constant buffer as nothing else, so a uniform buffer serves no other draw.
    if (usage.uniform && (usage.vertex || usage.index || usage.storage || usage.indirect)) {
        throw ContractError(ContractError::Rule::Buffer);
    }

    // WebGL2 binds a buffer of indices to no other target than copies, so an index buffer holds no vertices.
    if (usage.index && usage.vertex) {
        throw ContractError(ContractError::Rule::Buffer);
    }

    // An immutable buffer keeps the data it was made with, so it comes with that data and takes no copy into it.
    if (desc.update == Device::BufferUpdate::Immutable && (desc.data.empty() || usage.copyDestination)) {
        throw ContractError(ContractError::Rule::Buffer);
    }

    // Each frame holds its own contents of a stream buffer, which no copy between frames could name.
    if (desc.update == Device::BufferUpdate::Stream && (usage.copySource || usage.copyDestination)) {
        throw ContractError(ContractError::Rule::Buffer);
    }

    return {.size = size, .usage = usage, .update = desc.update};
}

void BufferRules::checkWrite(const Description& buffer, std::size_t offset, std::size_t size) {
    if (buffer.update == Device::BufferUpdate::Immutable || !isWhole(offset) || offset > buffer.size || size > buffer.size - offset) {
        throw ContractError(ContractError::Rule::BufferWrite);
    }

    if (buffer.update == Device::BufferUpdate::Dynamic && !isWhole(size)) {
        throw ContractError(ContractError::Rule::BufferWrite);
    }
}

void BufferRules::checkCopy(Device::Buffer source, const Description& sourceBuffer, std::size_t sourceOffset, Device::Buffer destination, const Description& destinationBuffer, std::size_t destinationOffset, std::size_t size) {
    if (!sourceBuffer.usage.copySource || !destinationBuffer.usage.copyDestination || source == destination) {
        throw ContractError(ContractError::Rule::BufferWrite);
    }

    if (!isWhole(sourceOffset) || !isWhole(destinationOffset) || !isWhole(size) || !holds(sourceBuffer, sourceOffset, size) || !holds(destinationBuffer, destinationOffset, size)) {
        throw ContractError(ContractError::Rule::BufferWrite);
    }
}

void BufferRules::checkRead(const Description& buffer, std::size_t offset, std::size_t size) {
    if (!buffer.usage.copySource || !isWhole(offset) || !isWhole(size) || !holds(buffer, offset, size)) {
        throw ContractError(ContractError::Rule::Readback);
    }
}

void BufferRules::checkVertex(const Description& buffer) {
    if (!buffer.usage.vertex) {
        throw ContractError(ContractError::Rule::Bindings);
    }
}

void BufferRules::checkIndex(const Description& buffer) {
    if (!buffer.usage.index) {
        throw ContractError(ContractError::Rule::Bindings);
    }
}

void BufferRules::checkUniform(const Description& buffer, std::size_t offset, std::size_t blockSize) {
    if (!buffer.usage.uniform || offset % Device::kUniformBufferAlignment != 0 || !holds(buffer, offset, blockSize)) {
        throw ContractError(ContractError::Rule::Bindings);
    }
}

bool BufferRules::holds(const Description& buffer, std::size_t offset, std::size_t size) noexcept {
    return size > 0 && offset <= buffer.size && size <= buffer.size - offset;
}

bool BufferRules::isWhole(std::size_t value) noexcept {
    return value % 4 == 0;
}

} // namespace haylen::gpu
