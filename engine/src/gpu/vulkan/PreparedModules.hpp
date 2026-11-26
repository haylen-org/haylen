#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <utility>
#include <vector>

#include <volk.h>

namespace haylen::gpu::vulkan {

class Context;

// The shader modules that `prepareProgram` makes on the I/O pool ahead of their programs, which `makeProgram` takes instead of making them. Its methods run on any thread, a module is made outside the lock, so the frame thread never waits for another module to be made, and once the device closes it, after the modules in the making are made, the modules it holds go and later preparations do nothing.
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
    std::condition_variable made;
    const Context* context = nullptr;

    // The code of every module that waits or is in the making, which holds a null module until it is made, and how many are in the making.
    std::vector<std::pair<std::vector<std::uint32_t>, VkShaderModule>> modules;
    int making = 0;
};

} // namespace haylen::gpu::vulkan
