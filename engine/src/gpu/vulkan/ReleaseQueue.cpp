#include "gpu/vulkan/ReleaseQueue.hpp"

#include <algorithm>

#include "gpu/vulkan/Context.hpp"

namespace haylen::gpu::vulkan {

ReleaseQueue::~ReleaseQueue() {
    clear();
}

void ReleaseQueue::push(Object object, std::uint64_t serial) {
    pending.emplace_back(serial, object);
}

void ReleaseQueue::collect(std::uint64_t completed) {
    const auto end = std::ranges::find_if(pending, [completed](const auto& entry) { return entry.first > completed; });
    for (auto entry = pending.begin(); entry != end; ++entry) {
        destroy(entry->second);
    }
    pending.erase(pending.begin(), end);
}

void ReleaseQueue::clear() {
    for (const auto& entry : pending) {
        destroy(entry.second);
    }
    pending.clear();
}

void ReleaseQueue::destroy(const Object& object) const {
    const VkDevice device = context.getDevice();
    const VolkDeviceTable& vk = context.getFunctions();
    if (const auto* buffer = std::get_if<Buffer>(&object)) {
        vmaDestroyBuffer(context.getAllocator(), buffer->buffer, buffer->allocation);
    } else if (const auto* image = std::get_if<Image>(&object)) {
        vmaDestroyImage(context.getAllocator(), image->image, image->allocation);
    } else if (const auto* view = std::get_if<ImageView>(&object)) {
        vk.vkDestroyImageView(device, view->view, nullptr);
    } else if (const auto* sampler = std::get_if<Sampler>(&object)) {
        vk.vkDestroySampler(device, sampler->sampler, nullptr);
    } else if (const auto* module = std::get_if<ShaderModule>(&object)) {
        vk.vkDestroyShaderModule(device, module->module, nullptr);
    } else if (const auto* pipeline = std::get_if<Pipeline>(&object)) {
        vk.vkDestroyPipeline(device, pipeline->pipeline, nullptr);
    } else if (const auto* framebuffer = std::get_if<Framebuffer>(&object)) {
        vk.vkDestroyFramebuffer(device, framebuffer->framebuffer, nullptr);
    } else if (const auto* set = std::get_if<DescriptorSet>(&object)) {
        vk.vkFreeDescriptorSets(device, set->pool, 1, &set->set);
    }
}

} // namespace haylen::gpu::vulkan
