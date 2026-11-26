#pragma once

#include <cstddef>
#include <span>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::direct3d {

// The check of the registers that the HLSL of a program binds its uniform blocks, textures and samplers to, which the devices of Direct3D 11 and 12 share. Beyond the slots that every device checks, no two bindings of one kind and one stage share a register, which Direct3D 11 would bind one over the other and Direct3D 12 would refuse in the root signature.
class ShaderRegisters final {
  public:
    ShaderRegisters() = delete;

    // Throws `ContractError` of the rule `Program` for the slots that `ProgramSlots` refuses and for two bindings of one kind and one stage at one register.
    static void check(const Device::ProgramDesc& desc);

  private:
    template <typename Binding> [[nodiscard]] static bool shareRegisters(std::span<const Binding> bindings) noexcept {
        for (std::size_t index = 0; index < bindings.size(); ++index) {
            for (std::size_t other = index + 1; other < bindings.size(); ++other) {
                if (bindings[index].stage == bindings[other].stage && bindings[index].binding == bindings[other].binding) {
                    return true;
                }
            }
        }

        return false;
    }
};

} // namespace haylen::gpu::direct3d
