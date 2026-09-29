#include "graphics/ResourceGraveyard.hpp"

namespace haylen::graphics {

void ResourceGraveyard::bury(sg_image image, sg_view view) {
    std::scoped_lock lock(mutex);
    images.push_back(image);
    views.push_back(view);
}

void ResourceGraveyard::buryView(sg_view view) {
    std::scoped_lock lock(mutex);
    views.push_back(view);
}

void ResourceGraveyard::buryBuffer(sg_buffer buffer) {
    std::scoped_lock lock(mutex);
    buffers.push_back(buffer);
}

void ResourceGraveyard::buryShader(sg_shader shader) {
    std::scoped_lock lock(mutex);
    shaders.push_back(shader);
}

void ResourceGraveyard::buryPipeline(sg_pipeline pipeline) {
    std::scoped_lock lock(mutex);
    pipelines.push_back(pipeline);
}

void ResourceGraveyard::collect() {
    std::vector<sg_image> deadImages;
    std::vector<sg_view> deadViews;
    std::vector<sg_buffer> deadBuffers;
    std::vector<sg_shader> deadShaders;
    std::vector<sg_pipeline> deadPipelines;
    {
        std::scoped_lock lock(mutex);
        deadImages.swap(images);
        deadViews.swap(views);
        deadBuffers.swap(buffers);
        deadShaders.swap(shaders);
        deadPipelines.swap(pipelines);
    }

    // Pipelines go before the shaders they were made from.
    for (const sg_pipeline pipeline : deadPipelines) {
        sg_destroy_pipeline(pipeline);
    }
    for (const sg_shader shader : deadShaders) {
        sg_destroy_shader(shader);
    }

    for (const sg_view view : deadViews) {
        sg_destroy_view(view);
    }
    for (const sg_image image : deadImages) {
        sg_destroy_image(image);
    }
    for (const sg_buffer buffer : deadBuffers) {
        sg_destroy_buffer(buffer);
    }
}

} // namespace haylen::graphics
