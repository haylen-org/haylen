#include "graphics/ResourceGraveyard.hpp"

namespace haylen::graphics {

void ResourceGraveyard::bury(gpu::Device::Image image) {
    std::scoped_lock lock(mutex);
    images.push_back(image);
}

void ResourceGraveyard::bury(gpu::Device::Buffer buffer) {
    std::scoped_lock lock(mutex);
    buffers.push_back(buffer);
}

void ResourceGraveyard::bury(gpu::Device::Program program) {
    std::scoped_lock lock(mutex);
    programs.push_back(program);
}

void ResourceGraveyard::bury(gpu::Device::Pipeline pipeline) {
    std::scoped_lock lock(mutex);
    pipelines.push_back(pipeline);
}

void ResourceGraveyard::collect(gpu::Device& device) {
    std::vector<gpu::Device::Image> deadImages;
    std::vector<gpu::Device::Buffer> deadBuffers;
    std::vector<gpu::Device::Program> deadPrograms;
    std::vector<gpu::Device::Pipeline> deadPipelines;
    {
        std::scoped_lock lock(mutex);
        deadImages.swap(images);
        deadBuffers.swap(buffers);
        deadPrograms.swap(programs);
        deadPipelines.swap(pipelines);
    }

    // Pipelines go before the programs they were made from.
    for (const gpu::Device::Pipeline pipeline : deadPipelines) {
        device.destroy(pipeline);
    }
    for (const gpu::Device::Program program : deadPrograms) {
        device.destroy(program);
    }
    for (const gpu::Device::Image image : deadImages) {
        device.destroy(image);
    }
    for (const gpu::Device::Buffer buffer : deadBuffers) {
        device.destroy(buffer);
    }
}

} // namespace haylen::graphics
