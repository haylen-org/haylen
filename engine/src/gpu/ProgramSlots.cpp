#include "gpu/ProgramSlots.hpp"

#include "haylen/gpu/ContractError.hpp"

namespace haylen::gpu {

void ProgramSlots::check(const Device::ProgramDesc& desc) {
    for (const Device::BlockBinding& block : desc.blocks) {
        if (block.slot < 0 || block.slot >= static_cast<int>(Device::kMaxBlocks) || block.size == 0 || block.binding < 0) {
            throw ContractError(ContractError::Rule::Program);
        }
    }

    for (const Device::TextureBinding& texture : desc.textures) {
        if (texture.slot < 0 || texture.slot >= static_cast<int>(Device::kMaxTextures) || texture.binding < 0) {
            throw ContractError(ContractError::Rule::Program);
        }
    }

    for (const Device::SamplerBinding& sampler : desc.samplers) {
        if (sampler.slot < 0 || sampler.slot >= static_cast<int>(Device::kMaxSamplers) || sampler.binding < 0) {
            throw ContractError(ContractError::Rule::Program);
        }
    }

    if (shareSlots(desc.blocks) || shareSlots(desc.textures) || shareSlots(desc.samplers)) {
        throw ContractError(ContractError::Rule::Program);
    }
}

} // namespace haylen::gpu
