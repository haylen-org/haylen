#include "gpu/NullDevice.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

namespace haylen::gpu {

std::optional<std::vector<std::uint8_t>> NullDevice::NullReadback::poll() {
    if (!std::exchange(started, true)) {
        return std::nullopt;
    }
    return std::vector<std::uint8_t>(size, 0);
}

void NullDevice::requireRoom(std::size_t used, int size, std::string_view message) {
    if (used >= static_cast<std::size_t>(size)) {
        throw std::runtime_error(std::vformat(message, std::make_format_args(size)));
    }
}

std::size_t NullDevice::getBytesPerPixel(PixelFormat format) {
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

Device::Language NullDevice::getLanguage() const noexcept {
    return language;
}

gpu::Device::Api NullDevice::getApi() const noexcept {
    return Api::None;
}

std::string NullDevice::getAdapterName() const {
    return {};
}

int NullDevice::getMaxImageSize() const noexcept {
    return kMaxImageSize;
}

bool NullDevice::isOriginTopLeft() const noexcept {
    return true;
}

bool NullDevice::canBlendInto(PixelFormat format) const noexcept {
    return format != PixelFormat::R32Float;
}

std::optional<double> NullDevice::getGpuFrameSeconds() const noexcept {
    return std::nullopt;
}

std::vector<Device::Pool> NullDevice::getPools() const {
    return {
        {.name = "images", .used = static_cast<int>(images.size()), .size = kImagePoolSize}, {.name = "buffers", .used = static_cast<int>(buffers.size()), .size = kBufferPoolSize}, {.name = "samplers", .used = static_cast<int>(samplers.size()), .size = kSamplerPoolSize}, {.name = "programs", .used = static_cast<int>(programs.size()), .size = kProgramPoolSize}, {.name = "pipelines", .used = static_cast<int>(pipelines.size()), .size = kPipelinePoolSize},
    };
}

Device::Buffer NullDevice::makeBuffer(const BufferDesc& desc) {
    const std::size_t size = desc.data.empty() ? desc.size : desc.data.size();
    if (size == 0) {
        throw std::logic_error(std::format("The buffer \"{}\" has no size.", desc.label));
    }
    if (desc.usage == Usage::Immutable && desc.data.empty()) {
        throw std::logic_error(std::format("The immutable buffer \"{}\" has no data.", desc.label));
    }
    requireRoom(buffers.size(), kBufferPoolSize, "The graphics device has no room for another buffer. At most {} baked sprite batches and draw buffers can exist at once.");
    const std::uint32_t id = nextId++;
    buffers.emplace(id, BufferState{.size = size, .usage = desc.usage, .index = desc.index});
    return {id};
}

Device::Image NullDevice::makeImage(const ImageDesc& desc) {
    if (desc.width <= 0 || desc.height <= 0 || desc.width > kMaxImageSize || desc.height > kMaxImageSize) {
        throw std::logic_error(std::format("The image \"{}\" of {}x{} pixels does not fit the device.", desc.label, desc.width, desc.height));
    }
    const std::size_t pixel = getBytesPerPixel(desc.format);
    if (desc.kind == ImageKind::Texture) {
        if (desc.levels.empty()) {
            throw std::logic_error(std::format("The texture \"{}\" has no pixels.", desc.label));
        }
        int width = desc.width;
        int height = desc.height;
        for (const std::span<const std::byte> level : desc.levels) {
            if (level.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * pixel) {
                throw std::logic_error(std::format("A level of the texture \"{}\" does not hold {}x{} pixels.", desc.label, width, height));
            }
            width = std::max(1, width / 2);
            height = std::max(1, height / 2);
        }
    } else if (!desc.levels.empty()) {
        throw std::logic_error(std::format("The image \"{}\" starts without pixels, since only a texture is made with them.", desc.label));
    }
    requireRoom(images.size(), kImagePoolSize, "The graphics device has no room for another texture. At most {} textures and render targets can exist at once.");
    const std::uint32_t id = nextId++;
    images.emplace(id, ImageState{.width = desc.width, .height = desc.height, .format = desc.format, .kind = desc.kind});
    return {id};
}

Device::Sampler NullDevice::makeSampler(const SamplerDesc& desc) {
    requireRoom(samplers.size(), kSamplerPoolSize, "The graphics device has no room for another sampler. At most {} samplers can exist at once.");
    const std::uint32_t id = nextId++;
    samplers.emplace(id, desc);
    return {id};
}

Device::Program NullDevice::makeProgram(const ProgramDesc& desc) {
    ProgramState program{.blocks = {desc.blocks.begin(), desc.blocks.end()}, .textures = {desc.textures.begin(), desc.textures.end()}, .samplers = {desc.samplers.begin(), desc.samplers.end()}};
    for (const BlockBinding& block : program.blocks) {
        if (block.slot < 0 || block.slot >= static_cast<int>(kMaxBlocks) || block.size == 0) {
            throw std::logic_error(std::format("The program \"{}\" has a uniform block at slot {} that the device cannot bind.", desc.label, block.slot));
        }
    }
    for (const TextureBinding& texture : program.textures) {
        if (texture.slot < 0 || texture.slot >= static_cast<int>(kMaxTextures)) {
            throw std::logic_error(std::format("The program \"{}\" has a texture at slot {} that the device cannot bind.", desc.label, texture.slot));
        }
    }
    for (const SamplerBinding& sampler : program.samplers) {
        if (sampler.slot < 0 || sampler.slot >= static_cast<int>(kMaxSamplers)) {
            throw std::logic_error(std::format("The program \"{}\" has a sampler at slot {} that the device cannot bind.", desc.label, sampler.slot));
        }
    }
    requireRoom(programs.size(), kProgramPoolSize, "The graphics device has no room for another shader. At most {} shader programs can exist at once.");
    const std::uint32_t id = nextId++;
    programs.emplace(id, std::move(program));
    return {id};
}

Device::Pipeline NullDevice::makePipeline(const PipelineDesc& desc) {
    if (!programs.contains(desc.program.id)) {
        throw std::logic_error(std::format("The pipeline \"{}\" names a program that does not exist.", desc.label));
    }
    if (desc.colors.empty() || desc.colors.size() > kMaxColorTargets) {
        throw std::logic_error(std::format("The pipeline \"{}\" needs one to {} color targets.", desc.label, kMaxColorTargets));
    }
    for (const VertexAttribute& attribute : desc.attributes) {
        if (attribute.buffer < 0 || attribute.buffer >= static_cast<int>(desc.buffers.size())) {
            throw std::logic_error(std::format("An attribute of the pipeline \"{}\" reads a vertex buffer it does not lay out.", desc.label));
        }
    }
    requireRoom(pipelines.size(), kPipelinePoolSize, "The graphics device has no room for another pipeline. At most {} pipelines can exist at once.");
    const std::uint32_t id = nextId++;
    pipelines.emplace(id, PipelineState{.program = desc.program, .buffers = {desc.buffers.begin(), desc.buffers.end()}, .indexType = desc.indexType, .firstMask = desc.colors.front().writeMask, .colors = desc.colors.size()});
    return {id};
}

void NullDevice::prepareProgram(const ProgramDesc&, core::JobSystem&) {
    ++preparedPrograms;
}

void NullDevice::destroy(Buffer buffer) noexcept {
    buffers.erase(buffer.id);
}

void NullDevice::destroy(Image image) noexcept {
    images.erase(image.id);
}

void NullDevice::destroy(Sampler sampler) noexcept {
    samplers.erase(sampler.id);
}

void NullDevice::destroy(Program program) noexcept {
    programs.erase(program.id);
}

void NullDevice::destroy(Pipeline pipeline) noexcept {
    pipelines.erase(pipeline.id);
}

const NullDevice::BufferState& NullDevice::getBuffer(Buffer buffer) const {
    const auto found = buffers.find(buffer.id);
    if (found == buffers.end()) {
        throw std::logic_error(std::format("The buffer {} does not exist.", buffer.id));
    }
    return found->second;
}

const NullDevice::ImageState& NullDevice::getImage(Image image) const {
    const auto found = images.find(image.id);
    if (found == images.end()) {
        throw std::logic_error(std::format("The image {} does not exist.", image.id));
    }
    return found->second;
}

void NullDevice::requirePass(std::string_view call) const {
    if (!passOpen) {
        throw std::logic_error(std::format("The call \"{}\" happened outside a pass.", call));
    }
}

void NullDevice::writeBuffer(Buffer buffer, std::size_t offset, std::span<const std::byte> data) {
    const BufferState& state = getBuffer(buffer);
    if (state.usage != Usage::Dynamic) {
        throw std::logic_error("Only a dynamic buffer takes new contents.");
    }
    if (passOpen) {
        throw std::logic_error("Buffers take new contents before the passes of a frame.");
    }
    if (offset + data.size() > state.size) {
        throw std::logic_error(std::format("The buffer of {} bytes cannot take {} bytes at {}.", state.size, data.size(), offset));
    }
    ++currentFrame.bufferWrites;
}

void NullDevice::writeImages(std::span<const ImageWrite> writes) {
    if (passOpen || recordedPasses > 0) {
        throw std::logic_error("Images take new pixels before the first pass of a frame.");
    }
    if (imagesWritten) {
        throw std::logic_error("The images of a frame take their pixels in one call, and this frame made it already.");
    }
    imagesWritten = true;
    for (const ImageWrite& write : writes) {
        const ImageState& image = getImage(write.image);
        if (image.kind != ImageKind::Dynamic) {
            throw std::logic_error("Only a dynamic image takes new pixels.");
        }
        if (write.pixels.size() != static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * getBytesPerPixel(image.format)) {
            throw std::logic_error(std::format("The pixels do not fill the image of {}x{}.", image.width, image.height));
        }
        ++currentFrame.imageWrites;
        currentFrame.imageWriteBytes += write.pixels.size();
    }
}

void NullDevice::beginPass(const PassDesc& desc) {
    if (passOpen) {
        throw std::logic_error("A pass began while another pass was open.");
    }
    for (const ColorAttachment& color : desc.colors) {
        if (getImage(color.image).kind != ImageKind::RenderTarget) {
            throw std::logic_error("A pass draws only into render targets.");
        }
    }
    passOpen = true;
    applied = {};
    const bool swapchain = desc.colors.empty();
    if (recordedPasses == currentFrame.passes.size()) {
        currentFrame.passes.emplace_back();
    }
    PassRecord& pass = currentFrame.passes[recordedPasses++];
    pass.swapchain = swapchain;
    pass.load = swapchain ? desc.load : desc.colors.front().load;
    pass.clear = swapchain ? desc.clear : desc.colors.front().clear;
    pass.masks.clear();
    pass.draws = 0;
}

void NullDevice::applyViewport(const math::Rect&) {
    requirePass("applyViewport");
}

void NullDevice::applyScissor(const math::Rect&) {
    requirePass("applyScissor");
}

void NullDevice::applyPipeline(Pipeline pipeline) {
    requirePass("applyPipeline");
    const auto found = pipelines.find(pipeline.id);
    if (found == pipelines.end()) {
        throw std::logic_error(std::format("The pipeline {} does not exist.", pipeline.id));
    }
    applied = pipeline;
    currentFrame.passes[recordedPasses - 1].masks.push_back(found->second.firstMask);
}

void NullDevice::applyBindings(const Bindings& bindings) {
    requirePass("applyBindings");
    if (!applied.isValid()) {
        throw std::logic_error("Bindings were applied before a pipeline.");
    }
    const PipelineState& pipeline = pipelines.at(applied.id);
    for (std::size_t index = 0; index < pipeline.buffers.size(); ++index) {
        static_cast<void>(getBuffer(bindings.vertexBuffers[index]));
    }
    if (pipeline.indexType != IndexType::None && !getBuffer(bindings.indexBuffer).index) {
        throw std::logic_error("An indexed pipeline needs an index buffer.");
    }
    const ProgramState& program = programs.at(pipeline.program.id);
    for (const TextureBinding& texture : program.textures) {
        static_cast<void>(getImage(bindings.textures[static_cast<std::size_t>(texture.slot)]));
    }
    for (const SamplerBinding& sampler : program.samplers) {
        if (!samplers.contains(bindings.samplers[static_cast<std::size_t>(sampler.slot)].id)) {
            throw std::logic_error(std::format("The sampler slot {} of the program is empty.", sampler.slot));
        }
    }
}

void NullDevice::applyUniforms(int slot, std::span<const std::byte> data) {
    requirePass("applyUniforms");
    if (!applied.isValid()) {
        throw std::logic_error("Uniforms were applied before a pipeline.");
    }
    const ProgramState& program = programs.at(pipelines.at(applied.id).program.id);
    const auto block = std::ranges::find(program.blocks, slot, &BlockBinding::slot);
    if (block == program.blocks.end()) {
        throw std::logic_error(std::format("The program has no uniform block at slot {}.", slot));
    }
    if (data.size() != block->size) {
        throw std::logic_error(std::format("The uniform block at slot {} holds {} bytes, not {}.", slot, block->size, data.size()));
    }
}

void NullDevice::draw(int first, int count, int instances) {
    requirePass("draw");
    if (!applied.isValid()) {
        throw std::logic_error("A draw happened before a pipeline.");
    }
    if (first < 0 || count < 0 || instances < 0) {
        throw std::logic_error("A draw needs counts of zero or more.");
    }
    ++currentFrame.passes[recordedPasses - 1].draws;
}

void NullDevice::endPass() {
    requirePass("endPass");
    passOpen = false;
}

void NullDevice::commit() {
    if (passOpen) {
        throw std::logic_error("The frame was committed while a pass was open.");
    }
    // The records of both frames keep their storage, so a frame that repeats the one before it allocates nothing.
    lastFrame.passes.resize(recordedPasses);
    for (std::size_t index = 0; index < recordedPasses; ++index) {
        const PassRecord& made = currentFrame.passes[index];
        PassRecord& kept = lastFrame.passes[index];
        kept.swapchain = made.swapchain;
        kept.load = made.load;
        kept.clear = made.clear;
        kept.masks.assign(made.masks.begin(), made.masks.end());
        kept.draws = made.draws;
    }
    lastFrame.imageWrites = std::exchange(currentFrame.imageWrites, 0);
    lastFrame.imageWriteBytes = std::exchange(currentFrame.imageWriteBytes, 0);
    lastFrame.bufferWrites = std::exchange(currentFrame.bufferWrites, 0);
    recordedPasses = 0;
    imagesWritten = false;
}

std::unique_ptr<Device::Readback> NullDevice::readImage(Image image, int width, int height) {
    if (getImage(image).kind != ImageKind::RenderTarget) {
        throw std::logic_error("Only a render target can be read.");
    }
    return std::make_unique<NullReadback>(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
}

} // namespace haylen::gpu
