#include "gpu/d3d12/MipmapGenerator.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <climits>
#include <format>
#include <stdexcept>
#include <string>
#include <utility>

#include "gpu/d3d12/StateDescriptions.hpp"
#include "gpu/direct3d/HResult.hpp"
#include "gpu/direct3d/ShaderCompiler.hpp"

namespace haylen::gpu::d3d12 {

using direct3d::HResult;
using Microsoft::WRL::ComPtr;

// The shaders start compiling at once on the threads of the compiler, which nothing waits for, so a device that goes away before its first generation never waits for them.
MipmapGenerator::MipmapGenerator(ID3D12Device& owner, direct3d::ShaderCompiler& shaders) : device(owner), compiler(shaders) {
    makeRootSignature();
    compiler.prepare({.vertex = makeStage(kVertexSource, kVertexProfile), .fragment = makeStage(kLayerSource, kFragmentProfile), .label = kLabel});
    compiler.prepare({.vertex = makeStage(kVertexSource, kVertexProfile), .fragment = makeStage(kVolumeSource, kFragmentProfile), .label = kLabel});
}

gpu::Device::StageCode MipmapGenerator::makeStage(std::string_view source, std::string_view profile) noexcept {
    return {.source = source, .entry = "main", .profile = profile};
}

// The draws read their layer or depth from root constants and the level above through a table of one view, with a linear sampler that clamps at the edges, so no texel of a neighbor layer or of the other side reaches the filter.
void MipmapGenerator::makeRootSignature() {
    const D3D12_DESCRIPTOR_RANGE1 range{.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV, .NumDescriptors = 1, .BaseShaderRegister = 0, .RegisterSpace = 0, .Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE, .OffsetInDescriptorsFromTableStart = 0};
    std::array<D3D12_ROOT_PARAMETER1, 2> parameters{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[0].Constants = {.ShaderRegister = 0, .RegisterSpace = 0, .Num32BitValues = kConstants};
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].DescriptorTable = {.NumDescriptorRanges = 1, .pDescriptorRanges = &range};
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0F;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC description{};
    description.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    description.Desc_1_1.NumParameters = static_cast<UINT>(parameters.size());
    description.Desc_1_1.pParameters = parameters.data();
    description.Desc_1_1.NumStaticSamplers = 1;
    description.Desc_1_1.pStaticSamplers = &sampler;
    description.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> errors;
    if (FAILED(D3D12SerializeVersionedRootSignature(&description, serialized.GetAddressOf(), errors.GetAddressOf()))) {
        const std::string reason = errors != nullptr ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize()) : "Direct3D 12 refused it.";
        throw std::runtime_error(std::format("The root signature that generates mip levels could not be built: {}", reason));
    }

    HResult::check(device.CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(signature.GetAddressOf())), "create the root signature that generates mip levels");
    signature->SetName(L"haylen-mipmaps");
}

// A pipeline of the defaults of a description draws without blending, culling or depth, into the one target of the format.
ID3D12PipelineState& MipmapGenerator::getPipeline(DXGI_FORMAT format, bool volume) {
    const auto found = std::ranges::find_if(pipelines, [format, volume](const Pipeline& pipeline) { return pipeline.format == format && pipeline.volume == volume; });
    if (found != pipelines.end()) {
        return *found->state.Get();
    }

    if (vertexCode == nullptr) {
        vertexCode = compiler.compile(makeStage(kVertexSource, kVertexProfile), kLabel);
    }

    ComPtr<ID3DBlob>& fragmentCode = volume ? volumeCode : layerCode;
    if (fragmentCode == nullptr) {
        fragmentCode = compiler.compile(makeStage(volume ? kVolumeSource : kLayerSource, kFragmentProfile), kLabel);
    }

    const gpu::Device::PipelineDesc plain{};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC state{};
    state.pRootSignature = signature.Get();
    state.VS = {.pShaderBytecode = vertexCode->GetBufferPointer(), .BytecodeLength = vertexCode->GetBufferSize()};
    state.PS = {.pShaderBytecode = fragmentCode->GetBufferPointer(), .BytecodeLength = fragmentCode->GetBufferSize()};
    state.BlendState = StateDescriptions::makeBlend(plain);
    state.SampleMask = UINT_MAX;
    state.RasterizerState = StateDescriptions::makeRasterizer(plain);
    state.DepthStencilState = StateDescriptions::makeDepthStencil(plain);
    state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    state.NumRenderTargets = 1;
    state.RTVFormats[0] = format;
    state.SampleDesc.Count = 1;

    ComPtr<ID3D12PipelineState> made;
    HResult::check(device.CreateGraphicsPipelineState(&state, IID_PPV_ARGS(made.GetAddressOf())), "create the pipeline that generates mip levels");
    made->SetName(L"haylen-mipmaps");
    pipelines.push_back({.format = format, .volume = volume, .state = std::move(made)});
    return *pipelines.back().state.Get();
}

void MipmapGenerator::begin(ID3D12GraphicsCommandList4& list, DXGI_FORMAT format, bool volume) {
    ID3D12PipelineState& pipeline = getPipeline(format, volume);
    list.SetGraphicsRootSignature(signature.Get());
    list.SetPipelineState(&pipeline);
    list.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// The draw covers the whole target, so its render pass discards what the target held before.
void MipmapGenerator::draw(ID3D12GraphicsCommandList4& list, const Target& target) const {
    D3D12_RENDER_PASS_RENDER_TARGET_DESC color{};
    color.cpuDescriptor = target.view;
    color.BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
    color.EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
    list.BeginRenderPass(1, &color, nullptr, D3D12_RENDER_PASS_FLAG_NONE);

    const D3D12_VIEWPORT viewport{.TopLeftX = 0.0F, .TopLeftY = 0.0F, .Width = static_cast<float>(target.width), .Height = static_cast<float>(target.height), .MinDepth = 0.0F, .MaxDepth = 1.0F};
    const D3D12_RECT scissor{.left = 0, .top = 0, .right = static_cast<LONG>(target.width), .bottom = static_cast<LONG>(target.height)};
    list.RSSetViewports(1, &viewport);
    list.RSSetScissorRects(1, &scissor);

    const std::array<UINT, kConstants> constants{std::bit_cast<UINT>(target.layer), 0, 0, 0};
    list.SetGraphicsRoot32BitConstants(0, kConstants, constants.data(), 0);
    list.SetGraphicsRootDescriptorTable(1, target.source);
    list.DrawInstanced(3, 1, 0, 0);
    list.EndRenderPass();
}

} // namespace haylen::gpu::d3d12
