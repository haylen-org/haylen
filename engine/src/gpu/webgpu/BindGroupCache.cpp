#include "gpu/webgpu/BindGroupCache.hpp"

#include <algorithm>
#include <bit>

namespace haylen::gpu::webgpu {

BindGroupCache::~BindGroupCache() {
    for (const auto& [key, entry] : groups) {
        wgpuBindGroupRelease(entry.group);
    }
}

// The hash of FNV-1a over the layout and the ids.
std::size_t BindGroupCache::KeyHash::operator()(const Key& key) const noexcept {
    constexpr std::uint64_t kPrime = 1099511628211ULL;
    std::uint64_t hash = 14695981039346656037ULL;
    hash = (hash ^ static_cast<std::uint64_t>(std::bit_cast<std::uintptr_t>(key.layout))) * kPrime;
    for (const std::uint32_t id : key.images) {
        hash = (hash ^ id) * kPrime;
    }
    for (const std::uint32_t id : key.samplers) {
        hash = (hash ^ id) * kPrime;
    }
    return static_cast<std::size_t>(hash);
}

WGPUBindGroup BindGroupCache::find(const Key& key, std::uint64_t frame) noexcept {
    const auto found = groups.find(key);
    if (found == groups.end()) {
        return nullptr;
    }
    found->second.frame = frame;
    return found->second.group;
}

void BindGroupCache::add(const Key& key, WGPUBindGroup group, std::uint64_t frame) {
    groups.emplace(key, Entry{.group = group, .frame = frame});
}

template <typename Matches> void BindGroupCache::forgetWhere(Matches&& matches) noexcept {
    for (auto entry = groups.begin(); entry != groups.end();) {
        if (matches(entry->first, entry->second)) {
            wgpuBindGroupRelease(entry->second.group);
            entry = groups.erase(entry);
        } else {
            ++entry;
        }
    }
}

void BindGroupCache::forgetImage(std::uint32_t id) noexcept {
    forgetWhere([id](const Key& key, const Entry&) { return std::ranges::find(key.images, id) != key.images.end(); });
}

void BindGroupCache::forgetSampler(std::uint32_t id) noexcept {
    forgetWhere([id](const Key& key, const Entry&) { return std::ranges::find(key.samplers, id) != key.samplers.end(); });
}

void BindGroupCache::trim(std::uint64_t frame) noexcept {
    if (frame - lastTrim < kIdleFrames) {
        return;
    }
    lastTrim = frame;
    forgetWhere([frame](const Key&, const Entry& entry) { return frame - entry.frame > kIdleFrames; });
}

std::size_t BindGroupCache::size() const noexcept {
    return groups.size();
}

std::size_t BindGroupCache::capacity() const noexcept {
    return groups.bucket_count();
}

} // namespace haylen::gpu::webgpu
