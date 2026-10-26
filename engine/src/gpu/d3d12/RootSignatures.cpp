#include "gpu/d3d12/RootSignatures.hpp"

#include <format>
#include <stdexcept>

#include "gpu/direct3d/HResult.hpp"

namespace haylen::gpu::d3d12 {

using Microsoft::WRL::ComPtr;

RootSignatures::RootSignatures(ID3D12Device& owner) : device(owner) {}

D3D12_SHADER_VISIBILITY RootSignatures::toVisibility(std::size_t stage) noexcept {
    return stage == 0 ? D3D12_SHADER_VISIBILITY_VERTEX : D3D12_SHADER_VISIBILITY_PIXEL;
}

RootSignatures::Layout RootSignatures::make(const Device::ProgramDesc& desc) {
    Layout layout;
    layout.blockParameters.fill(-1);
    std::vector<D3D12_ROOT_PARAMETER1> parameters;

    for (const Device::BlockBinding& block : desc.blocks) {
        if (block.slot < 0 || block.slot >= static_cast<int>(Device::kMaxBlocks) || block.size == 0 || block.binding < 0) {
            throw std::logic_error(std::format("The program \"{}\" has a uniform block at slot {} that the device cannot bind.", desc.label, block.slot));
        }
        D3D12_ROOT_PARAMETER1 parameter{};
        parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        parameter.Descriptor = {.ShaderRegister = static_cast<UINT>(block.binding), .RegisterSpace = 0, .Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE};
        parameter.ShaderVisibility = toVisibility(block.stage == Device::Stage::Vertex ? 0 : 1);
        layout.blockParameters[static_cast<std::size_t>(block.slot)] = static_cast<int>(parameters.size());
        layout.blockSizes[static_cast<std::size_t>(block.slot)] = block.size;
        parameters.push_back(parameter);
    }

    // The ranges of the four tables, which the parameters point into until the description is serialized.
    std::array<std::vector<D3D12_DESCRIPTOR_RANGE1>, 4> ranges;
    for (const Device::TextureBinding& texture : desc.textures) {
        if (texture.slot < 0 || texture.slot >= static_cast<int>(Device::kMaxTextures) || texture.binding < 0) {
            throw std::logic_error(std::format("The program \"{}\" has a texture at slot {} that the device cannot bind.", desc.label, texture.slot));
        }
        const std::size_t stage = texture.stage == Device::Stage::Vertex ? 0 : 1;
        layout.textures[stage].slots.push_back(texture.slot);
        ranges[stage].push_back({.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV, .NumDescriptors = 1, .BaseShaderRegister = static_cast<UINT>(texture.binding), .RegisterSpace = 0, .Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE, .OffsetInDescriptorsFromTableStart = static_cast<UINT>(ranges[stage].size())});
    }
    for (const Device::SamplerBinding& sampler : desc.samplers) {
        if (sampler.slot < 0 || sampler.slot >= static_cast<int>(Device::kMaxSamplers) || sampler.binding < 0) {
            throw std::logic_error(std::format("The program \"{}\" has a sampler at slot {} that the device cannot bind.", desc.label, sampler.slot));
        }
        const std::size_t stage = sampler.stage == Device::Stage::Vertex ? 0 : 1;
        layout.samplers[stage].slots.push_back(sampler.slot);
        ranges[2 + stage].push_back({.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, .NumDescriptors = 1, .BaseShaderRegister = static_cast<UINT>(sampler.binding), .RegisterSpace = 0, .Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE, .OffsetInDescriptorsFromTableStart = static_cast<UINT>(ranges[2 + stage].size())});
    }
    for (std::size_t table = 0; table < ranges.size(); ++table) {
        if (ranges[table].empty()) {
            continue;
        }
        Table& target = table < 2 ? layout.textures[table] : layout.samplers[table - 2];
        target.parameter = static_cast<int>(parameters.size());
        D3D12_ROOT_PARAMETER1 parameter{};
        parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameter.DescriptorTable = {.NumDescriptorRanges = static_cast<UINT>(ranges[table].size()), .pDescriptorRanges = ranges[table].data()};
        parameter.ShaderVisibility = toVisibility(table % 2);
        parameters.push_back(parameter);
    }

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC signature{};
    signature.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    signature.Desc_1_1.NumParameters = static_cast<UINT>(parameters.size());
    signature.Desc_1_1.pParameters = parameters.data();
    signature.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> errors;
    if (FAILED(D3D12SerializeVersionedRootSignature(&signature, serialized.GetAddressOf(), errors.GetAddressOf()))) {
        const std::string reason = errors != nullptr ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize()) : "Direct3D 12 refused it.";
        throw std::runtime_error(std::format("The root signature of the program \"{}\" could not be built: {}", desc.label, reason));
    }

    std::string key(static_cast<const char*>(serialized->GetBufferPointer()), serialized->GetBufferSize());
    ComPtr<ID3D12RootSignature>& shared = signatures[std::move(key)];
    if (shared == nullptr) {
        direct3d::HResult::check(device.CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(shared.GetAddressOf())), std::format("create the root signature of the program \"{}\"", desc.label));
    }
    layout.signature = shared;
    return layout;
}

} // namespace haylen::gpu::d3d12
