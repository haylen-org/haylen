#include "backends/sokol/GpuDevice.hpp"

#include <bit>
#include <chrono>
#include <deque>
#include <format>
#include <stdexcept>
#include <utility>

#include "backends/sokol/Adapter.hpp"
#include "backends/sokol/ImageReadback.hpp"
#include "backends/sokol/Precompiler.hpp"
#include "haylen/core/Log.hpp"

namespace haylen::sokol {

GpuDevice::GpuDevice(Setup setup) : swapchain(std::move(setup.swapchain)) {
    if (sg_isvalid()) {
        throw std::logic_error("Only one graphics device can exist at a time.");
    }
    sg_desc desc{};
    desc.environment = setup.environment;
    desc.logger = setup.logger;
    desc.disable_validation = !setup.validation;
    desc.image_pool_size = kImagePoolSize;
    desc.view_pool_size = kViewPoolSize;
    desc.buffer_pool_size = kBufferPoolSize;
    desc.sampler_pool_size = kSamplerPoolSize;
    desc.shader_pool_size = kShaderPoolSize;
    desc.pipeline_pool_size = kPipelinePoolSize;
    sg_setup(&desc);
    if (!sg_isvalid()) {
        throw std::runtime_error("The graphics device could not be initialized.");
    }
}

GpuDevice::~GpuDevice() {
    if (staging.id != SG_INVALID_ID) {
        sg_destroy_buffer(staging);
    }
    sg_shutdown();
}

double GpuDevice::millisecondsSince(std::chrono::steady_clock::time_point start) noexcept {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

sg_pixel_format GpuDevice::toPixelFormat(PixelFormat format) noexcept {
    switch (format) {
    case PixelFormat::R8:
        return SG_PIXELFORMAT_R8;
    case PixelFormat::Rgba8:
        return SG_PIXELFORMAT_RGBA8;
    case PixelFormat::R32Float:
        return SG_PIXELFORMAT_R32F;
    case PixelFormat::Rgba16Float:
        return SG_PIXELFORMAT_RGBA16F;
    }
    return SG_PIXELFORMAT_RGBA8;
}

std::size_t GpuDevice::getBytesPerPixel(PixelFormat format) noexcept {
    switch (format) {
    case PixelFormat::R8:
        return 1;
    case PixelFormat::Rgba8:
    case PixelFormat::R32Float:
        return 4;
    case PixelFormat::Rgba16Float:
        return 8;
    }
    return 4;
}

sg_vertex_format GpuDevice::toVertexFormat(VertexFormat format) noexcept {
    switch (format) {
    case VertexFormat::Float:
        return SG_VERTEXFORMAT_FLOAT;
    case VertexFormat::Float2:
        return SG_VERTEXFORMAT_FLOAT2;
    case VertexFormat::Float3:
        return SG_VERTEXFORMAT_FLOAT3;
    case VertexFormat::Float4:
        return SG_VERTEXFORMAT_FLOAT4;
    case VertexFormat::Half2:
        return SG_VERTEXFORMAT_HALF2;
    case VertexFormat::UByte4Normalized:
        return SG_VERTEXFORMAT_UBYTE4N;
    case VertexFormat::UShort4Normalized:
        return SG_VERTEXFORMAT_USHORT4N;
    }
    return SG_VERTEXFORMAT_FLOAT;
}

sg_blend_factor GpuDevice::toBlendFactor(BlendFactor factor) noexcept {
    switch (factor) {
    case BlendFactor::Zero:
        return SG_BLENDFACTOR_ZERO;
    case BlendFactor::One:
        return SG_BLENDFACTOR_ONE;
    case BlendFactor::SourceColor:
        return SG_BLENDFACTOR_SRC_COLOR;
    case BlendFactor::OneMinusSourceColor:
        return SG_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
    case BlendFactor::SourceAlpha:
        return SG_BLENDFACTOR_SRC_ALPHA;
    case BlendFactor::OneMinusSourceAlpha:
        return SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    case BlendFactor::DestinationColor:
        return SG_BLENDFACTOR_DST_COLOR;
    case BlendFactor::OneMinusDestinationColor:
        return SG_BLENDFACTOR_ONE_MINUS_DST_COLOR;
    case BlendFactor::DestinationAlpha:
        return SG_BLENDFACTOR_DST_ALPHA;
    case BlendFactor::OneMinusDestinationAlpha:
        return SG_BLENDFACTOR_ONE_MINUS_DST_ALPHA;
    }
    return SG_BLENDFACTOR_ONE;
}

sg_blend_op GpuDevice::toBlendOperation(BlendOperation operation) noexcept {
    switch (operation) {
    case BlendOperation::Add:
        return SG_BLENDOP_ADD;
    case BlendOperation::Subtract:
        return SG_BLENDOP_SUBTRACT;
    case BlendOperation::ReverseSubtract:
        return SG_BLENDOP_REVERSE_SUBTRACT;
    case BlendOperation::Minimum:
        return SG_BLENDOP_MIN;
    case BlendOperation::Maximum:
        return SG_BLENDOP_MAX;
    }
    return SG_BLENDOP_ADD;
}

// Sokol reads a mask of no channels as its default of every channel, and names writing none apart.
sg_color_mask GpuDevice::toColorMask(ColorMask mask) noexcept {
    return mask == ColorMask::None ? SG_COLORMASK_NONE : static_cast<sg_color_mask>(static_cast<int>(mask));
}

sg_filter GpuDevice::toFilter(Filter filter) noexcept {
    return filter == Filter::Nearest ? SG_FILTER_NEAREST : SG_FILTER_LINEAR;
}

sg_wrap GpuDevice::toWrap(Wrap wrap) noexcept {
    switch (wrap) {
    case Wrap::ClampToEdge:
        return SG_WRAP_CLAMP_TO_EDGE;
    case Wrap::Repeat:
        return SG_WRAP_REPEAT;
    case Wrap::MirroredRepeat:
        return SG_WRAP_MIRRORED_REPEAT;
    }
    return SG_WRAP_CLAMP_TO_EDGE;
}

sg_shader_stage GpuDevice::toStage(Stage stage) noexcept {
    return stage == Stage::Vertex ? SG_SHADERSTAGE_VERTEX : SG_SHADERSTAGE_FRAGMENT;
}

sg_image_type GpuDevice::toImageType(TextureType type) noexcept {
    switch (type) {
    case TextureType::Flat:
        return SG_IMAGETYPE_2D;
    case TextureType::Array:
        return SG_IMAGETYPE_ARRAY;
    case TextureType::Volume:
        return SG_IMAGETYPE_3D;
    case TextureType::Cube:
        return SG_IMAGETYPE_CUBE;
    }
    return SG_IMAGETYPE_2D;
}

sg_image_sample_type GpuDevice::toSampleType(SampleType type) noexcept {
    switch (type) {
    case SampleType::Float:
        return SG_IMAGESAMPLETYPE_FLOAT;
    case SampleType::UnfilterableFloat:
        return SG_IMAGESAMPLETYPE_UNFILTERABLE_FLOAT;
    case SampleType::Depth:
        return SG_IMAGESAMPLETYPE_DEPTH;
    case SampleType::SignedInteger:
        return SG_IMAGESAMPLETYPE_SINT;
    case SampleType::UnsignedInteger:
        return SG_IMAGESAMPLETYPE_UINT;
    }
    return SG_IMAGESAMPLETYPE_FLOAT;
}

sg_sampler_type GpuDevice::toSamplerType(SamplerType type) noexcept {
    switch (type) {
    case SamplerType::Filtering:
        return SG_SAMPLERTYPE_FILTERING;
    case SamplerType::NonFiltering:
        return SG_SAMPLERTYPE_NONFILTERING;
    case SamplerType::Comparison:
        return SG_SAMPLERTYPE_COMPARISON;
    }
    return SG_SAMPLERTYPE_FILTERING;
}

sg_color GpuDevice::toColor(math::Color color) noexcept {
    return {color.r, color.g, color.b, color.a};
}

gpu::Device::Language GpuDevice::getLanguage() const noexcept {
    switch (sg_query_backend()) {
    case SG_BACKEND_METAL_MACOS:
        return Language::MetalMacos;
    case SG_BACKEND_METAL_IOS:
        return Language::MetalIos;
    case SG_BACKEND_METAL_SIMULATOR:
        return Language::MetalIosSimulator;
    case SG_BACKEND_D3D11:
        return Language::Hlsl5;
    case SG_BACKEND_GLCORE:
        return Language::Glsl430;
    case SG_BACKEND_GLES3:
        return Language::Glsl300es;
    case SG_BACKEND_WGPU:
        return Language::Wgsl;
    case SG_BACKEND_VULKAN:
    case SG_BACKEND_DUMMY:
        break;
    }
    return Language::None;
}

// The OpenGL ES 3 of the web is WebGL 2.
gpu::Device::Api GpuDevice::getApi() const noexcept {
    switch (sg_query_backend()) {
    case SG_BACKEND_GLCORE:
        return Api::OpenGl;
    case SG_BACKEND_GLES3:
#if defined(__EMSCRIPTEN__)
        return Api::WebGl2;
#else
        return Api::OpenGlEs;
#endif
    case SG_BACKEND_D3D11:
        return Api::Direct3D11;
    case SG_BACKEND_METAL_IOS:
    case SG_BACKEND_METAL_MACOS:
    case SG_BACKEND_METAL_SIMULATOR:
        return Api::Metal;
    case SG_BACKEND_WGPU:
        return Api::WebGpu;
    case SG_BACKEND_VULKAN:
    case SG_BACKEND_DUMMY:
        break;
    }
    return Api::None;
}

std::string GpuDevice::getAdapterName() const {
    return Adapter::getName();
}

int GpuDevice::getMaxImageSize() const noexcept {
    return sg_query_limits().max_image_size_2d;
}

bool GpuDevice::isOriginTopLeft() const noexcept {
    return sg_query_features().origin_top_left;
}

bool GpuDevice::canBlendInto(PixelFormat format) const noexcept {
    const sg_pixelformat_info info = sg_query_pixelformat(toPixelFormat(format));
    return info.render && info.blend;
}

std::optional<double> GpuDevice::getGpuFrameSeconds() const noexcept {
    const sg_backend backend = sg_query_backend();
    if (backend != SG_BACKEND_METAL_MACOS && backend != SG_BACKEND_METAL_IOS && backend != SG_BACKEND_METAL_SIMULATOR) {
        return std::nullopt;
    }
    const std::uint64_t nanoseconds = sg_mtl_gpu_frame_nanoseconds();
    return nanoseconds == 0 ? std::nullopt : std::optional<double>(static_cast<double>(nanoseconds) * 1e-9);
}

std::vector<gpu::Device::Pool> GpuDevice::getPools() const {
    const sg_stats stats = sg_query_stats();
    const sg_desc desc = sg_query_desc();
    return {
        {.name = "images", .used = static_cast<int>(stats.total.images.alive), .size = desc.image_pool_size}, {.name = "buffers", .used = static_cast<int>(stats.total.buffers.alive), .size = desc.buffer_pool_size}, {.name = "samplers", .used = static_cast<int>(stats.total.samplers.alive), .size = desc.sampler_pool_size}, {.name = "programs", .used = static_cast<int>(stats.total.shaders.alive), .size = desc.shader_pool_size}, {.name = "pipelines", .used = static_cast<int>(stats.total.pipelines.alive), .size = desc.pipeline_pool_size},
    };
}

gpu::Device::Buffer GpuDevice::makeBuffer(const BufferDesc& desc) {
    sg_buffer_desc made{};
    made.size = desc.data.empty() ? desc.size : desc.data.size();
    if (!desc.data.empty()) {
        made.data = {.ptr = desc.data.data(), .size = desc.data.size()};
    }
    made.usage.write_transient = desc.usage == Usage::Dynamic;
    made.usage.vertex_buffer = !desc.index;
    made.usage.index_buffer = desc.index;
    const std::string label(desc.label);
    made.label = label.c_str();

    const sg_buffer buffer = sg_make_buffer(&made);
    if (buffer.id == SG_INVALID_ID) {
        throw std::runtime_error(std::format("The graphics device has no room for another buffer. At most {} baked sprite batches and draw buffers can exist at once.", kBufferPoolSize));
    }
    if (sg_query_buffer_state(buffer) != SG_RESOURCESTATE_VALID) {
        sg_destroy_buffer(buffer);
        throw std::runtime_error(std::format("The graphics device could not create a buffer of {} bytes.", made.size));
    }
    return {buffer.id};
}

gpu::Device::Image GpuDevice::makeImage(const ImageDesc& desc) {
    sg_image_desc made{};
    made.width = desc.width;
    made.height = desc.height;
    made.pixel_format = toPixelFormat(desc.format);
    made.usage.copy_dst = desc.kind == ImageKind::Dynamic;
    made.usage.color_attachment = desc.kind == ImageKind::RenderTarget;
    made.usage.copy_src = desc.kind == ImageKind::RenderTarget;
    made.sample_count = 1;
    if (desc.kind == ImageKind::Texture) {
        made.num_mipmaps = static_cast<int>(desc.levels.size());
        for (std::size_t level = 0; level < desc.levels.size(); ++level) {
            made.data.mip_levels[level] = {.ptr = desc.levels[level].data(), .size = desc.levels[level].size()};
        }
    }
    const std::string label(desc.label);
    made.label = label.c_str();

    const sg_image image = sg_make_image(&made);
    if (image.id == SG_INVALID_ID) {
        throw std::runtime_error(std::format("The graphics device has no room for another texture. At most {} textures and render targets can exist at once.", kImagePoolSize));
    }
    if (sg_query_image_state(image) != SG_RESOURCESTATE_VALID) {
        sg_destroy_image(image);
        throw std::runtime_error(std::format("The graphics device could not create a {}x{} texture.", desc.width, desc.height));
    }

    ImageRecord record{.width = desc.width, .height = desc.height, .format = desc.format};
    sg_view_desc texture{};
    texture.texture.image = image;
    record.texture = sg_make_view(&texture);
    if (desc.kind == ImageKind::RenderTarget) {
        sg_view_desc attachment{};
        attachment.color_attachment.image = image;
        record.attachment = sg_make_view(&attachment);
    }
    images.emplace(image.id, record);
    return {image.id};
}

gpu::Device::Sampler GpuDevice::makeSampler(const SamplerDesc& desc) {
    sg_sampler_desc made{};
    made.min_filter = toFilter(desc.minification);
    made.mag_filter = toFilter(desc.magnification);
    made.mipmap_filter = toFilter(desc.mipmap);
    made.wrap_u = toWrap(desc.wrapU);
    made.wrap_v = toWrap(desc.wrapV);
    made.label = "haylen-sampler";
    const sg_sampler sampler = sg_make_sampler(&made);
    if (sampler.id == SG_INVALID_ID) {
        throw std::runtime_error(std::format("The graphics device has no room for another sampler. At most {} samplers can exist at once.", kSamplerPoolSize));
    }
    return {sampler.id};
}

// Backends compile shader sources when the program is made, which the debug log times. The description points into the program description, which lives until the call returns.
gpu::Device::Program GpuDevice::makeProgram(const ProgramDesc& desc) {
    const std::string vertexSource(desc.vertex.source);
    const std::string vertexEntry(desc.vertex.entry);
    const std::string vertexProfile(desc.vertex.profile);
    const std::string fragmentSource(desc.fragment.source);
    const std::string fragmentEntry(desc.fragment.entry);
    const std::string fragmentProfile(desc.fragment.profile);
    const std::string label(desc.label);
    std::deque<std::string> names;

    sg_shader_desc made{};
    made.vertex_func = {.source = vertexSource.c_str(), .entry = vertexEntry.c_str(), .d3d11_target = vertexProfile.empty() ? nullptr : vertexProfile.c_str()};
    made.fragment_func = {.source = fragmentSource.c_str(), .entry = fragmentEntry.c_str(), .d3d11_target = fragmentProfile.empty() ? nullptr : fragmentProfile.c_str()};
    for (const Attribute& attribute : desc.attributes) {
        sg_shader_vertex_attr& target = made.attrs[static_cast<std::size_t>(attribute.location)];
        target.glsl_name = names.emplace_back(attribute.name).c_str();
        target.hlsl_sem_name = "TEXCOORD";
        target.hlsl_sem_index = static_cast<std::uint8_t>(attribute.location);
    }
    for (const BlockBinding& block : desc.blocks) {
        sg_shader_uniform_block& target = made.uniform_blocks[static_cast<std::size_t>(block.slot)];
        target.stage = toStage(block.stage);
        target.size = block.size;
        target.layout = SG_UNIFORMLAYOUT_STD140;
        target.hlsl_register_b_n = static_cast<std::uint8_t>(block.binding);
        target.msl_buffer_n = static_cast<std::uint8_t>(block.binding);
        target.wgsl_group0_binding_n = static_cast<std::uint8_t>(block.binding);
        if (!block.glslName.empty()) {
            target.glsl_uniforms[0] = {.type = block.glslIntegers ? SG_UNIFORMTYPE_INT4 : SG_UNIFORMTYPE_FLOAT4, .array_count = static_cast<std::uint16_t>(block.glslCount), .glsl_name = names.emplace_back(block.glslName).c_str()};
        }
    }
    for (const TextureBinding& texture : desc.textures) {
        sg_shader_texture_view& target = made.views[static_cast<std::size_t>(texture.slot)].texture;
        target.stage = toStage(texture.stage);
        target.image_type = toImageType(texture.type);
        target.sample_type = toSampleType(texture.sampleType);
        target.hlsl_register_t_n = static_cast<std::uint8_t>(texture.binding);
        target.msl_texture_n = static_cast<std::uint8_t>(texture.binding);
        target.wgsl_group1_binding_n = static_cast<std::uint8_t>(texture.binding);
    }
    for (const SamplerBinding& sampler : desc.samplers) {
        sg_shader_sampler& target = made.samplers[static_cast<std::size_t>(sampler.slot)];
        target.stage = toStage(sampler.stage);
        target.sampler_type = toSamplerType(sampler.type);
        target.hlsl_register_s_n = static_cast<std::uint8_t>(sampler.binding);
        target.msl_sampler_n = static_cast<std::uint8_t>(sampler.binding);
        target.wgsl_group1_binding_n = static_cast<std::uint8_t>(sampler.binding);
    }
    for (std::size_t index = 0; index < desc.pairs.size(); ++index) {
        const TexturePair& pair = desc.pairs[index];
        made.texture_sampler_pairs[index] = {.stage = toStage(pair.stage), .view_slot = static_cast<std::uint8_t>(pair.texture), .sampler_slot = static_cast<std::uint8_t>(pair.sampler), .glsl_name = names.emplace_back(pair.glslName).c_str()};
    }
    made.label = label.c_str();

    const auto started = std::chrono::steady_clock::now();
    const sg_shader shader = sg_make_shader(&made);
    if (shader.id == SG_INVALID_ID) {
        throw std::runtime_error(std::format("The graphics device has no room for another shader. At most {} shader programs can exist at once.", kShaderPoolSize));
    }
    if (sg_query_shader_state(shader) != SG_RESOURCESTATE_VALID) {
        sg_destroy_shader(shader);
        throw std::runtime_error(std::format("The graphics device could not create the shader \"{}\".", label));
    }
    core::Log::debug("The graphics device made the shader \"{}\" in {:.2f} ms.", label, millisecondsSince(started));
    return {shader.id};
}

gpu::Device::Pipeline GpuDevice::makePipeline(const PipelineDesc& desc) {
    sg_pipeline_desc made{};
    made.shader = {desc.program.id};
    for (std::size_t index = 0; index < desc.buffers.size(); ++index) {
        made.layout.buffers[index].stride = desc.buffers[index].stride;
        made.layout.buffers[index].step_func = desc.buffers[index].perInstance ? SG_VERTEXSTEP_PER_INSTANCE : SG_VERTEXSTEP_PER_VERTEX;
    }
    for (const VertexAttribute& attribute : desc.attributes) {
        made.layout.attrs[static_cast<std::size_t>(attribute.location)] = {.buffer_index = attribute.buffer, .offset = attribute.offset, .format = toVertexFormat(attribute.format)};
    }
    made.primitive_type = desc.primitive == Primitive::Triangles ? SG_PRIMITIVETYPE_TRIANGLES : SG_PRIMITIVETYPE_TRIANGLE_STRIP;
    made.index_type = desc.indexType == IndexType::UInt32 ? SG_INDEXTYPE_UINT32 : (desc.indexType == IndexType::UInt16 ? SG_INDEXTYPE_UINT16 : SG_INDEXTYPE_NONE);
    made.color_count = static_cast<int>(desc.colors.size());
    for (std::size_t index = 0; index < desc.colors.size(); ++index) {
        const ColorTarget& color = desc.colors[index];
        sg_color_target_state& target = made.colors[index];
        if (!desc.swapchain) {
            target.pixel_format = toPixelFormat(color.format);
        }
        target.write_mask = toColorMask(color.writeMask);
        target.blend = {.enabled = color.blend.enabled, .src_factor_rgb = toBlendFactor(color.blend.sourceColor), .dst_factor_rgb = toBlendFactor(color.blend.destinationColor), .op_rgb = toBlendOperation(color.blend.colorOperation), .src_factor_alpha = toBlendFactor(color.blend.sourceAlpha), .dst_factor_alpha = toBlendFactor(color.blend.destinationAlpha), .op_alpha = toBlendOperation(color.blend.alphaOperation)};
    }
    if (!desc.swapchain) {
        made.depth.pixel_format = SG_PIXELFORMAT_NONE;
        made.sample_count = 1;
    }
    const std::string label(desc.label);
    made.label = label.c_str();

    const auto started = std::chrono::steady_clock::now();
    const sg_pipeline pipeline = sg_make_pipeline(&made);
    if (pipeline.id == SG_INVALID_ID) {
        throw std::runtime_error(std::format("The graphics device has no room for another pipeline. At most {} pipelines can exist at once.", kPipelinePoolSize));
    }
    if (sg_query_pipeline_state(pipeline) != SG_RESOURCESTATE_VALID) {
        sg_destroy_pipeline(pipeline);
        throw std::runtime_error(std::format("The graphics device could not create the pipeline \"{}\".", label));
    }
    core::Log::debug("The graphics device made the pipeline \"{}\" in {:.2f} ms.", label, millisecondsSince(started));
    return {pipeline.id};
}

void GpuDevice::prepareProgram(const ProgramDesc& desc, core::JobSystem& jobs) {
    Precompiler::start(jobs, {std::string(desc.vertex.source), std::string(desc.fragment.source)});
}

void GpuDevice::destroy(Buffer buffer) noexcept {
    sg_destroy_buffer({buffer.id});
}

void GpuDevice::destroy(Image image) noexcept {
    const auto found = images.find(image.id);
    if (found != images.end()) {
        sg_destroy_view(found->second.texture);
        if (found->second.attachment.id != SG_INVALID_ID) {
            sg_destroy_view(found->second.attachment);
        }
        images.erase(found);
    }
    sg_destroy_image({image.id});
}

void GpuDevice::destroy(Sampler sampler) noexcept {
    sg_destroy_sampler({sampler.id});
}

void GpuDevice::destroy(Program program) noexcept {
    sg_destroy_shader({program.id});
}

void GpuDevice::destroy(Pipeline pipeline) noexcept {
    sg_destroy_pipeline({pipeline.id});
}

const GpuDevice::ImageRecord& GpuDevice::getRecord(Image image) const {
    const auto found = images.find(image.id);
    if (found == images.end()) {
        throw std::logic_error(std::format("The image {} does not exist.", image.id));
    }
    return found->second;
}

void GpuDevice::writeBuffer(Buffer buffer, std::size_t offset, std::span<const std::byte> data) {
    sg_write_buffer_desc write{};
    write.src.data = {.ptr = data.data(), .size = data.size()};
    write.dst = {.buffer = {buffer.id}, .offset = offset};
    sg_write_buffer_transient(&write);
}

void GpuDevice::reserveStaging(std::size_t bytes) {
    if (bytes <= stagingSize) {
        return;
    }
    // Copies of the frames the GPU still runs read the smaller buffer, so it goes once the frame is committed.
    if (staging.id != SG_INVALID_ID) {
        retired.push_back(staging);
    }
    stagingSize = std::bit_ceil(bytes);
    sg_buffer_desc desc{};
    desc.size = stagingSize;
    desc.usage.staging_buffer = true;
    desc.usage.write_transient = true;
    desc.usage.copy_src = true;
    desc.label = "haylen-texture-staging";
    staging = sg_make_buffer(&desc);
    if (sg_query_buffer_state(staging) != SG_RESOURCESTATE_VALID) {
        throw std::runtime_error(std::format("The graphics device could not create a staging buffer of {} bytes.", stagingSize));
    }
}

// Sokol takes every write into the staging buffer before the first copy out of it in a frame, so the pixels of all the images go in before any of them is copied into its image, each at a multiple of four bytes.
void GpuDevice::writeImages(std::span<const ImageWrite> writes) {
    std::size_t total = 0;
    for (const ImageWrite& write : writes) {
        total += (write.pixels.size() + 3U) & ~std::size_t{3U};
    }
    reserveStaging(total);

    std::size_t offset = 0;
    for (const ImageWrite& write : writes) {
        sg_write_buffer_desc copy{};
        copy.src.data = {.ptr = write.pixels.data(), .size = write.pixels.size()};
        copy.dst = {.buffer = staging, .offset = offset};
        sg_write_buffer_transient(&copy);
        offset += (write.pixels.size() + 3U) & ~std::size_t{3U};
    }

    offset = 0;
    for (const ImageWrite& write : writes) {
        const ImageRecord& record = getRecord(write.image);
        sg_copy_buffer_to_image_desc copy{};
        copy.src.buffer = staging;
        copy.src.offset = offset;
        copy.src.bytes_per_row = static_cast<int>(static_cast<std::size_t>(record.width) * getBytesPerPixel(record.format));
        copy.src.bytes_per_slice = static_cast<int>(write.pixels.size());
        copy.dst.image = {write.image.id};
        copy.size = {.width = record.width, .height = record.height, .num_slices = 1};
        sg_copy_buffer_to_image(&copy);
        offset += (write.pixels.size() + 3U) & ~std::size_t{3U};
    }
}

void GpuDevice::beginPass(const PassDesc& desc) {
    sg_pass pass{};
    const std::string label(desc.label);
    pass.label = label.c_str();
    if (desc.colors.empty()) {
        pass.action.colors[0].load_action = desc.load == Load::Clear ? SG_LOADACTION_CLEAR : SG_LOADACTION_LOAD;
        pass.action.colors[0].clear_value = toColor(desc.clear);
        pass.swapchain = swapchain();
        sg_begin_pass(&pass);
        return;
    }
    for (std::size_t index = 0; index < desc.colors.size(); ++index) {
        const ColorAttachment& color = desc.colors[index];
        pass.action.colors[index].load_action = color.load == Load::Clear ? SG_LOADACTION_CLEAR : SG_LOADACTION_LOAD;
        pass.action.colors[index].clear_value = toColor(color.clear);
        pass.attachments.colors[index] = getRecord(color.image).attachment;
    }
    sg_begin_pass(&pass);
}

void GpuDevice::applyViewport(const math::Rect& pixels) {
    sg_apply_viewportf(pixels.x, pixels.y, pixels.width, pixels.height, true);
}

void GpuDevice::applyScissor(const math::Rect& pixels) {
    sg_apply_scissor_rectf(pixels.x, pixels.y, pixels.width, pixels.height, true);
}

void GpuDevice::applyPipeline(Pipeline pipeline) {
    sg_apply_pipeline({pipeline.id});
}

void GpuDevice::applyBindings(const Bindings& bindings) {
    sg_bindings made{};
    for (std::size_t index = 0; index < kMaxVertexBuffers; ++index) {
        made.vertex_buffers[index] = {bindings.vertexBuffers[index].id};
        made.vertex_buffer_offsets[index] = bindings.vertexOffsets[index];
    }
    made.index_buffer = {bindings.indexBuffer.id};
    made.index_buffer_offset = bindings.indexOffset;
    for (std::size_t slot = 0; slot < kMaxTextures; ++slot) {
        if (bindings.textures[slot].isValid()) {
            made.views[slot] = getRecord(bindings.textures[slot]).texture;
        }
    }
    for (std::size_t slot = 0; slot < kMaxSamplers; ++slot) {
        made.samplers[slot] = {bindings.samplers[slot].id};
    }
    sg_apply_bindings(&made);
}

void GpuDevice::applyUniforms(int slot, std::span<const std::byte> data) {
    sg_apply_uniforms(slot, {.ptr = data.data(), .size = data.size()});
}

void GpuDevice::draw(int first, int count, int instances) {
    sg_draw(first, count, instances);
}

void GpuDevice::endPass() {
    sg_end_pass();
}

void GpuDevice::commit() {
    sg_commit();
    for (const sg_buffer buffer : retired) {
        sg_destroy_buffer(buffer);
    }
    retired.clear();
}

std::unique_ptr<gpu::Device::Readback> GpuDevice::readImage(Image image, int width, int height) {
    return std::make_unique<ImageReadback>(sg_image{image.id}, width, height);
}

} // namespace haylen::sokol
