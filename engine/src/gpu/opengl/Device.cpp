#include "gpu/opengl/Device.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "gpu/DriverIssues.hpp"
#include "gpu/opengl/PixelReadback.hpp"
#include "haylen/core/Log.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten/html5_webgl.h>
#endif

namespace haylen::gpu::opengl {

std::unique_ptr<gpu::Device> Device::create(Api flavour, const std::optional<Surface>& surface, const DeviceFactory::Options& options) {
    if (!surface || surface->kind != Surface::Kind::Gl || surface->gl == nullptr) {
        throw std::runtime_error("The OpenGL device draws through an OpenGL context, and the platform gave it no surface with one.");
    }
    return std::make_unique<Device>(flavour, *surface, options.validation);
}

std::shared_ptr<Context> Device::makeContext(Api flavour, const Surface& surface) {
    surface.gl->makeCurrent();
    auto made = std::make_shared<Context>();
    made->gl.load(*surface.gl, flavour);
    made->state.reset();
    return made;
}

Device::Device(Api flavour, const Surface& surface, bool validation) : context(makeContext(flavour, surface)), gl(context->gl), state(context->state), api(flavour), surfaceContext(surface.gl), surfaceWidth(surface.width), surfaceHeight(surface.height), surfaceAttached(true), adapterName(gl.getString(Gl::kRenderer)), debug(gl, validation), timer(gl), framebuffers(gl, state) {
    gl.pixelStorei(Gl::kPackAlignment, 1);
    gl.pixelStorei(Gl::kUnpackAlignment, 1);
    if (gl.isDesktop()) {
        gl.enable(Gl::kProgramPointSize);
        gl.enable(Gl::kTextureCubeMapSeamless);
    }
    if (!gl.isWeb()) {
        ring = std::make_unique<UploadRing>(gl, state, kFramesInFlight);
    }
    gl.genVertexArrays(1, &uploadVertexArray);
    surfaceContext->setSwapInterval(surface.vsync ? 1 : 0);
    applyDriverIssues();
    readCapabilities();
}

Device::~Device() {
    for (Gl::Sync& fence : frameFences) {
        if (fence != nullptr) {
            gl.deleteSync(fence);
        }
    }
    buffers.forEach([&](const BufferRecord& record) { gl.deleteBuffers(1, &record.name); });
    samplers.forEach([&](const Gl::Uint& sampler) { gl.deleteSamplers(1, &sampler); });
    // clang-format off
    images.forEach([&](const ImageRecord& record) {
        if (record.target == Gl::kRenderbuffer) {
            gl.deleteRenderbuffers(1, &record.name);
        } else {
            gl.deleteTextures(1, &record.name);
        }
    });
    // clang-format on
    gl.deleteVertexArrays(1, &uploadVertexArray);
}

Device::Language Device::toLanguage(Api flavour) noexcept {
    return flavour == Api::OpenGl ? Language::Glsl430 : Language::Glsl300es;
}

std::uintptr_t Device::toAddress(const void* pointer) noexcept {
    return reinterpret_cast<std::uintptr_t>(pointer);
}

int Device::getLevelSize(int size, int level) noexcept {
    return std::max(1, size >> level);
}

void Device::copyPixels(std::byte* destination, std::span<const std::byte> source, bool swapsRedBlue) noexcept {
    if (!swapsRedBlue) {
        std::memcpy(destination, source.data(), source.size());
        return;
    }
    for (std::size_t index = 0; index + 3 < source.size(); index += 4) {
        destination[index] = source[index + 2];
        destination[index + 1] = source[index + 1];
        destination[index + 2] = source[index];
        destination[index + 3] = source[index + 3];
    }
}

// The issues are applied before the device makes any program, since the compiling of the context follows one of them.
void Device::applyDriverIssues() {
    const std::string version = gl.getString(Gl::kVersion);
    const DriverIssues::Driver driver{.api = api, .renderer = adapterName, .version = version};
    const std::optional<std::string_view> floatFiltering = DriverIssues::find(driver, DriverIssues::Issue::NoFloatFiltering);
    const std::optional<std::string_view> passFlushes = DriverIssues::find(driver, DriverIssues::Issue::FlushEachPass);
    const std::optional<std::string_view> noGpuDisjoint = DriverIssues::find(driver, DriverIssues::Issue::NoGpuDisjoint);
    const std::optional<std::string_view> callingThread = DriverIssues::find(driver, DriverIssues::Issue::CompileOnCallingThread);
    for (const std::optional<std::string_view>& reason : {floatFiltering, passFlushes, noGpuDisjoint, callingThread}) {
        if (reason) {
            core::Log::info("The OpenGL device works around its driver. {}", *reason);
        }
    }

    noFloatFiltering = floatFiltering.has_value();
    flushEachPass = passFlushes.has_value();
    if (noGpuDisjoint) {
        timer.setReadsDisjoint(false);
    }
    if (callingThread && gl.getFeatures().parallelShaderCompile) {
        gl.maxShaderCompilerThreads(0);
    }
}

void Device::readCapabilities() {
    const Gl::Features& features = gl.getFeatures();
    Gl::Int value = 0;
    gl.getIntegerv(Gl::kMaxTextureSize, &value);
    capabilities.maxImageSize = value;
    gl.getIntegerv(Gl::kMaxArrayTextureLayers, &value);
    capabilities.maxArrayLayers = value;
    gl.getIntegerv(Gl::kMaxSamples, &value);
    capabilities.maxSampleCount = std::max(1, value);
    capabilities.originTopLeft = false;
    capabilities.independentBlend = features.drawBuffersIndexed;
    capabilities.multisampledRenderTargets = true;
    capabilities.gpuTiming = timer.isAvailable();

    // Every flavour makes and samples images of every format, and OpenGL ES and WebGL2 draw into float formats and filter 32-bit floats only with their extensions.
    for (std::size_t index = 0; index < capabilities.textureFormats.size(); ++index) {
        const Conversions::Pixel pixel = Conversions::getPixel(static_cast<PixelFormat>(index), gl.isDesktop());
        capabilities.textureFormats[index] = true;
        capabilities.renderFormats[index] = isRenderable(pixel);
        capabilities.blendableFormats[index] = !pixel.depth && isRenderable(pixel) && (!pixel.floating || features.floatBlend);
        capabilities.filterableFormats[index] = !pixel.depth && (!pixel.floating || (features.textureFloatLinear && !noFloatFiltering));
    }

    if (features.anisotropicFiltering) {
        gl.getIntegerv(Gl::kMaxTextureMaxAnisotropy, &value);
        maxAnisotropy = static_cast<float>(std::max(1, value));
    }
}

// OpenGL draws into every format of the RHI, and OpenGL ES and WebGL2 draw into float formats only with the extensions that make them renderable.
bool Device::isRenderable(const Conversions::Pixel& pixel) const noexcept {
    if (pixel.floating) {
        return gl.getFeatures().colorBufferFloat;
    }
    if (pixel.halfFloating) {
        return gl.getFeatures().colorBufferHalfFloat;
    }
    return true;
}

Device::Language Device::getLanguage() const noexcept {
    return toLanguage(api);
}

Device::Api Device::getApi() const noexcept {
    return api;
}

const Device::Capabilities& Device::getCapabilities() const noexcept {
    return capabilities;
}

std::string Device::getAdapterName() const {
    return adapterName;
}

std::optional<double> Device::getGpuFrameSeconds() const noexcept {
    return timer.getSeconds();
}

std::vector<Device::Pool> Device::getPools() const {
    return {
        {.name = images.getName(), .used = images.size(), .size = images.capacity()}, {.name = buffers.getName(), .used = buffers.size(), .size = buffers.capacity()}, {.name = samplers.getName(), .used = samplers.size(), .size = samplers.capacity()}, {.name = programs.getName(), .used = programs.size(), .size = programs.capacity()}, {.name = pipelines.getName(), .used = pipelines.size(), .size = pipelines.capacity()},
    };
}

bool Device::isLost() const noexcept {
    return lost;
}

// An index buffer takes its data while the vertex array of uploads is bound, since the element array binding belongs to a vertex array, and WebGL2 fixes a buffer to indices by the first target it is bound to.
Device::Buffer Device::makeBuffer(const BufferDesc& desc) {
    const std::size_t size = desc.data.empty() ? desc.size : desc.data.size();
    if (size == 0) {
        throw std::logic_error(std::format("The buffer \"{}\" has no size.", desc.label));
    }
    if (desc.usage == Usage::Immutable && desc.data.empty()) {
        throw std::logic_error(std::format("The immutable buffer \"{}\" has no data.", desc.label));
    }

    Gl::Uint name = 0;
    gl.genBuffers(1, &name);
    const Gl::Enum usage = desc.usage == Usage::Immutable ? Gl::kStaticDraw : Gl::kDynamicDraw;
    const void* data = desc.data.empty() ? nullptr : desc.data.data();
    if (desc.index) {
        state.bindVertexArray(uploadVertexArray);
        gl.bindBuffer(Gl::kElementArrayBuffer, name);
        gl.bufferData(Gl::kElementArrayBuffer, static_cast<Gl::SizeiPtr>(size), data, usage);
        gl.bindBuffer(Gl::kElementArrayBuffer, 0);
    } else {
        state.bindBuffer(Gl::kArrayBuffer, name);
        gl.bufferData(Gl::kArrayBuffer, static_cast<Gl::SizeiPtr>(size), data, usage);
    }
    debug.label(Gl::kBuffer, name, desc.label);
    return {buffers.add({.name = name, .size = size, .usage = desc.usage, .index = desc.index})};
}

void Device::checkLevels(const ImageDesc& desc, const Conversions::Pixel& pixel) const {
    if (desc.kind != ImageKind::Texture) {
        if (!desc.levels.empty()) {
            throw std::logic_error(std::format("The image \"{}\" starts without pixels, since only a texture is made with them.", desc.label));
        }
        return;
    }
    if (desc.levels.size() != static_cast<std::size_t>(desc.mipLevels)) {
        throw std::logic_error(std::format("The texture \"{}\" has {} mip levels and pixels for {}.", desc.label, desc.mipLevels, desc.levels.size()));
    }
    for (int level = 0; level < desc.mipLevels; ++level) {
        const int layers = desc.type == TextureType::Volume ? getLevelSize(desc.layers, level) : desc.layers;
        const std::size_t expected = static_cast<std::size_t>(getLevelSize(desc.width, level)) * static_cast<std::size_t>(getLevelSize(desc.height, level)) * static_cast<std::size_t>(layers) * pixel.bytes;
        if (desc.levels[static_cast<std::size_t>(level)].size() != expected) {
            throw std::logic_error(std::format("The level {} of the texture \"{}\" does not hold {} bytes.", level, desc.label, expected));
        }
    }
}

void Device::checkSampleCount(const ImageDesc& desc, const Conversions::Pixel& pixel) const {
    if (desc.kind != ImageKind::RenderTarget && desc.kind != ImageKind::DepthStencil) {
        throw std::logic_error(std::format("The image \"{}\" takes {} samples, which only render targets and depth stencil images take.", desc.label, desc.sampleCount));
    }
    if (desc.type != TextureType::Flat || desc.mipLevels != 1) {
        throw std::logic_error(std::format("The multisampled image \"{}\" must be flat with one mip level.", desc.label));
    }
    Gl::Int largest = 0;
    gl.getInternalformativ(Gl::kRenderbuffer, pixel.internalFormat, Gl::kSamples, 1, &largest);
    if (desc.sampleCount > largest) {
        throw std::runtime_error(std::format("The image \"{}\" takes {} samples, and the OpenGL context draws its format with at most {}.", desc.label, desc.sampleCount, largest));
    }
}

Gl::Uint Device::makeRenderbuffer(const ImageDesc& desc, const Conversions::Pixel& pixel) {
    Gl::Uint name = 0;
    gl.genRenderbuffers(1, &name);
    gl.bindRenderbuffer(Gl::kRenderbuffer, name);
    gl.renderbufferStorageMultisample(Gl::kRenderbuffer, desc.sampleCount, pixel.internalFormat, desc.width, desc.height);
    gl.bindRenderbuffer(Gl::kRenderbuffer, 0);
    return name;
}

Gl::Uint Device::makeTexture(const ImageDesc& desc, const Conversions::Pixel& pixel, Gl::Enum target) {
    Gl::Uint name = 0;
    gl.genTextures(1, &name);
    state.bindTexture(0, target, name);
    if (desc.type == TextureType::Flat || desc.type == TextureType::Cube) {
        gl.texStorage2D(target, desc.mipLevels, pixel.internalFormat, desc.width, desc.height);
    } else {
        gl.texStorage3D(target, desc.mipLevels, pixel.internalFormat, desc.width, desc.height, desc.layers);
    }

    // The texture names its last level itself, since the OpenGL ES translator of the Android emulator stores immutable textures without their level count and samples a texture of one level as incomplete under the default mipmap filter, which reads zero.
    gl.texParameteri(target, Gl::kTextureMaxLevel, desc.mipLevels - 1);
    return name;
}

std::size_t Device::getLevelBytes(const ImageRecord& record, int level) const noexcept {
    const int layers = record.type == TextureType::Volume ? getLevelSize(record.layers, level) : record.layers;
    return static_cast<std::size_t>(getLevelSize(record.width, level)) * static_cast<std::size_t>(getLevelSize(record.height, level)) * static_cast<std::size_t>(layers) * record.pixel.bytes;
}

// The source is the address of the pixels, or their offset in the pixel unpack buffer while one is bound, and the faces of a cube follow one another in it.
void Device::uploadLevel(const ImageRecord& record, int level, std::uintptr_t source) {
    const int width = getLevelSize(record.width, level);
    const int height = getLevelSize(record.height, level);
    const Conversions::Pixel& pixel = record.pixel;
    state.bindTexture(0, record.target, record.name);

    switch (record.type) {
    case TextureType::Flat:
        gl.texSubImage2D(Gl::kTexture2d, level, 0, 0, width, height, pixel.format, pixel.type, reinterpret_cast<const void*>(source));
        return;
    case TextureType::Cube: {
        const std::size_t face = getLevelBytes(record, level) / 6;
        for (Gl::Enum index = 0; index < 6; ++index) {
            gl.texSubImage2D(Gl::kTextureCubeMapPositiveX + index, level, 0, 0, width, height, pixel.format, pixel.type, reinterpret_cast<const void*>(source + face * index));
        }
        return;
    }
    case TextureType::Array:
    case TextureType::Volume: {
        const int layers = record.type == TextureType::Volume ? getLevelSize(record.layers, level) : record.layers;
        gl.texSubImage3D(record.target, level, 0, 0, 0, width, height, layers, pixel.format, pixel.type, reinterpret_cast<const void*>(source));
        return;
    }
    }
}

void Device::checkImage(const ImageDesc& desc, const Conversions::Pixel& pixel) const {
    if (desc.width <= 0 || desc.height <= 0 || desc.width > capabilities.maxImageSize || desc.height > capabilities.maxImageSize || desc.layers < 1 || desc.mipLevels < 1 || desc.sampleCount < 1) {
        throw std::logic_error(std::format("The image \"{}\" of {}x{} pixels does not fit the device.", desc.label, desc.width, desc.height));
    }

    if (desc.type == TextureType::Cube && (desc.layers != 6 || desc.width != desc.height)) {
        throw std::logic_error(std::format("The cube \"{}\" needs six square faces.", desc.label));
    }

    if (desc.type == TextureType::Array && desc.layers > capabilities.maxArrayLayers) {
        throw std::logic_error(std::format("The array \"{}\" has {} layers, and the device holds at most {}.", desc.label, desc.layers, capabilities.maxArrayLayers));
    }

    if ((desc.kind == ImageKind::DepthStencil) != pixel.depth) {
        throw std::logic_error(std::format("The image \"{}\" takes a depth format exactly when it is a depth stencil image.", desc.label));
    }

    const bool attachment = desc.kind == ImageKind::RenderTarget || desc.kind == ImageKind::DepthStencil;
    if (attachment && !isRenderable(pixel)) {
        throw std::runtime_error(std::format("The OpenGL context cannot draw into the format of the image \"{}\".", desc.label));
    }

    checkLevels(desc, pixel);
    if (desc.sampleCount > 1) {
        checkSampleCount(desc, pixel);
    }
}

void Device::uploadTexture(const ImageDesc& desc, const ImageRecord& record) {
    state.bindBuffer(Gl::kPixelUnpackBuffer, 0);
    for (int level = 0; level < desc.mipLevels; ++level) {
        std::span<const std::byte> pixels = desc.levels[static_cast<std::size_t>(level)];
        if (record.pixel.swapsRedBlue) {
            staging.resize(pixels.size());
            copyPixels(staging.data(), pixels, true);
            pixels = staging;
        }
        uploadLevel(record, level, toAddress(pixels.data()));
    }
}

Device::Image Device::makeImage(const ImageDesc& desc) {
    const Conversions::Pixel pixel = Conversions::getPixel(desc.format, gl.isDesktop());
    checkImage(desc, pixel);

    ImageRecord record{.target = desc.sampleCount > 1 ? Gl::kRenderbuffer : Conversions::toTextureTarget(desc.type), .type = desc.type, .format = desc.format, .pixel = pixel, .width = desc.width, .height = desc.height, .layers = desc.layers, .mipLevels = desc.mipLevels, .sampleCount = desc.sampleCount, .kind = desc.kind};
    if (record.target == Gl::kRenderbuffer) {
        record.name = makeRenderbuffer(desc, pixel);
        debug.label(Gl::kRenderbuffer, record.name, desc.label);
        return {images.add(record)};
    }

    record.name = makeTexture(desc, pixel, record.target);
    if (desc.kind == ImageKind::Texture) {
        uploadTexture(desc, record);
    }
    debug.label(Gl::kTexture, record.name, desc.label);
    return {images.add(record)};
}

Device::Sampler Device::makeSampler(const SamplerDesc& desc) {
    Gl::Uint name = 0;
    gl.genSamplers(1, &name);
    gl.samplerParameteri(name, Gl::kTextureMinFilter, static_cast<Gl::Int>(Conversions::toMinification(desc.minification, desc.mipmap)));
    gl.samplerParameteri(name, Gl::kTextureMagFilter, static_cast<Gl::Int>(Conversions::toMagnification(desc.magnification)));
    gl.samplerParameteri(name, Gl::kTextureWrapS, static_cast<Gl::Int>(Conversions::toWrap(desc.wrapU)));
    gl.samplerParameteri(name, Gl::kTextureWrapT, static_cast<Gl::Int>(Conversions::toWrap(desc.wrapV)));
    gl.samplerParameteri(name, Gl::kTextureWrapR, static_cast<Gl::Int>(Conversions::toWrap(desc.wrapW)));
    gl.samplerParameterf(name, Gl::kTextureMinLod, desc.minLod);
    gl.samplerParameterf(name, Gl::kTextureMaxLod, desc.maxLod);
    if (desc.compare) {
        gl.samplerParameteri(name, Gl::kTextureCompareMode, static_cast<Gl::Int>(Gl::kCompareRefToTexture));
        gl.samplerParameteri(name, Gl::kTextureCompareFunc, static_cast<Gl::Int>(Conversions::toCompare(*desc.compare)));
    }
    if (desc.maxAnisotropy > 1 && gl.getFeatures().anisotropicFiltering) {
        gl.samplerParameterf(name, Gl::kTextureMaxAnisotropy, std::min(static_cast<float>(desc.maxAnisotropy), maxAnisotropy));
    }
    debug.label(Gl::kSampler, name, desc.label);
    return {samplers.add(name)};
}

Device::Program Device::makeProgram(const ProgramDesc& desc) {
    auto program = std::make_shared<ShaderProgram>(gl, state, desc);
    debug.label(Gl::kProgram, program->getName(), desc.label);
    return {programs.add(std::move(program))};
}

// OpenGL compiles and links a program only in its context, which belongs to the frame thread, so programs link when they are made and nothing starts earlier.
void Device::prepareProgram(const ProgramDesc&, core::JobSystem&) {}

StateCache::Raster Device::toRaster(const PipelineDesc& desc) const {
    StateCache::Raster raster;
    const ColorTarget& first = desc.colors.front();
    raster.independent = capabilities.independentBlend && std::ranges::any_of(desc.colors, [&](const ColorTarget& color) { return color.blend != first.blend || color.writeMask != first.writeMask; });
    for (std::size_t index = 0; index < StateCache::kTargets; ++index) {
        const ColorTarget& color = raster.independent && index < desc.colors.size() ? desc.colors[index] : first;
        const Blend& blend = color.blend;
        raster.blends[index] = {.enabled = blend.enabled, .sourceColor = Conversions::toBlendFactor(blend.sourceColor), .destinationColor = Conversions::toBlendFactor(blend.destinationColor), .colorOperation = Conversions::toBlendOperation(blend.colorOperation), .sourceAlpha = Conversions::toBlendFactor(blend.sourceAlpha), .destinationAlpha = Conversions::toBlendFactor(blend.destinationAlpha), .alphaOperation = Conversions::toBlendOperation(blend.alphaOperation)};
        raster.masks[index] = static_cast<std::uint8_t>(color.writeMask);
    }

    // OpenGL writes depth only while the depth test runs, so a pipeline that writes depth runs it even when every fragment passes.
    const DepthStencil& depth = desc.depthStencil;
    const bool hasDepth = desc.swapchain || depth.format.has_value();
    raster.depthTest = hasDepth && (depth.depthCompare != CompareFunction::Always || depth.depthWrite);
    raster.depthCompare = Conversions::toCompare(depth.depthCompare);
    raster.depthWrite = hasDepth && depth.depthWrite;
    raster.stencilTest = hasDepth && depth.stencilEnabled;
    raster.stencilFront = {.compare = Conversions::toCompare(depth.stencilFront.compare), .fail = Conversions::toStencilOperation(depth.stencilFront.fail), .depthFail = Conversions::toStencilOperation(depth.stencilFront.depthFail), .pass = Conversions::toStencilOperation(depth.stencilFront.pass)};
    raster.stencilBack = {.compare = Conversions::toCompare(depth.stencilBack.compare), .fail = Conversions::toStencilOperation(depth.stencilBack.fail), .depthFail = Conversions::toStencilOperation(depth.stencilBack.depthFail), .pass = Conversions::toStencilOperation(depth.stencilBack.pass)};
    raster.stencilReadMask = depth.stencilReadMask;
    raster.stencilWriteMask = depth.stencilWriteMask;
    raster.polygonOffset = depth.depthBias != 0.0F || depth.depthBiasSlope != 0.0F;
    raster.polygonOffsetFactor = depth.depthBiasSlope;
    raster.polygonOffsetUnits = depth.depthBias;

    raster.cull = desc.cullMode != CullMode::None;
    raster.cullFace = desc.cullMode == CullMode::Front ? Gl::kFront : Gl::kBack;
    raster.frontFace = desc.frontFace == FrontFace::Clockwise ? Gl::kClockwise : Gl::kCounterClockwise;
    raster.alphaToCoverage = desc.alphaToCoverage;
    return raster;
}

Device::Pipeline Device::makePipeline(const PipelineDesc& desc) {
    std::shared_ptr<ShaderProgram> shader = programs.get(desc.program.id);
    if (desc.colors.empty() || desc.colors.size() > kMaxColorTargets) {
        throw std::logic_error(std::format("The pipeline \"{}\" needs one to {} color targets.", desc.label, kMaxColorTargets));
    }

    PipelineRecord record{.shader = std::move(shader), .raster = toRaster(desc), .primitive = Conversions::toPrimitive(desc.primitive), .indexed = desc.indexType != IndexType::None, .indexType = Conversions::toIndexType(desc.indexType), .indexSize = Conversions::getIndexSize(desc.indexType), .bufferCount = desc.buffers.size()};
    record.vertexArray = std::make_unique<VertexArray>(gl, state, desc.buffers, desc.attributes);
    debug.label(Gl::kVertexArray, record.vertexArray->getName(), desc.label);
    return {pipelines.add(std::move(record))};
}

void Device::destroy(Buffer buffer) noexcept {
    const std::optional<BufferRecord> record = buffers.remove(buffer.id);
    if (!record) {
        return;
    }
    gl.deleteBuffers(1, &record->name);
    state.forgetBuffer(record->name);
    pipelines.forEach([&](PipelineRecord& pipeline) { pipeline.vertexArray->forgetBuffer(record->name); });
}

void Device::destroy(Image image) noexcept {
    const std::optional<ImageRecord> record = images.remove(image.id);
    if (!record) {
        return;
    }
    framebuffers.forgetImage(image.id);
    if (record->target == Gl::kRenderbuffer) {
        gl.deleteRenderbuffers(1, &record->name);
        return;
    }
    gl.deleteTextures(1, &record->name);
    state.forgetTexture(record->name);
}

void Device::destroy(Sampler sampler) noexcept {
    const std::optional<Gl::Uint> name = samplers.remove(sampler.id);
    if (!name) {
        return;
    }
    gl.deleteSamplers(1, &*name);
    state.forgetSampler(*name);
}

// The pipelines made from the program keep it until the last of them is destroyed.
void Device::destroy(Program program) noexcept {
    static_cast<void>(programs.remove(program.id));
}

void Device::destroy(Pipeline pipeline) noexcept {
    if (applied != nullptr && applied == pipelines.find(pipeline.id)) {
        applied = nullptr;
    }
    static_cast<void>(pipelines.remove(pipeline.id));
}

void Device::beginFrame() {
    if (frameStarted) {
        return;
    }
    frameStarted = true;
    if (ring) {
        waitForFrame(frameIndex % kFramesInFlight);
        ring->beginFrame(frameIndex);
    }
    timer.beginFrame();
}

// The region of the upload ring that the frame writes was read by the frame that ran as many frames ago as there are frames in flight, so the CPU waits for its fence, which bounds how far it runs ahead of the GPU.
void Device::waitForFrame(std::size_t slot) {
    Gl::Sync& fence = frameFences[slot];
    if (fence == nullptr) {
        return;
    }
    constexpr Gl::Uint64 kWaitNanoseconds = 100'000'000;
    Gl::Enum result = gl.clientWaitSync(fence, Gl::kSyncFlushCommandsBit, kWaitNanoseconds);
    while (result == Gl::kTimeoutExpired && !lost) {
        checkLost();
        result = gl.clientWaitSync(fence, 0, kWaitNanoseconds);
    }
    gl.deleteSync(fence);
    fence = nullptr;
}

void Device::writeBuffer(Buffer buffer, std::size_t offset, std::span<const std::byte> data) {
    const BufferRecord& record = buffers.get(buffer.id);
    if (record.usage != Usage::Dynamic) {
        throw std::logic_error("Only a dynamic buffer takes new contents.");
    }

    if (pass.open) {
        throw std::logic_error("Buffers take new contents outside the passes of a frame.");
    }

    if (offset + data.size() > record.size) {
        throw std::logic_error(std::format("The buffer of {} bytes cannot take {} bytes at {}.", record.size, data.size(), offset));
    }

    if (data.empty()) {
        return;
    }
    beginFrame();

    if (!ring) {
        state.bindBuffer(Gl::kCopyWriteBuffer, record.name);
        gl.bufferSubData(Gl::kCopyWriteBuffer, static_cast<Gl::IntPtr>(offset), static_cast<Gl::SizeiPtr>(data.size()), data.data());
        return;
    }

    const UploadRing::Slice slice = ring->map(data.size());
    std::memcpy(slice.data, data.data(), data.size());
    ring->unmap();
    state.bindBuffer(Gl::kCopyReadBuffer, ring->getBuffer());
    state.bindBuffer(Gl::kCopyWriteBuffer, record.name);
    gl.copyBufferSubData(Gl::kCopyReadBuffer, Gl::kCopyWriteBuffer, static_cast<Gl::IntPtr>(slice.offset), static_cast<Gl::IntPtr>(offset), static_cast<Gl::SizeiPtr>(data.size()));
}

std::size_t Device::checkImageWrites(std::span<const ImageWrite> writes) {
    std::size_t total = 0;
    for (const ImageWrite& write : writes) {
        const ImageRecord& record = images.get(write.image.id);
        if (record.kind != ImageKind::Dynamic) {
            throw std::logic_error("Only a dynamic image takes new pixels.");
        }
        if (write.pixels.size() != getLevelBytes(record, 0)) {
            throw std::logic_error(std::format("The pixels do not fill the image of {}x{}.", record.width, record.height));
        }
        total += UploadRing::align(write.pixels.size());
    }
    return total;
}

void Device::uploadImagesFromMemory(std::span<const ImageWrite> writes) {
    state.bindBuffer(Gl::kPixelUnpackBuffer, 0);
    for (const ImageWrite& write : writes) {
        const ImageRecord& record = images.get(write.image.id);
        std::span<const std::byte> pixels = write.pixels;
        if (record.pixel.swapsRedBlue) {
            staging.resize(pixels.size());
            copyPixels(staging.data(), pixels, true);
            pixels = staging;
        }
        uploadLevel(record, 0, toAddress(pixels.data()));
    }
}

void Device::uploadImagesThroughRing(std::span<const ImageWrite> writes, std::size_t total) {
    const UploadRing::Slice slice = ring->map(total);
    std::size_t cursor = 0;
    for (const ImageWrite& write : writes) {
        copyPixels(slice.data + cursor, write.pixels, images.get(write.image.id).pixel.swapsRedBlue);
        cursor += UploadRing::align(write.pixels.size());
    }
    ring->unmap();

    state.bindBuffer(Gl::kPixelUnpackBuffer, ring->getBuffer());
    cursor = slice.offset;
    for (const ImageWrite& write : writes) {
        uploadLevel(images.get(write.image.id), 0, cursor);
        cursor += UploadRing::align(write.pixels.size());
    }
    state.bindBuffer(Gl::kPixelUnpackBuffer, 0);
}

// The pixels of every image go into the upload ring in one mapping, and the textures copy them from it as a pixel unpack buffer. WebGL2 takes them from memory, which the browser stages itself.
void Device::writeImages(std::span<const ImageWrite> writes) {
    if (pass.open) {
        throw std::logic_error("Images take new pixels before the first pass of a frame.");
    }

    const std::size_t total = checkImageWrites(writes);
    if (total == 0) {
        return;
    }
    beginFrame();

    if (ring) {
        uploadImagesThroughRing(writes, total);
    } else {
        uploadImagesFromMemory(writes);
    }
}

void Device::requirePass(std::string_view call) const {
    if (!pass.open) {
        throw std::logic_error(std::format("The call \"{}\" happened outside a pass.", call));
    }
}

FramebufferCache::Attachment Device::toAttachment(Image image, int mipLevel, int layer) {
    const ImageRecord& record = images.get(image.id);
    return {.image = image.id, .name = record.name, .target = record.target, .mipLevel = mipLevel, .layer = layer};
}

Gl::Uint Device::getSingleFramebuffer(Image image, std::string_view label) {
    FramebufferCache::Key key{.colorCount = 1};
    key.colors[0] = toAttachment(image, 0, 0);
    return framebuffers.get(key, label);
}

// Each pass starts with the viewport on all of its attachments, no scissor, no stencil reference and a transparent blend color, as the passes of every device do.
void Device::beginPass(const PassDesc& desc) {
    if (pass.open) {
        throw std::logic_error("A pass began while another pass was open.");
    }
    beginFrame();
    timer.beginPass();
    pass = {.open = true};
    applied = nullptr;

    if (desc.colors.empty() && !desc.depthStencil) {
        beginSwapchainPass(desc);
    } else {
        beginOffscreenPass(desc);
    }
    if (pass.skipped) {
        return;
    }

    if (!desc.label.empty()) {
        debug.pushGroup(desc.label);
        pass.grouped = true;
    }
    state.setViewport({.x = 0, .y = 0, .width = pass.width, .height = pass.height});
    state.setScissorTest(false);
    state.applyStencilReference(0);
    state.applyBlendColor(math::Color::transparent());
}

void Device::beginSwapchainPass(const PassDesc& desc) {
    pass.swapchain = true;
    if (!surfaceAttached || surfaceWidth <= 0 || surfaceHeight <= 0) {
        pass.skipped = true;
        return;
    }
    pass.width = surfaceWidth;
    pass.height = surfaceHeight;
    state.bindDrawFramebuffer(0);
    state.setFramebufferSrgb(false);

    if (desc.load == Load::DontCare) {
        const std::array<Gl::Enum, 3> attachments{Gl::kColor, Gl::kDepth, Gl::kStencil};
        gl.invalidateFramebuffer(Gl::kDrawFramebuffer, static_cast<Gl::Sizei>(attachments.size()), attachments.data());
        return;
    }
    if (desc.load == Load::Clear) {
        const std::array<Gl::Float, 4> color{desc.clear.r, desc.clear.g, desc.clear.b, desc.clear.a};
        state.prepareClear();
        gl.clearBufferfv(Gl::kColor, 0, color.data());
        gl.clearBufferfi(Gl::kDepthStencil, 0, desc.clearDepth, desc.clearStencil);
    }
}

void Device::beginOffscreenPass(const PassDesc& desc) {
    if (desc.colors.size() > kMaxColorTargets) {
        throw std::logic_error(std::format("A pass draws into at most {} color attachments.", kMaxColorTargets));
    }
    FramebufferCache::Key key{.colorCount = desc.colors.size()};
    bool srgb = false;
    for (std::size_t index = 0; index < desc.colors.size(); ++index) {
        const ColorAttachment& color = desc.colors[index];
        const ImageRecord& record = images.get(color.image.id);
        if (record.kind != ImageKind::RenderTarget) {
            throw std::logic_error("A pass draws only into render targets.");
        }
        if (color.resolve.isValid() && (record.sampleCount == 1 || images.get(color.resolve.id).sampleCount != 1)) {
            throw std::logic_error("A pass resolves only a multisampled attachment into an image of one sample.");
        }
        key.colors[index] = toAttachment(color.image, color.mipLevel, color.layer);
        pass.colors[index] = color;
        srgb = srgb || record.pixel.srgb;
        if (index == 0) {
            pass.width = getLevelSize(record.width, color.mipLevel);
            pass.height = getLevelSize(record.height, color.mipLevel);
        }
    }
    pass.colorCount = desc.colors.size();

    if (desc.depthStencil) {
        const ImageRecord& record = images.get(desc.depthStencil->image.id);
        if (record.kind != ImageKind::DepthStencil) {
            throw std::logic_error("A pass takes only a depth stencil image as its depth stencil attachment.");
        }
        key.depthStencil = toAttachment(desc.depthStencil->image, 0, 0);
        key.stencil = record.pixel.stencil;
        pass.depthStencil = desc.depthStencil;
        pass.stencil = record.pixel.stencil;
        if (desc.colors.empty()) {
            pass.width = record.width;
            pass.height = record.height;
        }
    }

    pass.framebuffer = framebuffers.get(key, desc.label);
    state.bindDrawFramebuffer(pass.framebuffer);
    state.setFramebufferSrgb(srgb);
    loadAttachments();
}

// Attachments whose old contents the pass does not need are invalidated, which spares tile GPUs loading them, and the cleared ones are cleared with every channel open.
void Device::loadAttachments() {
    std::array<Gl::Enum, kMaxColorTargets + 2> discarded{};
    Gl::Sizei discardedCount = 0;
    bool clears = false;
    for (std::size_t index = 0; index < pass.colorCount; ++index) {
        const Load load = pass.colors[index].load;
        if (load == Load::DontCare) {
            discarded[static_cast<std::size_t>(discardedCount++)] = Gl::kColorAttachment0 + static_cast<Gl::Enum>(index);
        }
        clears = clears || load == Load::Clear;
    }
    if (pass.depthStencil) {
        const DepthStencilAttachment& depth = *pass.depthStencil;
        if (depth.depthLoad == Load::DontCare && (!pass.stencil || depth.stencilLoad == Load::DontCare)) {
            discarded[static_cast<std::size_t>(discardedCount++)] = pass.stencil ? Gl::kDepthStencilAttachment : Gl::kDepthAttachment;
        } else if (depth.depthLoad == Load::DontCare) {
            discarded[static_cast<std::size_t>(discardedCount++)] = Gl::kDepthAttachment;
        } else if (pass.stencil && depth.stencilLoad == Load::DontCare) {
            discarded[static_cast<std::size_t>(discardedCount++)] = Gl::kStencilAttachment;
        }
        clears = clears || depth.depthLoad == Load::Clear || (pass.stencil && depth.stencilLoad == Load::Clear);
    }
    if (discardedCount > 0) {
        gl.invalidateFramebuffer(Gl::kDrawFramebuffer, discardedCount, discarded.data());
    }
    if (!clears) {
        return;
    }

    state.prepareClear();
    for (std::size_t index = 0; index < pass.colorCount; ++index) {
        const ColorAttachment& color = pass.colors[index];
        if (color.load == Load::Clear) {
            const std::array<Gl::Float, 4> value{color.clear.r, color.clear.g, color.clear.b, color.clear.a};
            gl.clearBufferfv(Gl::kColor, static_cast<Gl::Int>(index), value.data());
        }
    }
    if (!pass.depthStencil) {
        return;
    }
    const DepthStencilAttachment& depth = *pass.depthStencil;
    const bool clearsStencil = pass.stencil && depth.stencilLoad == Load::Clear;
    if (depth.depthLoad == Load::Clear && clearsStencil) {
        gl.clearBufferfi(Gl::kDepthStencil, 0, depth.clearDepth, depth.clearStencil);
        return;
    }
    if (depth.depthLoad == Load::Clear) {
        gl.clearBufferfv(Gl::kDepth, 0, &depth.clearDepth);
    }
    if (clearsStencil) {
        const Gl::Int stencil = depth.clearStencil;
        gl.clearBufferiv(Gl::kStencil, 0, &stencil);
    }
}

StateCache::Rectangle Device::toWindow(const math::Rect& pixels) const noexcept {
    const auto left = static_cast<Gl::Int>(std::lround(pixels.x));
    const auto right = static_cast<Gl::Int>(std::lround(pixels.x + pixels.width));
    const auto top = static_cast<Gl::Int>(std::lround(pixels.y));
    const auto bottom = static_cast<Gl::Int>(std::lround(pixels.y + pixels.height));
    return {.x = left, .y = pass.height - bottom, .width = std::max(0, right - left), .height = std::max(0, bottom - top)};
}

void Device::applyViewport(const math::Rect& pixels) {
    requirePass("applyViewport");
    if (!pass.skipped) {
        state.setViewport(toWindow(pixels));
    }
}

void Device::applyScissor(const math::Rect& pixels) {
    requirePass("applyScissor");
    if (!pass.skipped) {
        state.setScissorTest(true);
        state.setScissor(toWindow(pixels));
    }
}

void Device::applyPipeline(Pipeline pipeline) {
    requirePass("applyPipeline");
    if (pass.skipped) {
        return;
    }
    applied = &pipelines.get(pipeline.id);
    state.useProgram(applied->shader->getName());
    state.applyRaster(applied->raster);
}

void Device::applyBindings(const Bindings& bindings) {
    requirePass("applyBindings");
    if (pass.skipped) {
        return;
    }
    if (applied == nullptr) {
        throw std::logic_error("Bindings were applied before a pipeline.");
    }

    std::array<Gl::Uint, kMaxVertexBuffers> vertexBuffers{};
    for (std::size_t index = 0; index < applied->bufferCount; ++index) {
        vertexBuffers[index] = buffers.get(bindings.vertexBuffers[index].id).name;
    }
    Gl::Uint indexBuffer = 0;
    if (applied->indexed) {
        const BufferRecord& record = buffers.get(bindings.indexBuffer.id);
        if (!record.index) {
            throw std::logic_error("An indexed pipeline needs an index buffer.");
        }
        indexBuffer = record.name;
        indexOffset = bindings.indexOffset;
    }
    applied->vertexArray->bind(vertexBuffers, bindings.vertexOffsets, indexBuffer);

    const std::span<const ShaderProgram::Unit> units = applied->shader->getUnits();
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        const ImageRecord& image = images.get(bindings.textures[units[unit].texture].id);
        if (image.target != units[unit].target) {
            throw std::logic_error(std::format("The texture slot {} takes a texture of another type than the one bound to it.", units[unit].texture));
        }
        state.bindTexture(unit, image.target, image.name);
        state.bindSampler(unit, samplers.get(bindings.samplers[units[unit].sampler].id));
    }
}

void Device::applyUniforms(int slot, std::span<const std::byte> data) {
    requirePass("applyUniforms");
    if (pass.skipped) {
        return;
    }
    if (applied == nullptr) {
        throw std::logic_error("Uniforms were applied before a pipeline.");
    }
    applied->shader->applyUniforms(slot, data);
}

void Device::applyStencilReference(std::uint8_t value) {
    requirePass("applyStencilReference");
    if (!pass.skipped) {
        state.applyStencilReference(value);
    }
}

void Device::applyBlendColor(math::Color value) {
    requirePass("applyBlendColor");
    if (!pass.skipped) {
        state.applyBlendColor(value);
    }
}

void Device::draw(int first, int count, int instances) {
    requirePass("draw");
    if (applied == nullptr && !pass.skipped) {
        throw std::logic_error("A draw happened before a pipeline.");
    }
    if (first < 0 || count < 0 || instances < 0) {
        throw std::logic_error("A draw needs counts of zero or more.");
    }
    if (pass.skipped || count == 0 || instances == 0) {
        return;
    }

    if (!applied->indexed) {
        gl.drawArraysInstanced(applied->primitive, first, count, instances);
        return;
    }
    const std::uintptr_t start = static_cast<std::uintptr_t>(indexOffset) + static_cast<std::uintptr_t>(first) * applied->indexSize;
    gl.drawElementsInstanced(applied->primitive, count, applied->indexType, reinterpret_cast<const void*>(start), instances);
}

// A multisampled attachment resolves into its image by a blit, which the scissor and the write masks would limit.
void Device::resolveAttachments() {
    bool resolved = false;
    for (std::size_t index = 0; index < pass.colorCount; ++index) {
        const ColorAttachment& color = pass.colors[index];
        if (!color.resolve.isValid()) {
            continue;
        }
        const Gl::Uint target = getSingleFramebuffer(color.resolve, "haylen-resolve");
        state.prepareClear();
        state.bindReadFramebuffer(pass.framebuffer);
        gl.readBuffer(Gl::kColorAttachment0 + static_cast<Gl::Enum>(index));
        state.bindDrawFramebuffer(target);
        gl.blitFramebuffer(0, 0, pass.width, pass.height, 0, 0, pass.width, pass.height, Gl::kColorBufferBit, Gl::kNearest);
        resolved = true;
    }
    if (resolved) {
        state.bindDrawFramebuffer(pass.framebuffer);
    }
}

void Device::discardAttachments() {
    std::array<Gl::Enum, kMaxColorTargets + 2> discarded{};
    Gl::Sizei discardedCount = 0;
    for (std::size_t index = 0; index < pass.colorCount; ++index) {
        if (pass.colors[index].store == Store::Discard) {
            discarded[static_cast<std::size_t>(discardedCount++)] = Gl::kColorAttachment0 + static_cast<Gl::Enum>(index);
        }
    }
    if (pass.depthStencil) {
        const DepthStencilAttachment& depth = *pass.depthStencil;
        if (depth.depthStore == Store::Discard && (!pass.stencil || depth.stencilStore == Store::Discard)) {
            discarded[static_cast<std::size_t>(discardedCount++)] = pass.stencil ? Gl::kDepthStencilAttachment : Gl::kDepthAttachment;
        } else if (depth.depthStore == Store::Discard) {
            discarded[static_cast<std::size_t>(discardedCount++)] = Gl::kDepthAttachment;
        } else if (pass.stencil && depth.stencilStore == Store::Discard) {
            discarded[static_cast<std::size_t>(discardedCount++)] = Gl::kStencilAttachment;
        }
    }
    if (discardedCount > 0) {
        gl.invalidateFramebuffer(Gl::kDrawFramebuffer, discardedCount, discarded.data());
    }
}

void Device::endPass() {
    requirePass("endPass");
    if (!pass.skipped && pass.swapchain) {
        drewSwapchain = true;
    }
    const bool offscreen = !pass.skipped && !pass.swapchain;
    if (offscreen) {
        resolveAttachments();
    }
    timer.endPass();
    if (offscreen) {
        discardAttachments();
    }
    if (flushEachPass) {
        gl.flush();
    }
    if (pass.grouped) {
        debug.popGroup();
    }
    debug.checkErrors("a pass");
    pass.open = false;
    applied = nullptr;
}

void Device::pushDebugGroup(std::string_view name) {
    debug.pushGroup(name);
}

void Device::popDebugGroup() {
    debug.popGroup();
}

// The depth and stencil of the window are never kept past the frame, so they are invalidated before the swap, and the fence of the frame tells a later frame when its region of the upload ring is free.
void Device::commit() {
    if (pass.open) {
        throw std::logic_error("The frame was committed while a pass was open.");
    }
    beginFrame();
    timer.endFrame();
    if (drewSwapchain) {
        const std::array<Gl::Enum, 2> attachments{Gl::kDepth, Gl::kStencil};
        state.bindDrawFramebuffer(0);
        gl.invalidateFramebuffer(Gl::kDrawFramebuffer, static_cast<Gl::Sizei>(attachments.size()), attachments.data());
        surfaceContext->swapBuffers();
    }
    if (ring) {
        frameFences[frameIndex % kFramesInFlight] = gl.fenceSync(Gl::kSyncGpuCommandsComplete, 0);
    }
    debug.checkErrors("the frame");
    checkLost();

    ++frameIndex;
    frameStarted = false;
    drewSwapchain = false;
}

void Device::checkLost() {
    if (lost) {
        return;
    }
#if defined(__EMSCRIPTEN__)
    lost = emscripten_is_webgl_context_lost(emscripten_webgl_get_current_context()) != 0;
#else
    lost = gl.getFeatures().robustness && gl.getGraphicsResetStatus() != Gl::kNoError;
#endif
    if (lost) {
        core::Log::error("The OpenGL context of the GPU device was lost, so the device stops drawing.");
    }
}

void Device::resizeSurface(int width, int height) {
    surfaceWidth = width;
    surfaceHeight = height;
}

void Device::detachSurface() {
    surfaceAttached = false;
}

// The objects of the device live in its context, so the platform keeps the context while the window is away and hands the same one back with the new drawable.
void Device::attachSurface(const Surface& surface) {
    if (surface.kind != Surface::Kind::Gl || surface.gl != surfaceContext) {
        throw std::logic_error("The OpenGL device takes back only the context it was made with, which holds its objects.");
    }
    surfaceContext->makeCurrent();
    surfaceContext->setSwapInterval(surface.vsync ? 1 : 0);
    surfaceWidth = surface.width;
    surfaceHeight = surface.height;
    surfaceAttached = true;
}

std::unique_ptr<Device::Readback> Device::readImage(Image image, int width, int height) {
    const ImageRecord& record = images.get(image.id);
    if (record.kind != ImageKind::RenderTarget || record.sampleCount != 1) {
        throw std::logic_error("Only a render target of one sample can be read.");
    }
    if (width <= 0 || height <= 0 || width > record.width || height > record.height) {
        throw std::logic_error(std::format("A region of {}x{} pixels does not fit the render target of {}x{}.", width, height, record.width, record.height));
    }
    const Gl::Uint framebuffer = getSingleFramebuffer(image, "haylen-readback");
    return std::make_unique<PixelReadback>(context, framebuffer, width, height, record.pixel.floating || record.pixel.halfFloating);
}

} // namespace haylen::gpu::opengl
