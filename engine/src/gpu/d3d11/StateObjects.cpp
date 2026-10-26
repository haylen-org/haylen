#include "gpu/d3d11/StateObjects.hpp"

#include <algorithm>

#include "gpu/direct3d/HResult.hpp"

namespace haylen::gpu::d3d11 {

using Microsoft::WRL::ComPtr;

ComPtr<ID3D11BlendState> StateObjects::makeBlend(ID3D11Device& device, const Device::PipelineDesc& desc) {
    D3D11_BLEND_DESC made{};
    made.AlphaToCoverageEnable = desc.alphaToCoverage ? TRUE : FALSE;
    made.IndependentBlendEnable = desc.colors.size() > 1 ? TRUE : FALSE;
    for (std::size_t index = 0; index < Device::kMaxColorTargets; ++index) {
        D3D11_RENDER_TARGET_BLEND_DESC& target = made.RenderTarget[index];
        if (index >= desc.colors.size()) {
            target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            target.SrcBlend = target.SrcBlendAlpha = D3D11_BLEND_ONE;
            target.DestBlend = target.DestBlendAlpha = D3D11_BLEND_ZERO;
            target.BlendOp = target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            continue;
        }
        const Device::ColorTarget& color = desc.colors[index];
        target.BlendEnable = color.blend.enabled ? TRUE : FALSE;
        target.SrcBlend = toBlend(color.blend.sourceColor);
        target.DestBlend = toBlend(color.blend.destinationColor);
        target.BlendOp = toBlendOperation(color.blend.colorOperation);
        target.SrcBlendAlpha = toAlphaBlend(color.blend.sourceAlpha);
        target.DestBlendAlpha = toAlphaBlend(color.blend.destinationAlpha);
        target.BlendOpAlpha = toBlendOperation(color.blend.alphaOperation);
        target.RenderTargetWriteMask = static_cast<UINT8>(color.writeMask);
    }

    ComPtr<ID3D11BlendState> state;
    direct3d::HResult::check(device.CreateBlendState(&made, state.GetAddressOf()), "create a blend state");
    return state;
}

// Multisampled targets cover triangles with every sample either way, and lines stay one pixel wide as on the other devices, which the quadrilateral lines of multisampling would widen.
ComPtr<ID3D11RasterizerState> StateObjects::makeRasterizer(ID3D11Device& device, const Device::PipelineDesc& desc) {
    D3D11_RASTERIZER_DESC made{};
    made.FillMode = D3D11_FILL_SOLID;
    made.CullMode = desc.cullMode == Device::CullMode::Front ? D3D11_CULL_FRONT : (desc.cullMode == Device::CullMode::Back ? D3D11_CULL_BACK : D3D11_CULL_NONE);
    made.FrontCounterClockwise = desc.frontFace == Device::FrontFace::CounterClockwise ? TRUE : FALSE;
    made.DepthBias = static_cast<INT>(desc.depthStencil.depthBias);
    made.SlopeScaledDepthBias = desc.depthStencil.depthBiasSlope;
    made.DepthClipEnable = TRUE;
    made.ScissorEnable = TRUE;

    ComPtr<ID3D11RasterizerState> state;
    direct3d::HResult::check(device.CreateRasterizerState(&made, state.GetAddressOf()), "create a rasterizer state");
    return state;
}

// A pipeline tests depth only where it draws into a depth attachment, the one of the swapchain or the one its format names, and a test that always passes without writing skips the depth buffer altogether.
ComPtr<ID3D11DepthStencilState> StateObjects::makeDepthStencil(ID3D11Device& device, const Device::PipelineDesc& desc) {
    const Device::DepthStencil& depth = desc.depthStencil;
    const bool attached = desc.swapchain || depth.format.has_value();

    D3D11_DEPTH_STENCIL_DESC made{};
    made.DepthEnable = attached && (depth.depthCompare != Device::CompareFunction::Always || depth.depthWrite) ? TRUE : FALSE;
    made.DepthWriteMask = depth.depthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    made.DepthFunc = toCompare(depth.depthCompare);
    made.StencilEnable = attached && depth.stencilEnabled ? TRUE : FALSE;
    made.StencilReadMask = depth.stencilReadMask;
    made.StencilWriteMask = depth.stencilWriteMask;
    made.FrontFace = toStencilFace(depth.stencilFront);
    made.BackFace = toStencilFace(depth.stencilBack);

    ComPtr<ID3D11DepthStencilState> state;
    direct3d::HResult::check(device.CreateDepthStencilState(&made, state.GetAddressOf()), "create a depth stencil state");
    return state;
}

ComPtr<ID3D11SamplerState> StateObjects::makeSampler(ID3D11Device& device, const Device::SamplerDesc& desc) {
    D3D11_SAMPLER_DESC made{};
    made.Filter = toFilter(desc);
    made.AddressU = toAddress(desc.wrapU);
    made.AddressV = toAddress(desc.wrapV);
    made.AddressW = toAddress(desc.wrapW);
    made.MaxAnisotropy = static_cast<UINT>(std::clamp(desc.maxAnisotropy, 1, D3D11_REQ_MAXANISOTROPY));
    made.ComparisonFunc = desc.compare ? toCompare(*desc.compare) : D3D11_COMPARISON_NEVER;
    made.MinLOD = desc.minLod;
    made.MaxLOD = desc.maxLod;

    ComPtr<ID3D11SamplerState> state;
    direct3d::HResult::check(device.CreateSamplerState(&made, state.GetAddressOf()), "create a sampler");
    return state;
}

D3D11_PRIMITIVE_TOPOLOGY StateObjects::toTopology(Device::Primitive primitive) noexcept {
    switch (primitive) {
    case Device::Primitive::Triangles:
        return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case Device::Primitive::TriangleStrip:
        return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    case Device::Primitive::Lines:
        return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    case Device::Primitive::LineStrip:
        return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
    case Device::Primitive::Points:
        return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
    }
    return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

D3D11_BLEND StateObjects::toBlend(Device::BlendFactor factor) noexcept {
    using BlendFactor = Device::BlendFactor;
    switch (factor) {
    case BlendFactor::Zero:
        return D3D11_BLEND_ZERO;
    case BlendFactor::One:
        return D3D11_BLEND_ONE;
    case BlendFactor::SourceColor:
        return D3D11_BLEND_SRC_COLOR;
    case BlendFactor::OneMinusSourceColor:
        return D3D11_BLEND_INV_SRC_COLOR;
    case BlendFactor::SourceAlpha:
        return D3D11_BLEND_SRC_ALPHA;
    case BlendFactor::OneMinusSourceAlpha:
        return D3D11_BLEND_INV_SRC_ALPHA;
    case BlendFactor::DestinationColor:
        return D3D11_BLEND_DEST_COLOR;
    case BlendFactor::OneMinusDestinationColor:
        return D3D11_BLEND_INV_DEST_COLOR;
    case BlendFactor::DestinationAlpha:
        return D3D11_BLEND_DEST_ALPHA;
    case BlendFactor::OneMinusDestinationAlpha:
        return D3D11_BLEND_INV_DEST_ALPHA;
    case BlendFactor::SourceAlphaSaturated:
        return D3D11_BLEND_SRC_ALPHA_SAT;
    case BlendFactor::BlendColor:
        return D3D11_BLEND_BLEND_FACTOR;
    case BlendFactor::OneMinusBlendColor:
        return D3D11_BLEND_INV_BLEND_FACTOR;
    }
    return D3D11_BLEND_ONE;
}

// Direct3D takes no factor of the color channels for alpha, so the factor of a color takes its alpha there, which is the value it gives the alpha channel.
D3D11_BLEND StateObjects::toAlphaBlend(Device::BlendFactor factor) noexcept {
    using BlendFactor = Device::BlendFactor;
    switch (factor) {
    case BlendFactor::SourceColor:
        return D3D11_BLEND_SRC_ALPHA;
    case BlendFactor::OneMinusSourceColor:
        return D3D11_BLEND_INV_SRC_ALPHA;
    case BlendFactor::DestinationColor:
        return D3D11_BLEND_DEST_ALPHA;
    case BlendFactor::OneMinusDestinationColor:
        return D3D11_BLEND_INV_DEST_ALPHA;
    default:
        return toBlend(factor);
    }
}

D3D11_BLEND_OP StateObjects::toBlendOperation(Device::BlendOperation operation) noexcept {
    switch (operation) {
    case Device::BlendOperation::Add:
        return D3D11_BLEND_OP_ADD;
    case Device::BlendOperation::Subtract:
        return D3D11_BLEND_OP_SUBTRACT;
    case Device::BlendOperation::ReverseSubtract:
        return D3D11_BLEND_OP_REV_SUBTRACT;
    case Device::BlendOperation::Minimum:
        return D3D11_BLEND_OP_MIN;
    case Device::BlendOperation::Maximum:
        return D3D11_BLEND_OP_MAX;
    }
    return D3D11_BLEND_OP_ADD;
}

D3D11_COMPARISON_FUNC StateObjects::toCompare(Device::CompareFunction function) noexcept {
    using CompareFunction = Device::CompareFunction;
    switch (function) {
    case CompareFunction::Never:
        return D3D11_COMPARISON_NEVER;
    case CompareFunction::Less:
        return D3D11_COMPARISON_LESS;
    case CompareFunction::Equal:
        return D3D11_COMPARISON_EQUAL;
    case CompareFunction::LessEqual:
        return D3D11_COMPARISON_LESS_EQUAL;
    case CompareFunction::Greater:
        return D3D11_COMPARISON_GREATER;
    case CompareFunction::NotEqual:
        return D3D11_COMPARISON_NOT_EQUAL;
    case CompareFunction::GreaterEqual:
        return D3D11_COMPARISON_GREATER_EQUAL;
    case CompareFunction::Always:
        return D3D11_COMPARISON_ALWAYS;
    }
    return D3D11_COMPARISON_ALWAYS;
}

D3D11_STENCIL_OP StateObjects::toStencilOperation(Device::StencilOperation operation) noexcept {
    using StencilOperation = Device::StencilOperation;
    switch (operation) {
    case StencilOperation::Keep:
        return D3D11_STENCIL_OP_KEEP;
    case StencilOperation::Zero:
        return D3D11_STENCIL_OP_ZERO;
    case StencilOperation::Replace:
        return D3D11_STENCIL_OP_REPLACE;
    case StencilOperation::IncrementClamp:
        return D3D11_STENCIL_OP_INCR_SAT;
    case StencilOperation::DecrementClamp:
        return D3D11_STENCIL_OP_DECR_SAT;
    case StencilOperation::Invert:
        return D3D11_STENCIL_OP_INVERT;
    case StencilOperation::IncrementWrap:
        return D3D11_STENCIL_OP_INCR;
    case StencilOperation::DecrementWrap:
        return D3D11_STENCIL_OP_DECR;
    }
    return D3D11_STENCIL_OP_KEEP;
}

D3D11_DEPTH_STENCILOP_DESC StateObjects::toStencilFace(const Device::StencilFace& face) noexcept {
    return {.StencilFailOp = toStencilOperation(face.fail), .StencilDepthFailOp = toStencilOperation(face.depthFail), .StencilPassOp = toStencilOperation(face.pass), .StencilFunc = toCompare(face.compare)};
}

D3D11_TEXTURE_ADDRESS_MODE StateObjects::toAddress(Device::Wrap wrap) noexcept {
    switch (wrap) {
    case Device::Wrap::ClampToEdge:
        return D3D11_TEXTURE_ADDRESS_CLAMP;
    case Device::Wrap::Repeat:
        return D3D11_TEXTURE_ADDRESS_WRAP;
    case Device::Wrap::MirroredRepeat:
        return D3D11_TEXTURE_ADDRESS_MIRROR;
    }
    return D3D11_TEXTURE_ADDRESS_CLAMP;
}

// The filters of Direct3D are bits: linear mipmaps, linear magnification, linear minification and comparison, with anisotropy filtering all three.
D3D11_FILTER StateObjects::toFilter(const Device::SamplerDesc& desc) noexcept {
    const UINT comparison = desc.compare ? 0x80U : 0U;
    if (desc.maxAnisotropy > 1) {
        return static_cast<D3D11_FILTER>(0x55U | comparison);
    }
    const UINT mipmap = desc.mipmap == Device::Filter::Linear ? 0x1U : 0U;
    const UINT magnification = desc.magnification == Device::Filter::Linear ? 0x4U : 0U;
    const UINT minification = desc.minification == Device::Filter::Linear ? 0x10U : 0U;
    return static_cast<D3D11_FILTER>(mipmap | magnification | minification | comparison);
}

} // namespace haylen::gpu::d3d11
