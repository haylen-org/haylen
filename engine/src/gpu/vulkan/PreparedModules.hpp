#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <utility>
#include <vector>

#include <volk.h>

namespace haylen::gpu::vulkan {

class Context;

// The shader modules that `prepareProgram` makes on the I/O pool ahead of their programs, which `makeProgram` takes instead of making them. Its methods run on any thread, and once the device closes it, the modules it holds go and later preparations do nothing.
class PreparedModules final {
  public:
    explicit PreparedModules(const Context& owner) noexcept : context(&owner) {}
    ~PreparedModules();
    PreparedModules(const PreparedModules&) = delete;
    PreparedModules& operator=(const PreparedModules&) = delete;

    void prepare(std::span<const std::uint32_t> code);

    // The module made for the code, or null when none was prepared.
    [[nodiscard]] VkShaderModule take(std::span<const std::uint32_t> code);

    void close();

  private:
    void destroyAll();

    std::mutex mutex;
    const Context* context = nullptr;
    std::vector<std::pair<std::vector<std::uint32_t>, VkShaderModule>> modules;
};

} // namespace haylen::gpu::vulkan
