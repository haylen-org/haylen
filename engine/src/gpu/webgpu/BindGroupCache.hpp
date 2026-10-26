#pragma once

#include <webgpu/webgpu.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::webgpu {

// The bind groups of the textures and samplers of draws, kept by the layout they follow and the images and samplers they hold, so a frame that binds what an earlier frame bound creates nothing. Groups that no frame used for a while go away, and so do the groups of an image or a sampler that is destroyed.
class BindGroupCache final {
  public:
    // The frames a group stays unused before it goes away.
    static constexpr std::uint64_t kIdleFrames = 240;

    // A layout with the ids of the images at their texture slots and of the samplers at their sampler slots, zero where the program binds none.
    struct Key {
        WGPUBindGroupLayout layout = nullptr;
        std::array<std::uint32_t, Device::kMaxTextures> images{};
        std::array<std::uint32_t, Device::kMaxSamplers> samplers{};

        friend bool operator==(const Key&, const Key&) = default;
    };

    BindGroupCache() = default;
    BindGroupCache(const BindGroupCache&) = delete;
    BindGroupCache& operator=(const BindGroupCache&) = delete;
    ~BindGroupCache();

    // The group of the key, marked used in the frame, or none.
    [[nodiscard]] WGPUBindGroup find(const Key& key, std::uint64_t frame) noexcept;
    void add(const Key& key, WGPUBindGroup group, std::uint64_t frame);

    void forgetImage(std::uint32_t id) noexcept;
    void forgetSampler(std::uint32_t id) noexcept;

    // Releases the groups that no frame used for `kIdleFrames`, once in that many frames.
    void trim(std::uint64_t frame) noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t capacity() const noexcept;

  private:
    struct KeyHash {
        [[nodiscard]] std::size_t operator()(const Key& key) const noexcept;
    };

    struct Entry {
        WGPUBindGroup group = nullptr;
        std::uint64_t frame = 0;
    };

    template <typename Matches> void forgetWhere(Matches&& matches) noexcept;

    std::unordered_map<Key, Entry, KeyHash> groups;
    std::uint64_t lastTrim = 0;
};

} // namespace haylen::gpu::webgpu
