#include "gpu/SamplerRules.hpp"

#include <cmath>

#include "haylen/gpu/ContractError.hpp"

namespace haylen::gpu {

void SamplerRules::check(const Device::SamplerDesc& desc, const Device::Capabilities& capabilities) {
    const bool linear = desc.minification == Device::Filter::Linear && desc.magnification == Device::Filter::Linear && desc.mipmap == Device::Filter::Linear;
    if (desc.maxAnisotropy < 1 || desc.maxAnisotropy > capabilities.maxAnisotropy || (desc.maxAnisotropy > 1 && !linear)) {
        throw ContractError(ContractError::Rule::Sampler);
    }

    if (!(desc.minLod >= 0.0F) || !(desc.minLod <= desc.maxLod) || !(std::abs(desc.lodBias) <= capabilities.maxLodBias)) {
        throw ContractError(ContractError::Rule::Sampler);
    }

    if (readsBorder(desc) && !capabilities.borderColors) {
        throw ContractError(ContractError::Rule::Sampler);
    }
}

bool SamplerRules::readsBorder(const Device::SamplerDesc& desc) noexcept {
    return desc.wrapU == Device::Wrap::ClampToBorder || desc.wrapV == Device::Wrap::ClampToBorder || desc.wrapW == Device::Wrap::ClampToBorder;
}

} // namespace haylen::gpu
