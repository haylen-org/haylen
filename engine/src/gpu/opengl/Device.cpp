#include "gpu/opengl/Device.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <format>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

#include "gpu/DriverIssues.hpp"
#include "gpu/ImageInfo.hpp"
#include "gpu/ImageRules.hpp"
#include "gpu/ProgramSlots.hpp"
#include "gpu/SamplerRules.hpp"
#include "gpu/opengl/BufferReadback.hpp"
#include "gpu/opengl/PixelReadback.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/gpu/ContractError.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten/html5_webgl.h>
#endif

namespace haylen::gpu::opengl {

std::unique_ptr<gpu::Device> Device::create(Api flavour, const std::optional<Surface>& surface, const DeviceOptions& options) {
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

Device::Device(Api flavour, const Surface& surface, bool validation) : context(makeContext(flavour, surface)), gl(context->gl), state(context->state), api(flavour), surfaceContext(surface.gl), surfaceSize(surface.size), surfaceSamples(surface.sampleCount), surfaceAttached(true), surfaceVsync(surface.vsync), adapterName(gl.getString(Gl::kRenderer)), debug(gl, validation), timer(gl), uniformRing(gl, state, kFramesInFlight, readUniformAlignment(gl)), framebuffers(gl, state) {
    gl.pixelStorei(Gl::kPackAlignment, 1);
    gl.pixelStorei(Gl::kUnpackAlignment, 1);

    if (gl.isDesktop()) {
        gl.enable(Gl::kProgramPointSize);
        gl.enable(Gl::kTextureCubeMapSeamless);
    }

    // The vertex stages write depth from 0 to 1, which clip control maps onto the depth of the attachments without the loss of precision that a range from -1 to 1 brings to reversed depth.
    if (gl.getFeatures().clipControl) {
        gl.clipControl(Gl::kLowerLeft, Gl::kZeroToOne);
    }

    if (!gl.isWeb()) {
        ring = std::make_unique<UploadRing>(gl, state, kFramesInFlight);
    }

    surfaceContext->setSwapInterval(surface.vsync ? 1 : 0);
    applyDriverIssues();
    readCapabilities();
    scaleDrawable(surfaceScaled, surfaceSize);
    gl.genVertexArrays(1, &uploadVertexArray);
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

std::size_t Device::readUniformAlignment(const Gl& functions) {
    Gl::Int value = 0;
    functions.getIntegerv(Gl::kUniformBufferOffsetAlignment, &value);
    return static_cast<std::size_t>(value);
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
    const DriverIssues::Driver driver{.api = api, .renderer = adapterName, .version = version, .versionNumber = DriverIssues::readMesaVersion(version)};
    const std::optional<std::string_view> floatFiltering = DriverIssues::find(driver, DriverIssues::Issue::NoFloatFiltering);
    const std::optional<std::string_view> passFlushes = DriverIssues::find(driver, DriverIssues::Issue::FlushEachPass);
    const std::optional<std::string_view> noGpuDisjoint = DriverIssues::find(driver, DriverIssues::Issue::NoGpuDisjoint);
    const std::optional<std::string_view> callingThread = DriverIssues::find(driver, DriverIssues::Issue::CompileOnCallingThread);
    const std::optional<std::string_view> layerClears = DriverIssues::find(driver, DriverIssues::Issue::ClearLayersByDrawing);
    const std::optional<std::string_view> sampleClears = DriverIssues::find(driver, DriverIssues::Issue::ClearSamplesByDrawing);
    const std::optional<std::string_view> noAnisotropy = DriverIssues::find(driver, DriverIssues::Issue::NoAnisotropy);
    const std::optional<std::string_view> depthWithStencil = DriverIssues::find(driver, DriverIssues::Issue::MakeFloatDepthWithStencil);

    for (const std::optional<std::string_view>& reason : {floatFiltering, passFlushes, noGpuDisjoint, callingThread, layerClears, sampleClears, noAnisotropy, depthWithStencil}) {
        if (reason) {
            core::Log::info("The OpenGL device works around its driver. {}", *reason);
        }
    }

    noFloatFiltering = floatFiltering.has_value();
    anisotropic = !noAnisotropy.has_value();
    flushEachPass = passFlushes.has_value();
    floatDepthWithStencil = depthWithStencil.has_value();
    layersClearedByDrawing = layerClears.has_value();
    samplesClearedByDrawing = sampleClears.has_value();

    if (noGpuDisjoint) {
        timer.setReadsDisjoint(false);
    }

    if (callingThread && gl.getFeatures().parallelShaderCompile) {
        gl.maxShaderCompilerThreads(0);
    }

    if (layersClearedByDrawing || samplesClearedByDrawing) {
        drawnClear = std::make_unique<DrawnClear>(gl, state);
    }
}

void Device::readCapabilities() {
    const Gl::Features& features = gl.getFeatures();
    Gl::Int value = 0;
    gl.getIntegerv(Gl::kMaxTextureSize, &value);
    capabilities.maxImageSize = value;
    gl.getIntegerv(Gl::kMaxArrayTextureLayers, &value);
    capabilities.maxArrayLayers = value;
    gl.getIntegerv(Gl::kMax3dTextureSize, &value);
    capabilities.maxVolumeSize = value;
    gl.getIntegerv(Gl::kMaxCubeMapTextureSize, &value);
    capabilities.maxCubeSize = value;
    capabilities.originTopLeft = false;
    capabilities.depthRange = features.clipControl ? math::Mat4::DepthRange::ZeroToOne : math::Mat4::DepthRange::NegativeOneToOne;
    capabilities.independentBlend = features.drawBuffersIndexed;
    capabilities.gpuTiming = timer.isAvailable();

    // Every flavour makes and samples images of every uncompressed format and of the compressed families its version or extensions hold, and OpenGL ES and WebGL2 draw into float formats and filter 32-bit floats only with their extensions.
    for (std::size_t index = 0; index < capabilities.formats.size(); ++index) {
        const auto format = static_cast<PixelFormat>(index);
        const Conversions::Pixel pixel = Conversions::getPixel(format, gl.isDesktop());
        const bool renderable = !pixel.compressed && isRenderable(pixel);
        FormatSupport& support = capabilities.formats[index];
        support.sample = isSampled(format);
        support.render = renderable;
        support.blend = !pixel.depth && renderable && (!pixel.floating || features.floatBlend);
        support.filter = !pixel.depth && support.sample && (!pixel.floating || (features.textureFloatLinear && !noFloatFiltering));
        support.storage = isStorage(format);
        support.sampleCounts = renderable ? getSampleCounts(pixel) : std::uint8_t{0};
    }

    // Only OpenGL biases the level of detail in its samplers, and OpenGL ES 3.2 and the extensions of OpenGL ES read border colors, which WebGL2 lacks.
    capabilities.maxAnisotropy = 1;
    if (features.anisotropicFiltering && anisotropic) {
        gl.getIntegerv(Gl::kMaxTextureMaxAnisotropy, &value);
        capabilities.maxAnisotropy = std::max(1, value);
    }

    if (gl.isDesktop()) {
        float bias = 0.0F;
        gl.getFloatv(Gl::kMaxTextureLodBias, &bias);
        capabilities.maxLodBias = bias;
    }

    capabilities.borderColors = features.borderClamp;

    // OpenGL 4.3 and OpenGL ES 3.1 bring the storage buffers and the indirect draws that OpenGL ES 3.0 and WebGL2 lack, and the context names the largest uniform block that it reads.
    capabilities.storageBuffers = hasComputeVersion();
    capabilities.indirectBuffers = hasComputeVersion();
    gl.getIntegerv(Gl::kMaxUniformBlockSize, &value);
    capabilities.maxUniformBlockSize = static_cast<std::size_t>(value);
}

// Whether the context runs OpenGL 4.3 or OpenGL ES 3.1 or later, the versions that bring compute shaders with storage buffers, image units and indirect draws.
bool Device::hasComputeVersion() const noexcept {
    const Gl::Features& features = gl.getFeatures();
    return gl.isDesktop() || (!gl.isWeb() && features.majorVersion * 10 + features.minorVersion >= 31);
}

// The context lists the sample counts of the multisampled renderbuffers of each format, which the multisampled attachments of the device are, into a list that it fills from the start, and one sample stands for every format it draws into. WebGL2 answers only the list itself, so the device asks for no count of its entries.
std::uint8_t Device::getSampleCounts(const Conversions::Pixel& pixel) const {
    std::array<Gl::Int, 16> listed{};
    gl.getInternalformativ(Gl::kRenderbuffer, pixel.internalFormat, Gl::kSamples, static_cast<Gl::Sizei>(listed.size()), listed.data());

    std::uint8_t counts = 1;
    for (const Gl::Int samples : listed) {
        if (samples == 2 || samples == 4 || samples == 8) {
            counts |= static_cast<std::uint8_t>(samples);
        }
    }

    return counts;
}

// OpenGL draws into every uncompressed format of the RHI, and OpenGL ES and WebGL2 draw into float formats only with the extensions that make them renderable.
bool Device::isRenderable(const Conversions::Pixel& pixel) const noexcept {
    if (pixel.floating || pixel.packedFloating) {
        return gl.getFeatures().colorBufferFloat;
    }

    if (pixel.halfFloating) {
        return gl.getFeatures().colorBufferHalfFloat;
    }

    return true;
}

// A block compressed family samples where the version of the flavour holds it or an extension adds it.
bool Device::isSampled(PixelFormat format) const noexcept {
    const Gl::Features& features = gl.getFeatures();
    switch (ImageInfo::getCompression(format)) {
    case ImageInfo::Compression::None:
        return true;
    case ImageInfo::Compression::Bc:
        break;
    case ImageInfo::Compression::Etc2:
        return features.etc2;
    case ImageInfo::Compression::Astc:
        return features.astc;
    case ImageInfo::Compression::AstcHdr:
        return features.astcHdr;
    }

    switch (format) {
    case PixelFormat::Bc1:
    case PixelFormat::Bc2:
    case PixelFormat::Bc3:
        return features.s3tc;
    case PixelFormat::Bc1Srgb:
    case PixelFormat::Bc2Srgb:
    case PixelFormat::Bc3Srgb:
        return features.s3tcSrgb;
    case PixelFormat::Bc4:
    case PixelFormat::Bc4Snorm:
    case PixelFormat::Bc5:
    case PixelFormat::Bc5Snorm:
        return features.rgtc;
    default:
        return features.bptc;
    }
}

// Shaders write as storage images the formats of the image units of OpenGL 4.3 and of OpenGL ES 3.1, and OpenGL ES 3.0 and WebGL2 have no image units.
bool Device::isStorage(PixelFormat format) const noexcept {
    switch (format) {
    case PixelFormat::Rgba8:
    case PixelFormat::Rgba16Float:
    case PixelFormat::R32Float:
    case PixelFormat::Rgba32Float:
        return hasComputeVersion();
    case PixelFormat::R8:
    case PixelFormat::Rg8:
    case PixelFormat::Rgb10A2:
    case PixelFormat::Rg11B10Float:
    case PixelFormat::R16Float:
    case PixelFormat::Rg16Float:
    case PixelFormat::Rg32Float:
        return gl.isDesktop();
    default:
        return false;
    }
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

void Device::readPools(std::vector<Pool>& pools) const {
    pools.assign({
        {.name = "images", .used = images.size(), .size = images.capacity()},
        {.name = "buffers", .used = buffers.size(), .size = buffers.capacity()},
        {.name = "samplers", .used = samplers.size(), .size = samplers.capacity()},
        {.name = "programs", .used = programs.size(), .size = programs.capacity()},
        {.name = "pipelines", .used = pipelines.size(), .size = pipelines.capacity()},
    });
}

bool Device::isLost() const noexcept {
    return context->lost;
}

// A buffer of indices takes its data while the vertex array of uploads is bound, since the element array binding belongs to a vertex array, and WebGL2 fixes a buffer to indices by the first target it is bound to, after which only the targets of copies take it. The other buffers take their data through the target of copy destinations, which fixes no kind of buffer on WebGL2.
Device::Buffer Device::makeBuffer(const BufferDesc& desc) {
    const BufferRules::Description description = BufferRules::check(desc, capabilities);

    Gl::Uint name = 0;
    gl.genBuffers(1, &name);
    const auto size = static_cast<Gl::SizeiPtr>(description.size);
    const void* data = desc.data.empty() ? nullptr : desc.data.data();
    const Gl::Enum usage = Conversions::toBufferUsage(description.update);
    if (description.usage.index) {
        state.bindVertexArray(uploadVertexArray);
        gl.bindBuffer(Gl::kElementArrayBuffer, name);
        gl.bufferData(Gl::kElementArrayBuffer, size, data, usage);
        gl.bindBuffer(Gl::kElementArrayBuffer, 0);
    } else {
        state.bindBuffer(Gl::kCopyWriteBuffer, name);
        gl.bufferData(Gl::kCopyWriteBuffer, size, data, usage);
    }

    debug.label(Gl::kBuffer, name, desc.label);
    return {buffers.add(BufferRecord{.name = name, .description = description})};
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
    return ImageInfo::getRegionBytes(record.format, getLevelSize(record.width, level), getLevelSize(record.height, level)) * static_cast<std::size_t>(layers);
}

// The source is the address of the pixels, or their offset in the pixel unpack buffer while one is bound, and the faces of a cube follow one another in it.
void Device::uploadLevel(const ImageRecord& record, int level, std::uintptr_t source) {
    const int width = getLevelSize(record.width, level);
    const int height = getLevelSize(record.height, level);
    state.bindTexture(0, record.target, record.name);

    switch (record.type) {
    case TextureType::Flat:
        uploadLayers(record, Gl::kTexture2d, level, {width, height, 1}, source);
        return;
    case TextureType::Cube: {
        const std::size_t face = getLevelBytes(record, level) / 6;
        for (Gl::Enum index = 0; index < 6; ++index) {
            uploadLayers(record, Gl::kTextureCubeMapPositiveX + index, level, {width, height, 1}, source + face * index);
        }

        return;
    }
    case TextureType::Array:
    case TextureType::Volume: {
        const int layers = record.type == TextureType::Volume ? getLevelSize(record.layers, level) : record.layers;
        uploadLayers(record, record.target, level, {width, height, layers}, source);
        return;
    }
    }
}

// A compressed format takes its blocks as they are, with the bytes they hold, and the other formats take their pixels by their format and type.
void Device::uploadLayers(const ImageRecord& record, Gl::Enum target, int level, const ImageInfo::Extent& extent, std::uintptr_t source) {
    const Conversions::Pixel& pixel = record.pixel;
    const auto* data = reinterpret_cast<const void*>(source);
    const bool flat = target != Gl::kTexture2dArray && target != Gl::kTexture3d;
    if (pixel.compressed) {
        const auto size = static_cast<Gl::Sizei>(ImageInfo::getRegionBytes(record.format, extent.width, extent.height) * static_cast<std::size_t>(extent.depth));
        if (flat) {
            gl.compressedTexSubImage2D(target, level, 0, 0, extent.width, extent.height, pixel.internalFormat, size, data);
            return;
        }

        gl.compressedTexSubImage3D(target, level, 0, 0, 0, extent.width, extent.height, extent.depth, pixel.internalFormat, size, data);
        return;
    }

    if (flat) {
        gl.texSubImage2D(target, level, 0, 0, extent.width, extent.height, pixel.format, pixel.type, data);
        return;
    }

    gl.texSubImage3D(target, level, 0, 0, 0, extent.width, extent.height, extent.depth, pixel.format, pixel.type, data);
}

// A dynamic image is flat with one mip level, whose region takes the pixels at the address or the offset into the pixel unpack buffer.
void Device::uploadRegion(const ImageRecord& record, const Region& region, std::uintptr_t source) {
    const Conversions::Pixel& pixel = record.pixel;
    state.bindTexture(0, record.target, record.name);
    gl.texSubImage2D(Gl::kTexture2d, 0, region.x, region.y, region.width, region.height, pixel.format, pixel.type, reinterpret_cast<const void*>(source));
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
    ImageRules::check(desc, capabilities);

    ImageRecord record{.target = desc.sampleCount > 1 ? Gl::kRenderbuffer : Conversions::toTextureTarget(desc.type), .type = desc.type, .format = desc.format, .pixel = pixel, .width = desc.width, .height = desc.height, .layers = desc.layers, .mipLevels = desc.mipLevels, .sampleCount = desc.sampleCount, .kind = desc.kind};
    if (record.target == Gl::kRenderbuffer) {
        record.name = makeRenderbuffer(desc, pixel);
        debug.label(Gl::kRenderbuffer, record.name, desc.label);
        return {images.add(std::move(record))};
    }

    // A texture of 32-bit float depth on a driver that needs it takes a stencil plane, which no pass attaches or clears, since its depth still attaches alone.
    Conversions::Pixel stored = pixel;
    if (floatDepthWithStencil && desc.format == PixelFormat::Depth32Float) {
        stored.internalFormat = Gl::kDepth32fStencil8;
    }

    record.name = makeTexture(desc, stored, record.target);
    if (desc.kind == ImageKind::Texture) {
        uploadTexture(desc, record);
    }

    debug.label(Gl::kTexture, record.name, desc.label);
    return {images.add(std::move(record))};
}

Device::Sampler Device::makeSampler(const SamplerDesc& desc) {
    SamplerRules::check(desc, capabilities);
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

    if (desc.maxAnisotropy > 1) {
        gl.samplerParameterf(name, Gl::kTextureMaxAnisotropy, static_cast<float>(desc.maxAnisotropy));
    }

    if (desc.lodBias != 0.0F) {
        gl.samplerParameterf(name, Gl::kTextureLodBias, desc.lodBias);
    }

    if (desc.wrapU == Wrap::ClampToBorder || desc.wrapV == Wrap::ClampToBorder || desc.wrapW == Wrap::ClampToBorder) {
        const std::array<float, 4> border = Conversions::toBorderColor(desc.borderColor);
        gl.samplerParameterfv(name, Gl::kTextureBorderColor, border.data());
    }

    debug.label(Gl::kSampler, name, desc.label);
    return {samplers.add(std::move(name))};
}

Device::Program Device::makeProgram(const ProgramDesc& desc) {
    ProgramSlots::check(desc);
    if (std::ranges::any_of(desc.blocks, [this](const BlockBinding& block) { return block.size > capabilities.maxUniformBlockSize; })) {
        throw ContractError(ContractError::Rule::Program);
    }

    auto program = std::make_shared<ShaderProgram>(gl, state, desc);
    debug.label(Gl::kProgram, program->getName(), desc.label);
    return {programs.add(std::move(program))};
}

// OpenGL compiles and links a program only in its context, which belongs to the frame thread, so programs link when they are made and nothing starts earlier.
void Device::prepareProgram(const ProgramDesc&, core::JobSystem&) {}

StateCache::Raster Device::toRaster(const PipelineDesc& desc) const {
    // A pipeline that draws depth alone, such as into a shadow map, writes no color.
    StateCache::Raster raster;
    const ColorTarget none{.writeMask = ColorMask::None};
    const ColorTarget& first = desc.colors.empty() ? none : desc.colors.front();
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

    if ((desc.colors.empty() && !desc.depthStencil.format) || desc.colors.size() > kMaxColorTargets) {
        throw ContractError(ContractError::Rule::Pipeline);
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

    applySizes();
    frameStarted = true;

    if (ring) {
        waitForFrame(frameIndex % kFramesInFlight);
        ring->beginFrame(frameIndex);
    }

    uniformRing.beginFrame(frameIndex);
    timer.beginFrame();
}

// The regions of the upload ring and of the uniform ring that the frame writes were read by the frame that ran as many frames ago as there are frames in flight, so the CPU waits for its fence, which bounds how far it runs ahead of the GPU.
// A context without robustness never reports a GPU that stopped, so a frame that the GPU did not finish within `kFrameTimeout` loses the device.
void Device::waitForFrame(std::size_t slot) {
    Gl::Sync& fence = frameFences[slot];
    if (fence == nullptr) {
        return;
    }

    Gl::Uint64 waited = kWaitStep;
    Gl::Enum result = gl.clientWaitSync(fence, Gl::kSyncFlushCommandsBit, kWaitStep);
    while (result == Gl::kTimeoutExpired && !context->lost && waited < kFrameTimeout) {
        checkLost();
        result = gl.clientWaitSync(fence, 0, kWaitStep);
        waited += kWaitStep;
    }

    if (result == Gl::kTimeoutExpired && !context->lost) {
        context->lost = true;
        core::Log::error("The GPU did not finish a frame within {} seconds, so the OpenGL device stops drawing.", kFrameTimeout / 1'000'000'000);
    }

    gl.deleteSync(fence);
    fence = nullptr;
}

// The bytes of a dynamic or a stream buffer reach it through a copy from the upload ring, or straight on WebGL2, whose browser stages them. OpenGL orders the write with the draws around it, so the passes before it read the old bytes and the passes after it the new ones.
void Device::writeBuffer(Buffer buffer, std::size_t offset, std::span<const std::byte> data) {
    if (pass.open) {
        throw ContractError(ContractError::Rule::BufferWrite);
    }

    const BufferRecord& record = buffers.get(buffer.id);
    BufferRules::checkWrite(record.description, offset, data.size());
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

// The targets of copies take a buffer of any kind, even a buffer of indices on WebGL2, and OpenGL orders the copy with the draws around it.
void Device::copyBuffer(Buffer source, std::size_t sourceOffset, Buffer destination, std::size_t destinationOffset, std::size_t size) {
    if (pass.open) {
        throw ContractError(ContractError::Rule::BufferWrite);
    }

    const BufferRecord& from = buffers.get(source.id);
    const BufferRecord& to = buffers.get(destination.id);
    BufferRules::checkCopy(source, from.description, sourceOffset, destination, to.description, destinationOffset, size);

    state.bindBuffer(Gl::kCopyReadBuffer, from.name);
    state.bindBuffer(Gl::kCopyWriteBuffer, to.name);
    gl.copyBufferSubData(Gl::kCopyReadBuffer, Gl::kCopyWriteBuffer, static_cast<Gl::IntPtr>(sourceOffset), static_cast<Gl::IntPtr>(destinationOffset), static_cast<Gl::SizeiPtr>(size));
}

std::size_t Device::checkImageWrites(std::span<const ImageWrite> writes) {
    std::size_t total = 0;
    for (const ImageWrite& write : writes) {
        const ImageRecord& record = images.get(write.image.id);
        if (record.kind != ImageKind::Dynamic) {
            throw ContractError(ContractError::Rule::ImageWrite);
        }

        const Region& region = write.region;
        const bool inside = region.x >= 0 && region.y >= 0 && region.width > 0 && region.height > 0 && region.x + region.width <= record.width && region.y + region.height <= record.height;
        const std::size_t bytes = ImageInfo::getRegionBytes(record.format, region.width, region.height);
        if (!inside || write.pixels.size() != bytes) {
            throw ContractError(ContractError::Rule::ImageWrite);
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

        uploadRegion(record, write.region, toAddress(pixels.data()));
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
        uploadRegion(images.get(write.image.id), write.region, cursor);
        cursor += UploadRing::align(write.pixels.size());
    }

    state.bindBuffer(Gl::kPixelUnpackBuffer, 0);
}

// The pixels of every image go into the upload ring in one mapping, and the textures copy them from it as a pixel unpack buffer. WebGL2 takes them from memory, which the browser stages itself.
void Device::writeImages(std::span<const ImageWrite> writes) {
    if (pass.open) {
        throw ContractError(ContractError::Rule::ImageWrite);
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

void Device::requirePass() const {
    if (!pass.open) {
        throw ContractError(ContractError::Rule::PassOrder);
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

// Each pass starts with the viewport on all of its attachments, no scissor, no stencil reference and a transparent blend color, as the passes of every device do. The pass opens once its framebuffer is bound, so a pass that fails to begin leaves no pass open.
void Device::beginPass(const PassDesc& desc) {
    if (pass.open) {
        throw ContractError(ContractError::Rule::PassOrder);
    }

    const bool swapchain = desc.colors.empty() && !desc.depthStencil;
    if (swapchain) {
        requireUnpresented(desc.surface);
    }

    beginFrame();
    pass = {};
    applied = nullptr;

    if (swapchain) {
        beginSwapchainPass(desc);
    } else {
        beginOffscreenPass(desc);
    }

    pass.open = true;
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

// A pass into a surface makes its drawable current, whose default framebuffer the context then draws into.
void Device::beginSwapchainPass(const PassDesc& desc) {
    pass.swapchain = true;
    pass.surface = desc.surface;
    const ScaledDrawable* scaled = nullptr;
    if (desc.surface == 0) {
        if (!surfaceAttached || surfaceSize.width <= 0 || surfaceSize.height <= 0) {
            pass.skipped = true;
            return;
        }

        leaveDrawable(0);
        makeDrawableCurrent(0, *surfaceContext);
        pass.width = surfaceSize.width;
        pass.height = surfaceSize.height;
        scaled = surfaceScaled.get();
    } else {
        const Drawable& drawable = requireDrawable(desc.surface);
        if (drawable.size.width <= 0 || drawable.size.height <= 0) {
            pass.skipped = true;
            return;
        }

        leaveDrawable(desc.surface);
        makeDrawableCurrent(desc.surface, *drawable.context);
        pass.width = drawable.size.width;
        pass.height = drawable.size.height;
        scaled = drawable.scaled.get();
    }

    timer.beginPass();
    state.bindDrawFramebuffer(scaled != nullptr ? scaled->getFramebuffer() : 0);
    state.setFramebufferSrgb(false);

    if (desc.load == Load::DontCare && scaled != nullptr) {
        const std::array<Gl::Enum, 2> attachments{Gl::kColorAttachment0, Gl::kDepthStencilAttachment};
        gl.invalidateFramebuffer(Gl::kDrawFramebuffer, static_cast<Gl::Sizei>(attachments.size()), attachments.data());
        return;
    }

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
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    FramebufferCache::Key key{.colorCount = desc.colors.size()};
    bool srgb = false;
    for (std::size_t index = 0; index < desc.colors.size(); ++index) {
        const ColorAttachment& color = desc.colors[index];
        const ImageRecord& record = images.get(color.image.id);
        if (record.kind != ImageKind::RenderTarget) {
            throw ContractError(ContractError::Rule::PassAttachments);
        }

        if (color.resolve.isValid() && (record.sampleCount == 1 || images.get(color.resolve.id).sampleCount != 1)) {
            throw ContractError(ContractError::Rule::PassAttachments);
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
            throw ContractError(ContractError::Rule::PassAttachments);
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
    timer.beginPass();
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

    clearByDrawing();
    state.prepareClear();

    for (std::size_t index = 0; index < pass.colorCount; ++index) {
        const ColorAttachment& color = pass.colors[index];
        if (color.load == Load::Clear && !isClearedByDrawing(color)) {
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

void Device::clearByDrawing() {
    for (std::size_t index = 0; index < pass.colorCount; ++index) {
        const ColorAttachment& color = pass.colors[index];
        if (color.load == Load::Clear && isClearedByDrawing(color)) {
            drawnClear->clear(index, pass.colorCount, color.clear, pass.width, pass.height);
        }
    }
}

bool Device::isClearedByDrawing(const ColorAttachment& color) {
    const ImageRecord& record = images.get(color.image.id);
    if (samplesClearedByDrawing && record.sampleCount > 1) {
        return true;
    }

    const bool layered = record.target == Gl::kTexture2dArray || record.target == Gl::kTexture3d;
    return layersClearedByDrawing && layered && color.layer > 0;
}

StateCache::Rectangle Device::toWindow(const math::Rect& pixels) const noexcept {
    const auto left = static_cast<Gl::Int>(std::lround(pixels.x));
    const auto right = static_cast<Gl::Int>(std::lround(pixels.x + pixels.width));
    const auto top = static_cast<Gl::Int>(std::lround(pixels.y));
    const auto bottom = static_cast<Gl::Int>(std::lround(pixels.y + pixels.height));
    return {.x = left, .y = pass.height - bottom, .width = std::max(0, right - left), .height = std::max(0, bottom - top)};
}

void Device::applyViewport(const math::Rect& pixels) {
    requirePass();

    if (!pass.skipped) {
        state.setViewport(toWindow(pixels));
    }
}

void Device::applyScissor(const math::Rect& pixels) {
    requirePass();

    if (!pass.skipped) {
        state.setScissorTest(true);
        state.setScissor(toWindow(pixels));
    }
}

void Device::applyPipeline(Pipeline pipeline) {
    requirePass();

    if (pass.skipped) {
        return;
    }

    applied = &pipelines.get(pipeline.id);
    appliedUnits = 0;
    appliedBlocks = 0;
    bufferBlocks = 0;
    state.useProgram(applied->shader->getName());
    state.bindVertexArray(applied->vertexArray->getName());
    state.applyRaster(applied->raster);
}

void Device::applyBindings(const Bindings& bindings) {
    requirePass();

    if (pass.skipped) {
        return;
    }

    if (applied == nullptr) {
        throw ContractError(ContractError::Rule::PipelineState);
    }

    std::array<Gl::Uint, kMaxVertexBuffers> vertexBuffers{};
    for (std::size_t index = 0; index < applied->bufferCount; ++index) {
        const BufferRecord& record = buffers.get(bindings.vertexBuffers[index].id);
        BufferRules::checkVertex(record.description);
        vertexBuffers[index] = record.name;
    }

    Gl::Uint indexBuffer = 0;
    if (applied->indexed) {
        const BufferRecord& record = buffers.get(bindings.indexBuffer.id);
        BufferRules::checkIndex(record.description);
        indexBuffer = record.name;
        indexOffset = bindings.indexOffset;
    }

    bindUniformBuffers(bindings);
    applied->vertexArray->bind(vertexBuffers, bindings.vertexOffsets, indexBuffer);

    const std::span<const ShaderProgram::Unit> units = applied->shader->getUnits();
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        const ImageRecord& image = images.get(bindings.textures[units[unit].texture].id);
        if (image.target != units[unit].target) {
            throw ContractError(ContractError::Rule::Bindings);
        }

        drawUnits[unit] = {.target = image.target, .texture = image.name, .sampler = samplers.get(bindings.samplers[units[unit].sampler].id)};
    }

    appliedUnits = units.size();
    bindApplied();
}

// Making an object of the device binds the program, the vertex array or the texture units it sets up, so the uniforms and the draws of the pipeline that the pass applied bind its state again, which the state cache makes free when nothing else bound since.
void Device::bindApplied() {
    state.useProgram(applied->shader->getName());
    state.bindVertexArray(applied->vertexArray->getName());
    for (std::size_t unit = 0; unit < appliedUnits; ++unit) {
        const DrawUnit& bound = drawUnits[unit];
        state.bindTexture(unit, bound.target, bound.texture);
        state.bindSampler(unit, bound.sampler);
    }
}

// A uniform block whose slot the bindings name a uniform buffer for reads the range of that buffer from the offset, which `applyUniforms` never fed since the pipeline, so no block has to choose between the two. The other blocks read what `applyUniforms` gives them.
void Device::bindUniformBuffers(const Bindings& bindings) {
    std::uint32_t fed = 0;
    for (const ShaderProgram::Block& block : applied->shader->getBlocks()) {
        const BufferRange& range = bindings.uniformBuffers[static_cast<std::size_t>(block.slot)];
        if (!range.buffer.isValid()) {
            continue;
        }

        const std::uint32_t bit = 1U << static_cast<unsigned>(block.slot);
        if ((appliedBlocks & bit) != 0) {
            throw ContractError(ContractError::Rule::Uniforms);
        }

        const BufferRecord& record = buffers.get(range.buffer.id);
        BufferRules::checkUniform(record.description, range.offset, block.size);
        if (block.active) {
            state.bindUniformRange(static_cast<std::size_t>(block.slot), {.buffer = record.name, .offset = static_cast<Gl::IntPtr>(range.offset), .size = static_cast<Gl::SizeiPtr>(block.size)});
        }

        fed |= bit;
    }

    bufferBlocks = fed;
}

void Device::applyUniforms(int slot, std::span<const std::byte> data) {
    requirePass();

    if (pass.skipped) {
        return;
    }

    if (applied == nullptr) {
        throw ContractError(ContractError::Rule::PipelineState);
    }

    const ShaderProgram::Block* block = applied->shader->findBlock(slot);
    if (block == nullptr || data.size() != block->size) {
        throw ContractError(ContractError::Rule::Uniforms);
    }

    const std::uint32_t bit = 1U << static_cast<unsigned>(slot);
    if ((bufferBlocks & bit) != 0) {
        throw ContractError(ContractError::Rule::Uniforms);
    }

    appliedBlocks |= bit;
    if (block->active) {
        const auto point = static_cast<std::size_t>(slot);
        state.bindUniformRange(point, uniformRing.write(point, data));
    }
}

void Device::applyStencilReference(std::uint8_t value) {
    requirePass();

    if (!pass.skipped) {
        state.applyStencilReference(value);
    }
}

void Device::applyBlendColor(math::Color value) {
    requirePass();

    if (!pass.skipped) {
        state.applyBlendColor(value);
    }
}

void Device::draw(int first, int count, int instances) {
    requirePass();

    if (applied == nullptr && !pass.skipped) {
        throw ContractError(ContractError::Rule::PipelineState);
    }

    if (first < 0 || count < 0 || instances < 0) {
        throw ContractError(ContractError::Rule::Draw);
    }

    if (pass.skipped || count == 0 || instances == 0) {
        return;
    }

    bindApplied();
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
    requirePass();

    if (!pass.skipped && pass.swapchain && pass.surface == 0) {
        drewSwapchain = true;
    } else if (!pass.skipped && pass.swapchain) {
        drawables[static_cast<std::size_t>(pass.surface - 1)].drawn = true;
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

void Device::trim() {
    if (pass.open) {
        throw ContractError(ContractError::Rule::PassOrder);
    }

    if (context->lost) {
        return;
    }

    if (ring) {
        ring->trim();
    }

    uniformRing.trim();
}

std::size_t Device::getSpareBytes() const noexcept {
    return (ring ? ring->getSpareBytes() : 0) + uniformRing.getSpareBytes();
}

// The depth and stencil of the window are never kept past the frame, so they are invalidated before the swap, and the fence of the frame tells a later frame when its regions of the upload ring and of the uniform ring are free.
void Device::commit() {
    if (pass.open) {
        throw ContractError(ContractError::Rule::PassOrder);
    }

    beginFrame();
    timer.endFrame();
    presentCurrentDrawable();

    for (Drawable& drawable : drawables) {
        drawable.drawn = false;
        drawable.presented = false;
    }

    drewSwapchain = false;
    presentedSwapchain = false;

    if (surfaceAttached) {
        makeDrawableCurrent(0, *surfaceContext);
    }

    if (ring) {
        frameFences[frameIndex % kFramesInFlight] = gl.fenceSync(Gl::kSyncGpuCommandsComplete, 0);
    }

    debug.checkErrors("the frame");
    checkLost();

    ++frameIndex;
    frameStarted = false;
}

void Device::checkLost() {
    if (context->lost) {
        return;
    }
#if defined(__EMSCRIPTEN__)
    context->lost = emscripten_is_webgl_context_lost(emscripten_webgl_get_current_context()) != 0;
#else
    context->lost = gl.getFeatures().robustness && gl.getGraphicsResetStatus() != Gl::kNoError;
#endif
    if (context->lost) {
        core::Log::error("The OpenGL context of the GPU device was lost, so the device stops drawing.");
    }
}

// A size that changes during a frame applies from the next one, so the passes of a frame draw into one framebuffer.
void Device::resizeSurface(const Surface::Size& size) {
    nextSurfaceSize = size;

    if (!frameStarted) {
        applySizes();
    }
}

// The platform may make another drawable current or none while the window is away, so the next pass makes its drawable current again.
void Device::detachSurface() {
    surfaceAttached = false;
    currentSurface = -1;
}

// The objects of the device live in its context, so the platform keeps the context while the window is away and hands the same one back with the new drawable, whose size replaces any size that the window had before it went.
void Device::attachSurface(const Surface& surface) {
    if (surface.kind != Surface::Kind::Gl || surface.gl != surfaceContext) {
        throw ContractError(ContractError::Rule::Context);
    }

    surfaceContext->makeCurrent();
    surfaceContext->setSwapInterval(surface.vsync ? 1 : 0);
    currentSurface = 0;
    surfaceSize = surface.size;
    nextSurfaceSize.reset();
    scaleDrawable(surfaceScaled, surface.size);
    surfaceAttached = true;
    surfaceVsync = surface.vsync;
}

std::unique_ptr<Device::Readback> Device::readImage(Image image, int width, int height) {
    const ImageRecord& record = images.get(image.id);
    if (record.kind != ImageKind::RenderTarget || record.sampleCount != 1) {
        throw ContractError(ContractError::Rule::Readback);
    }

    if (width <= 0 || height <= 0 || width > record.width || height > record.height) {
        throw ContractError(ContractError::Rule::Readback);
    }

    const Gl::Uint framebuffer = getSingleFramebuffer(image, "haylen-readback");
    return std::make_unique<PixelReadback>(context, framebuffer, width, height, record.pixel.floating || record.pixel.halfFloating || record.pixel.packedFloating);
}

// The copy takes its place in the command stream after the frames that the device committed, so it reads what they left in the buffer.
std::unique_ptr<Device::Readback> Device::readBuffer(Buffer buffer, std::size_t offset, std::size_t size) {
    if (pass.open) {
        throw ContractError(ContractError::Rule::Readback);
    }

    const BufferRecord& record = buffers.get(buffer.id);
    BufferRules::checkRead(record.description, offset, size);
    if (context->lost) {
        throw std::runtime_error("The OpenGL device stopped working, so it reads back no buffer.");
    }

    return std::make_unique<BufferReadback>(context, record.name, offset, size);
}

// The drawable takes the swap interval of its surface while it is current, and the window takes its own back, since some platforms keep the interval with the context rather than with the drawable.
int Device::addSurface(const Surface& added) {
    if (added.kind != Surface::Kind::Gl || added.gl == nullptr) {
        throw std::runtime_error("The OpenGL device draws only into a surface that holds another drawable of the context of the window, which the platform makes for it.");
    }

    added.gl->makeCurrent();
    added.gl->setSwapInterval(added.vsync ? 1 : 0);
    currentSurface = -1;

    if (surfaceAttached) {
        makeDrawableCurrent(0, *surfaceContext);
        surfaceContext->setSwapInterval(surfaceVsync ? 1 : 0);
    }

    Drawable made{.context = added.gl, .size = added.size};
    scaleDrawable(made.scaled, added.size);
    const auto free = std::ranges::find(drawables, nullptr, &Drawable::context);
    if (free != drawables.end()) {
        *free = std::move(made);
        return static_cast<int>(free - drawables.begin()) + 1;
    }

    drawables.push_back(std::move(made));
    return static_cast<int>(drawables.size());
}

void Device::resizeSurface(int number, const Surface::Size& size) {
    Drawable& drawable = requireDrawable(number);
    drawable.nextSize = size;

    if (!frameStarted) {
        applySizes();
    }
}

void Device::applySizes() {
    if (nextSurfaceSize) {
        surfaceSize = *nextSurfaceSize;
        scaleDrawable(surfaceScaled, surfaceSize);
        nextSurfaceSize.reset();
    }

    for (Drawable& drawable : drawables) {
        if (drawable.nextSize) {
            drawable.size = *drawable.nextSize;
            scaleDrawable(drawable.scaled, drawable.size);
            drawable.nextSize.reset();
        }
    }
}

// The window becomes current again before the drawable goes, since the platform destroys the drawable once the surface is removed.
void Device::removeSurface(int number) {
    Drawable& drawable = requireDrawable(number);
    if (drawable.drawn) {
        throw ContractError(ContractError::Rule::Surface);
    }

    if (currentSurface == number) {
        currentSurface = -1;

        if (surfaceAttached) {
            makeDrawableCurrent(0, *surfaceContext);
        }
    }

    drawable = {};
}

Device::Drawable& Device::requireDrawable(int number) {
    if (number < 1 || static_cast<std::size_t>(number) > drawables.size() || drawables[static_cast<std::size_t>(number - 1)].context == nullptr) {
        throw ContractError(ContractError::Rule::Surface);
    }

    return drawables[static_cast<std::size_t>(number - 1)];
}

// A surface whose window shows its drawable larger draws into a framebuffer of the size of the drawable, with the samples of the window, and the others draw into their default framebuffer.
void Device::scaleDrawable(std::unique_ptr<ScaledDrawable>& scaled, const Surface::Size& size) {
    if (!size.isScaled() || size.width <= 0 || size.height <= 0) {
        scaled.reset();
        return;
    }

    if (scaled == nullptr) {
        scaled = std::make_unique<ScaledDrawable>(gl, state, size, surfaceSamples);
        return;
    }

    scaled->resize(size);
}

void Device::makeDrawableCurrent(int number, GlContext& drawable) {
    if (currentSurface != number) {
        drawable.makeCurrent();
        currentSurface = number;
    }
}

// Some drivers, such as the software rasterizer of Mesa, keep the back buffer of a drawable only while it is current, so a pass into another drawable presents the drawable it leaves. The swapchain passes of a frame into one surface therefore follow one another.
void Device::requireUnpresented(int surface) {
    const bool presented = surface == 0 ? presentedSwapchain : requireDrawable(surface).presented;
    if (presented) {
        throw ContractError(ContractError::Rule::Surface);
    }
}

void Device::leaveDrawable(int next) {
    if (currentSurface != next) {
        presentCurrentDrawable();
    }
}

void Device::presentCurrentDrawable() {
    if (currentSurface == 0 && drewSwapchain) {
        presentDrawable(0, *surfaceContext, surfaceScaled.get());
        drewSwapchain = false;
        presentedSwapchain = true;
        return;
    }

    if (currentSurface > 0) {
        Drawable& drawable = requireDrawable(currentSurface);
        if (drawable.drawn) {
            presentDrawable(currentSurface, *drawable.context, drawable.scaled.get());
            drawable.drawn = false;
            drawable.presented = true;
        }
    }
}

// The depth and stencil of a drawable are never kept past the frame, so they are invalidated before the swap, and a drawable that its window shows larger is drawn over the window first.
void Device::presentDrawable(int number, GlContext& drawable, ScaledDrawable* scaled) {
    makeDrawableCurrent(number, drawable);

    if (scaled != nullptr) {
        state.bindDrawFramebuffer(scaled->getFramebuffer());
        const std::array<Gl::Enum, 1> depth{Gl::kDepthStencilAttachment};
        gl.invalidateFramebuffer(Gl::kDrawFramebuffer, static_cast<Gl::Sizei>(depth.size()), depth.data());
        scaled->present();
    }

    const std::array<Gl::Enum, 2> attachments{Gl::kDepth, Gl::kStencil};
    state.bindDrawFramebuffer(0);
    gl.invalidateFramebuffer(Gl::kDrawFramebuffer, static_cast<Gl::Sizei>(attachments.size()), attachments.data());
    drawable.swapBuffers();
}

// RenderDoc names an OpenGL device by its context, which the platform made, so a capture covers every context of the app.
gpu::Device::CaptureSupport Device::getCaptureSupport() const {
#if defined(__EMSCRIPTEN__)
    return {.tool = CaptureTool::None, .missing = std::string(kMissingWebCaptureTool)};
#else
    if (!renderDoc) {
        return {.tool = CaptureTool::None, .missing = std::string(renderdoc::Capturer::kMissing)};
    }

    return {.tool = CaptureTool::RenderDoc, .missing = {}};
#endif
}

void Device::beginCapture(const std::filesystem::path& path) {
    if (pass.open) {
        throw ContractError(ContractError::Rule::Capture);
    }
#if defined(__EMSCRIPTEN__)
    static_cast<void>(path);
    throw std::logic_error(std::string(kMissingWebCaptureTool));
#else
    if (!renderDoc) {
        throw std::logic_error(std::string(renderdoc::Capturer::kMissing));
    }

    renderDoc->begin(nullptr, path);
#endif
}

std::filesystem::path Device::endCapture() {
#if defined(__EMSCRIPTEN__)
    throw ContractError(ContractError::Rule::Capture);
#else
    if (!renderDoc) {
        throw ContractError(ContractError::Rule::Capture);
    }

    return renderDoc->end(nullptr);
#endif
}

} // namespace haylen::gpu::opengl
