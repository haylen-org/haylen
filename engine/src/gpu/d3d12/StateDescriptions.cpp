#include "gpu/d3d12/StateDescriptions.hpp"

#include <algorithm>

namespace haylen::gpu::d3d12 {

D3D12_BLEND_DESC StateDescriptions::makeBlend(const Device::PipelineDesc& desc) noexcept {
    D3D12_BLEND_DESC made{};
    made.AlphaToCoverageEnable = desc.alphaToCoverage ? TRUE : FALSE;
    made.IndependentBlendEnable = desc.colors.size() > 1 ? TRUE : FALSE;
    for (std::size_t index = 0; index < Device::kMaxColorTargets; ++index) {
        D3D12_RENDER_TARGET_BLEND_DESC& target = made.RenderTarget[index];
        target.SrcBlend = target.SrcBlendAlpha = D3D12_BLEND_ONE;
        target.DestBlend = target.DestBlendAlpha = D3D12_BLEND_ZERO;
        target.BlendOp = target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        target.LogicOp = D3D12_LOGIC_OP_NOOP;
        target.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        if (index >= desc.colors.size()) {
            continue;
        }
        const Device::ColorTarget& color = desc.colors[index];
        target.BlendEnable = color.blend.enabled ? TRUE : FALSE;
        target.SrcBlend = toBlend(color.blend.sourceColor);
        target.DestBlend = toBlend(color.blend.destinationColor);
        target.BlendOp = toBlendOperation(color.blend.colorOperation);
        target.SrcBlendAlpha = toBlend(color.blend.sourceAlpha);
        target.DestBlendAlpha = toBlend(color.blend.destinationAlpha);
        target.BlendOpAlpha = toBlendOperation(color.blend.alphaOperation);
        target.RenderTargetWriteMask = static_cast<UINT8>(color.writeMask);
    }
    return made;
}

D3D12_RASTERIZER_DESC StateDescriptions::makeRasterizer(const Device::PipelineDesc& desc) noexcept {
    D3D12_RASTERIZER_DESC made{};
    made.FillMode = D3D12_FILL_MODE_SOLID;
    made.CullMode = desc.cullMode == Device::CullMode::Front ? D3D12_CULL_MODE_FRONT : (desc.cullMode == Device::CullMode::Back ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_NONE);
    made.FrontCounterClockwise = desc.frontFace == Device::FrontFace::CounterClockwise ? TRUE : FALSE;
    made.DepthBias = static_cast<INT>(desc.depthStencil.depthBias);
    made.SlopeScaledDepthBias = desc.depthStencil.depthBiasSlope;
    made.DepthClipEnable = TRUE;
    made.MultisampleEnable = FALSE;
    return made;
}

// A pipeline tests depth only where it draws into a depth attachment, the one of the swapchain or the one its format names, and a test that always passes without writing skips the depth buffer altogether.
D3D12_DEPTH_STENCIL_DESC StateDescriptions::makeDepthStencil(const Device::PipelineDesc& desc) noexcept {
    const Device::DepthStencil& depth = desc.depthStencil;
    const bool attached = desc.swapchain || depth.format.has_value();

    D3D12_DEPTH_STENCIL_DESC made{};
    made.DepthEnable = attached && (depth.depthCompare != Device::CompareFunction::Always || depth.depthWrite) ? TRUE : FALSE;
    made.DepthWriteMask = depth.depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    made.DepthFunc = toCompare(depth.depthCompare);
    made.StencilEnable = attached && depth.stencilEnabled ? TRUE : FALSE;
    made.StencilReadMask = depth.stencilReadMask;
    made.StencilWriteMask = depth.stencilWriteMask;
    made.FrontFace = toStencilFace(depth.stencilFront);
    made.BackFace = toStencilFace(depth.stencilBack);
    return made;
}

D3D12_SAMPLER_DESC StateDescriptions::makeSampler(const Device::SamplerDesc& desc) noexcept {
    D3D12_SAMPLER_DESC made{};
    made.Filter = toFilter(desc);
    made.AddressU = toAddress(desc.wrapU);
    made.AddressV = toAddress(desc.wrapV);
    made.AddressW = toAddress(desc.wrapW);
    made.MaxAnisotropy = static_cast<UINT>(std::clamp(desc.maxAnisotropy, 1, D3D12_REQ_MAXANISOTROPY));
    made.ComparisonFunc = desc.compare ? toCompare(*desc.compare) : D3D12_COMPARISON_FUNC_NEVER;
    made.MinLOD = desc.minLod;
    made.MaxLOD = desc.maxLod;
    return made;
}

D3D12_PRIMITIVE_TOPOLOGY StateDescriptions::toTopology(Device::Primitive primitive) noexcept {
    switch (primitive) {
    case Device::Primitive::Triangles:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case Device::Primitive::TriangleStrip:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    case Device::Primitive::Lines:
        return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
    case Device::Primitive::LineStrip:
        return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
    case Device::Primitive::Points:
        return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
    }
    return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE StateDescriptions::toTopologyType(Device::Primitive primitive) noexcept {
    switch (primitive) {
    case Device::Primitive::Triangles:
    case Device::Primitive::TriangleStrip:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    case Device::Primitive::Lines:
    case Device::Primitive::LineStrip:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    case Device::Primitive::Points:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    }
    return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
}

D3D12_BLEND StateDescriptions::toBlend(Device::BlendFactor factor) noexcept {
    using BlendFactor = Device::BlendFactor;
    switch (factor) {
    case BlendFactor::Zero:
        return D3D12_BLEND_ZERO;
    case BlendFactor::One:
        return D3D12_BLEND_ONE;
    case BlendFactor::SourceColor:
        return D3D12_BLEND_SRC_COLOR;
    case BlendFactor::OneMinusSourceColor:
        return D3D12_BLEND_INV_SRC_COLOR;
    case BlendFactor::SourceAlpha:
        return D3D12_BLEND_SRC_ALPHA;
    case BlendFactor::OneMinusSourceAlpha:
        return D3D12_BLEND_INV_SRC_ALPHA;
    case BlendFactor::DestinationColor:
        return D3D12_BLEND_DEST_COLOR;
    case BlendFactor::OneMinusDestinationColor:
        return D3D12_BLEND_INV_DEST_COLOR;
    case BlendFactor::DestinationAlpha:
        return D3D12_BLEND_DEST_ALPHA;
    case BlendFactor::OneMinusDestinationAlpha:
        return D3D12_BLEND_INV_DEST_ALPHA;
    case BlendFactor::SourceAlphaSaturated:
        return D3D12_BLEND_SRC_ALPHA_SAT;
    case BlendFactor::BlendColor:
        return D3D12_BLEND_BLEND_FACTOR;
    case BlendFactor::OneMinusBlendColor:
        return D3D12_BLEND_INV_BLEND_FACTOR;
    }
    return D3D12_BLEND_ONE;
}

D3D12_BLEND_OP StateDescriptions::toBlendOperation(Device::BlendOperation operation) noexcept {
    switch (operation) {
    case Device::BlendOperation::Add:
        return D3D12_BLEND_OP_ADD;
    case Device::BlendOperation::Subtract:
        return D3D12_BLEND_OP_SUBTRACT;
    case Device::BlendOperation::ReverseSubtract:
        return D3D12_BLEND_OP_REV_SUBTRACT;
    case Device::BlendOperation::Minimum:
        return D3D12_BLEND_OP_MIN;
    case Device::BlendOperation::Maximum:
        return D3D12_BLEND_OP_MAX;
    }
    return D3D12_BLEND_OP_ADD;
}

D3D12_COMPARISON_FUNC StateDescriptions::toCompare(Device::CompareFunction function) noexcept {
    using CompareFunction = Device::CompareFunction;
    switch (function) {
    case CompareFunction::Never:
        return D3D12_COMPARISON_FUNC_NEVER;
    case CompareFunction::Less:
        return D3D12_COMPARISON_FUNC_LESS;
    case CompareFunction::Equal:
        return D3D12_COMPARISON_FUNC_EQUAL;
    case CompareFunction::LessEqual:
        return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case CompareFunction::Greater:
        return D3D12_COMPARISON_FUNC_GREATER;
    case CompareFunction::NotEqual:
        return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    case CompareFunction::GreaterEqual:
        return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    case CompareFunction::Always:
        return D3D12_COMPARISON_FUNC_ALWAYS;
    }
    return D3D12_COMPARISON_FUNC_ALWAYS;
}

D3D12_STENCIL_OP StateDescriptions::toStencilOperation(Device::StencilOperation operation) noexcept {
    using StencilOperation = Device::StencilOperation;
    switch (operation) {
    case StencilOperation::Keep:
        return D3D12_STENCIL_OP_KEEP;
    case StencilOperation::Zero:
        return D3D12_STENCIL_OP_ZERO;
    case StencilOperation::Replace:
        return D3D12_STENCIL_OP_REPLACE;
    case StencilOperation::IncrementClamp:
        return D3D12_STENCIL_OP_INCR_SAT;
    case StencilOperation::DecrementClamp:
        return D3D12_STENCIL_OP_DECR_SAT;
    case StencilOperation::Invert:
        return D3D12_STENCIL_OP_INVERT;
    case StencilOperation::IncrementWrap:
        return D3D12_STENCIL_OP_INCR;
    case StencilOperation::DecrementWrap:
        return D3D12_STENCIL_OP_DECR;
    }
    return D3D12_STENCIL_OP_KEEP;
}

D3D12_DEPTH_STENCILOP_DESC StateDescriptions::toStencilFace(const Device::StencilFace& face) noexcept {
    return {.StencilFailOp = toStencilOperation(face.fail), .StencilDepthFailOp = toStencilOperation(face.depthFail), .StencilPassOp = toStencilOperation(face.pass), .StencilFunc = toCompare(face.compare)};
}

D3D12_TEXTURE_ADDRESS_MODE StateDescriptions::toAddress(Device::Wrap wrap) noexcept {
    switch (wrap) {
    case Device::Wrap::ClampToEdge:
        return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    case Device::Wrap::Repeat:
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    case Device::Wrap::MirroredRepeat:
        return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    }
    return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
}

// The filters of Direct3D are bits: linear mipmaps, linear magnification, linear minification and comparison, with anisotropy filtering all three.
D3D12_FILTER StateDescriptions::toFilter(const Device::SamplerDesc& desc) noexcept {
    const UINT comparison = desc.compare ? 0x80U : 0U;
    if (desc.maxAnisotropy > 1) {
        return static_cast<D3D12_FILTER>(0x55U | comparison);
    }
    const UINT mipmap = desc.mipmap == Device::Filter::Linear ? 0x1U : 0U;
    const UINT magnification = desc.magnification == Device::Filter::Linear ? 0x4U : 0U;
    const UINT minification = desc.minification == Device::Filter::Linear ? 0x10U : 0U;
    return static_cast<D3D12_FILTER>(mipmap | magnification | minification | comparison);
}

} // namespace haylen::gpu::d3d12
