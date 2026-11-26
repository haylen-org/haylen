#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <string_view>
#include <vector>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::direct3d {
class ShaderCompiler;
}

namespace haylen::gpu::d3d12 {

// Fills the smaller mip levels of images on the GPU, which Direct3D 12 has no command for. Each layer of a mip level, and each slice of a mip level of a volume, takes a render pass of one triangle that covers it and samples the level above it with a linear filter at its own place, which averages the 2 by 2 texels above each texel of a flat image, an array or a cube, whose faces it reads as the layers of an array, and the 2 by 2 by 2 texels of a volume, which it samples halfway between the two slices above each slice. Its shaders compile on the threads of the shader compiler of the device from the moment it starts, so the first generation waits at most for them, and it makes the pipeline of each format once and keeps it.
class MipmapGenerator final {
  public:
    // A layer of a mip level, or a slice of a mip level of a volume, that a draw fills: the render target view of it, the table that holds the view of the level above it, its size in pixels, and the layer of the array or the depth of the slice in the volume that it samples there.
    struct Target {
        D3D12_CPU_DESCRIPTOR_HANDLE view{};
        D3D12_GPU_DESCRIPTOR_HANDLE source{};
        UINT width = 0;
        UINT height = 0;
        float layer = 0.0F;
    };

    MipmapGenerator(ID3D12Device& owner, direct3d::ShaderCompiler& shaders);
    MipmapGenerator(const MipmapGenerator&) = delete;
    MipmapGenerator& operator=(const MipmapGenerator&) = delete;
    ~MipmapGenerator() = default;

    // Sets the root signature and the pipeline of the levels of a volume or of the other types on the list, which the draws after it use. The format is the one that the views of the image read and write, so the filter of an sRGB image averages linear values.
    void begin(ID3D12GraphicsCommandList4& list, DXGI_FORMAT format, bool volume);
    void draw(ID3D12GraphicsCommandList4& list, const Target& target) const;

  private:
    static constexpr std::string_view kLabel = "haylen-mipmaps";
    static constexpr std::string_view kVertexProfile = "vs_5_0";
    static constexpr std::string_view kFragmentProfile = "ps_5_0";
    // The root constants fill one register of four values, whose first holds the layer or the depth that a draw samples.
    static constexpr UINT kConstants = 4;

    static constexpr std::string_view kVertexSource = R"(
struct Corner {
    float4 position : SV_Position;
    float2 place : TEXCOORD0;
};

Corner main(uint index : SV_VertexID) {
    Corner corner;
    corner.place = float2(float((index << 1) & 2), float(index & 2));
    corner.position = float4(corner.place.x * 2.0 - 1.0, 1.0 - corner.place.y * 2.0, 0.0, 1.0);
    return corner;
}
)";

    static constexpr std::string_view kLayerSource = R"(
cbuffer Source : register(b0) {
    float4 source;
};

Texture2DArray<float4> above : register(t0);
SamplerState linearClamp : register(s0);

struct Corner {
    float4 position : SV_Position;
    float2 place : TEXCOORD0;
};

float4 main(Corner corner) : SV_Target {
    return above.SampleLevel(linearClamp, float3(corner.place, source.x), 0.0);
}
)";

    static constexpr std::string_view kVolumeSource = R"(
cbuffer Source : register(b0) {
    float4 source;
};

Texture3D<float4> above : register(t0);
SamplerState linearClamp : register(s0);

struct Corner {
    float4 position : SV_Position;
    float2 place : TEXCOORD0;
};

float4 main(Corner corner) : SV_Target {
    return above.SampleLevel(linearClamp, float3(corner.place, source.x), 0.0);
}
)";

    struct Pipeline {
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        bool volume = false;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> state{};
    };

    [[nodiscard]] static gpu::Device::StageCode makeStage(std::string_view source, std::string_view profile) noexcept;
    void makeRootSignature();
    [[nodiscard]] ID3D12PipelineState& getPipeline(DXGI_FORMAT format, bool volume);

    ID3D12Device& device;
    direct3d::ShaderCompiler& compiler;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> vertexCode;
    Microsoft::WRL::ComPtr<ID3DBlob> layerCode;
    Microsoft::WRL::ComPtr<ID3DBlob> volumeCode;
    std::vector<Pipeline> pipelines;
};

} // namespace haylen::gpu::d3d12
