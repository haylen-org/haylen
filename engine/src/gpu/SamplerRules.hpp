#pragma once

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu {

// The check of the description of a sampler against the contract and the capabilities of a device, which every device runs before it makes a sampler: anisotropy within the device, with linear filters in every direction as WebGPU needs, levels of detail in order with a bias within the device, and border colors only on a device that reads them.
class SamplerRules final {
  public:
    SamplerRules() = delete;

    // Throws `ContractError` of the rule `Sampler` for a description that breaks the contract or reaches past the capabilities.
    static void check(const Device::SamplerDesc& desc, const Device::Capabilities& capabilities);

  private:
    [[nodiscard]] static bool readsBorder(const Device::SamplerDesc& desc) noexcept;
};

} // namespace haylen::gpu
