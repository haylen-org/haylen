#pragma once

#include <cstddef>
#include <span>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu {

// The check of the slots that a program binds its uniform blocks, textures and samplers at, which every device runs the same way before it reads them. Each binding names a slot of the contract and a binding number at zero or above, a block holds bytes, and no two bindings of one kind and one stage share a slot.
class ProgramSlots final {
  public:
    ProgramSlots() = delete;

    // Throws `ContractError` of the rule `Program` for a binding outside the slots of the contract, a binding number below zero, a block without bytes or two bindings of one kind and one stage at one slot.
    static void check(const Device::ProgramDesc& desc);

  private:
    template <typename Binding> [[nodiscard]] static bool shareSlots(std::span<const Binding> bindings) noexcept {
        for (std::size_t index = 0; index < bindings.size(); ++index) {
            for (std::size_t other = index + 1; other < bindings.size(); ++other) {
                if (bindings[index].stage == bindings[other].stage && bindings[index].slot == bindings[other].slot) {
                    return true;
                }
            }
        }

        return false;
    }
};

} // namespace haylen::gpu
