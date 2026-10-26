#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::d3d12 {

// Builds the root signature of a program from its reflection: a root constant buffer view for each uniform block, which points into the upload memory of the frame, and for each stage a descriptor table of its textures and one of its samplers, with every register where the HLSL of the package puts it. Programs of the same layout share one root signature, so switching between their pipelines keeps every root argument.
class RootSignatures final {
  public:
    // A descriptor table of one stage, with the slots of the bindings in the order of its descriptors.
    struct Table {
        int parameter = -1;
        std::vector<int> slots;
    };

    struct Layout {
        Microsoft::WRL::ComPtr<ID3D12RootSignature> signature;
        std::array<int, Device::kMaxBlocks> blockParameters{};
        std::array<std::uint32_t, Device::kMaxBlocks> blockSizes{};
        std::array<Table, 2> textures;
        std::array<Table, 2> samplers;
    };

    explicit RootSignatures(ID3D12Device& owner);

    [[nodiscard]] Layout make(const Device::ProgramDesc& desc);

  private:
    [[nodiscard]] static D3D12_SHADER_VISIBILITY toVisibility(std::size_t stage) noexcept;

    ID3D12Device& device;
    // The root signatures under their serialized descriptions.
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>> signatures;
};

} // namespace haylen::gpu::d3d12
