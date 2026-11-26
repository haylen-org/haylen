#include "gpu/direct3d/ShaderRegisters.hpp"

#include "gpu/ProgramSlots.hpp"
#include "haylen/gpu/ContractError.hpp"

namespace haylen::gpu::direct3d {

void ShaderRegisters::check(const Device::ProgramDesc& desc) {
    ProgramSlots::check(desc);

    if (shareRegisters(desc.blocks) || shareRegisters(desc.textures) || shareRegisters(desc.samplers)) {
        throw ContractError(ContractError::Rule::Program);
    }
}

} // namespace haylen::gpu::direct3d
