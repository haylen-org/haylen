#include "gpu/metal/Device.hpp"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <format>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gpu/metal/EncoderState.hpp"
#include "gpu/metal/Formats.hpp"
#include "gpu/metal/Graveyard.hpp"
#include "gpu/metal/ImageReadback.hpp"
#include "gpu/metal/LibraryCache.hpp"
#include "gpu/metal/ObjectPool.hpp"
#include "gpu/metal/Swapchain.hpp"
#include "gpu/metal/UploadRing.hpp"
#include "haylen/core/Log.hpp"

namespace haylen::gpu::metal {

// The Metal objects of the device and the state of the frame and the pass that record.
struct Device::Native {
    // Uniform blocks take the buffer indices from 0 of each stage, so vertex buffers take the ones after them.
    static constexpr NSUInteger kFirstVertexBuffer = kMaxBlocks;

    // Every region of a dynamic buffer starts at a multiple of this many bytes, which every Metal GPU accepts as the offset of a vertex buffer.
    static constexpr std::size_t kRegionAlignment = 256;

    static constexpr std::size_t kUploadRingSize = 256 * 1024;

    // The id of a debug group pushed into the command buffer, and of one pushed while a pass that records nothing was open.
    static constexpr std::uint32_t kCommandBufferGroup = 0;
    static constexpr std::uint32_t kClosedGroup = UINT32_MAX;

    struct BufferEntry {
        id<MTLBuffer> buffer = nil;
        std::byte* contents = nullptr;
        std::size_t size = 0;
        std::size_t region = 0;
        bool dynamic = false;
        bool index = false;
    };

    struct ImageEntry {
        id<MTLTexture> texture = nil;
        TextureType type = TextureType::Flat;
        int width = 0;
        int height = 0;
        int layers = 1;
        int mipLevels = 1;
        int sampleCount = 1;
        PixelFormat format = PixelFormat::Rgba8;
        ImageKind kind = ImageKind::Texture;
    };

    struct BlockSlot {
        bool used = false;
        Stage stage = Stage::Vertex;
        std::uint32_t size = 0;
        NSUInteger binding = 0;
    };

    struct ResourceSlot {
        std::size_t slot = 0;
        Stage stage = Stage::Fragment;
        NSUInteger binding = 0;
    };

    struct ProgramEntry {
        id<MTLLibrary> vertexLibrary = nil;
        id<MTLLibrary> fragmentLibrary = nil;
        id<MTLFunction> vertex = nil;
        id<MTLFunction> fragment = nil;
        std::array<BlockSlot, kMaxBlocks> blocks{};
        std::vector<ResourceSlot> textures;
        std::vector<ResourceSlot> samplers;
    };

    struct PipelineEntry {
        id<MTLRenderPipelineState> state = nil;
        id<MTLDepthStencilState> depthStencil = nil;
        Program program;
        MTLPrimitiveType primitive = MTLPrimitiveTypeTriangle;
        MTLIndexType indexType = MTLIndexTypeUInt16;
        NSUInteger indexSize = 0;
        MTLCullMode cull = MTLCullModeNone;
        MTLWinding winding = MTLWindingCounterClockwise;
        float depthBias = 0.0F;
        float depthBiasSlope = 0.0F;
        std::size_t buffers = 0;
    };

    // What draws need from the pipeline that the pass applied, copied so objects made during the pass never move it.
    struct Applied {
        Program program;
        MTLPrimitiveType primitive = MTLPrimitiveTypeTriangle;
        MTLIndexType indexType = MTLIndexTypeUInt16;
        NSUInteger indexSize = 0;
        std::size_t buffers = 0;
        bool valid = false;
    };

    // What the completion handlers of the command buffers report to the frame thread, which outlives a device whose last frames still run.
    struct Signals {
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(kFramesInFlight);
        std::atomic<std::uint64_t> completed = 0;
        std::atomic<double> gpuSeconds = 0.0;
        std::atomic<bool> lost = false;
        std::mutex mutex;
        std::vector<std::string> errors;
    };

    Native(id<MTLDevice> metal, int swapchainSamples, bool debug);

    [[nodiscard]] static Capabilities describe(id<MTLDevice> metal);
    [[nodiscard]] static bool hasTileMemory(id<MTLDevice> metal);
    [[nodiscard]] static NSString* toText(std::string_view text);
    [[nodiscard]] static std::string describeError(id<MTLCommandBuffer> commands);
    [[nodiscard]] static bool isLostError(NSInteger code) noexcept;
    static void complete(Signals& state, id<MTLCommandBuffer> commands, std::uint64_t frame);

    [[nodiscard]] BufferEntry& requireBuffer(Buffer buffer);
    [[nodiscard]] ImageEntry& requireImage(Image image);
    [[nodiscard]] ProgramEntry& requireProgram(Program program);
    [[nodiscard]] id<MTLBlitCommandEncoder> getTransferBlit();
    [[nodiscard]] id<MTLDepthStencilState> getDepthStencilState(const DepthStencil& desc, bool depth, bool stencil);
    [[nodiscard]] NSUInteger getRegionOffset(const BufferEntry& entry) const noexcept;
    [[nodiscard]] MTLTextureDescriptor* describeImage(const ImageDesc& desc) const;
    void releaseLibraries(const ProgramEntry& entry);
    static void copyLevel(id<MTLBlitCommandEncoder> blit, id<MTLBuffer> source, NSUInteger offset, const ImageEntry& image, int level);

    id<MTLDevice> device = nil;
    id<MTLCommandQueue> queue = nil;
    Capabilities capabilities;
    bool labels = false;
    bool unifiedMemory = false;
    bool depth24Stencil8 = false;
    std::size_t uniformAlignment = 256;
    std::shared_ptr<Signals> signals = std::make_shared<Signals>();
    Graveyard graveyard;
    Swapchain swapchain;
    LibraryCache libraries;
    std::vector<UploadRing> rings;

    ObjectPool<BufferEntry> buffers;
    ObjectPool<ImageEntry> images;
    ObjectPool<id<MTLSamplerState>> samplers;
    ObjectPool<ProgramEntry> programs;
    ObjectPool<PipelineEntry> pipelines;
    std::unordered_map<std::uint64_t, id<MTLDepthStencilState>> depthStencilStates;

    MTLCommandBufferDescriptor* commandsDesc = nil;
    MTLRenderPassDescriptor* passDesc = nil;

    // The frame that records, numbered from 1, or the next one between frames.
    std::uint64_t frameNumber = 1;
    std::size_t frameSlot = 0;
    bool frameOpen = false;
    int passes = 0;
    bool imagesWritten = false;
    id<MTLCommandBuffer> commands = nil;
    id<MTLCommandBuffer> transfer = nil;
    id<MTLBlitCommandEncoder> transferBlit = nil;
    std::vector<UploadRing::Allocation> staged;

    // The pass that records. A swapchain pass that ended keeps its encoder open while nothing else records, so a swapchain pass that keeps what it drew continues in it.
    EncoderState encoder;
    bool passOpen = false;
    bool swapchainPass = false;
    bool swapchainPending = false;
    std::uint32_t passSerial = 0;
    Applied applied;
    Bindings bindings;
    bool bindingsApplied = false;
    id<MTLBuffer> indexBuffer = nil;
    NSUInteger indexOffset = 0;
    std::vector<std::uint32_t> debugGroups;
};

Device::Native::Native(id<MTLDevice> metal, int swapchainSamples, bool debug) : device(metal), capabilities(describe(metal)), labels(debug), unifiedMemory(metal.hasUnifiedMemory), swapchain(metal, graveyard, swapchainSamples, hasTileMemory(metal)), libraries(metal) {
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    depth24Stencil8 = metal.depth24Stencil8PixelFormatSupported;
#endif
#if !TARGET_OS_SIMULATOR
    if ([metal supportsFamily:MTLGPUFamilyApple1]) {
        uniformAlignment = 16;
    }
#endif

    queue = [metal newCommandQueue];
    if (queue == nil) {
        throw std::runtime_error(std::format("The GPU \"{}\" gave no command queue to Metal.", metal.name.UTF8String));
    }
    queue.label = @"haylen-queue";

    rings.reserve(kFramesInFlight);
    for (int frame = 0; frame < kFramesInFlight; ++frame) {
        rings.emplace_back(metal, kUploadRingSize);
    }
    staged.reserve(16);
    debugGroups.reserve(16);

    // The device keeps every object a frame uses alive until the frame finished, so command buffers skip retaining them.
    commandsDesc = [MTLCommandBufferDescriptor new];
    commandsDesc.retainedReferences = NO;
    if (debug) {
        commandsDesc.errorOptions = MTLCommandBufferErrorOptionEncoderExecutionStatus;
    }
    passDesc = [MTLRenderPassDescriptor new];
}

// Apple GPUs blend into every color format, while the GPUs of Intel Macs blend into no format of 32 bit floats.
gpu::Device::Capabilities Device::Native::describe(id<MTLDevice> metal) {
    Capabilities made;
    made.maxImageSize = [metal supportsFamily:MTLGPUFamilyApple3] || [metal supportsFamily:MTLGPUFamilyMac2] ? 16384 : 8192;
    made.maxArrayLayers = 2048;
    for (const int samples : {8, 4, 2}) {
        if ([metal supportsTextureSampleCount:static_cast<NSUInteger>(samples)]) {
            made.maxSampleCount = samples;
            break;
        }
    }
    made.originTopLeft = true;
    made.independentBlend = true;
    made.multisampledRenderTargets = true;
    made.gpuTiming = true;

    const bool floats = [metal supportsFamily:MTLGPUFamilyApple1];
    for (std::size_t index = 0; index < made.blendableFormats.size(); ++index) {
        const auto format = static_cast<PixelFormat>(index);
        const bool wide = format == PixelFormat::R32Float || format == PixelFormat::Rg32Float || format == PixelFormat::Rgba32Float;
        made.blendableFormats[index] = !Formats::isDepth(format) && (!wide || floats);
    }
    return made;
}

// The simulator runs on the GPU of the Mac without the memoryless storage of Apple GPUs.
bool Device::Native::hasTileMemory(id<MTLDevice> metal) {
#if TARGET_OS_SIMULATOR
    static_cast<void>(metal);
    return false;
#else
    return [metal supportsFamily:MTLGPUFamilyApple1];
#endif
}

NSString* Device::Native::toText(std::string_view text) {
    return [[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding];
}

std::string Device::Native::describeError(id<MTLCommandBuffer> commands) {
    NSError* error = commands.error;
    std::string message = error == nil ? "The GPU gave no reason." : error.localizedDescription.UTF8String;
    NSArray* encoders = error.userInfo[MTLCommandBufferEncoderInfoErrorKey];
    for (id<MTLCommandBufferEncoderInfo> info in encoders) {
        if (info.errorState == MTLCommandEncoderErrorStateFaulted) {
            message += std::format(" The pass \"{}\" faulted.", info.label.UTF8String);
        }
    }
    return message;
}

// Only a Mac can lose its GPU itself, such as an external GPU that was unplugged, while every system can revoke the access of the app to the GPU or fail inside its driver.
bool Device::Native::isLostError(NSInteger code) noexcept {
#if TARGET_OS_OSX
    if (code == MTLCommandBufferErrorDeviceRemoved) {
        return true;
    }
#endif
    return code == MTLCommandBufferErrorAccessRevoked || code == MTLCommandBufferErrorInternal;
}

// Runs on a thread of Metal once the GPU finished the frame. Command buffers of one queue finish in the order they were committed.
void Device::Native::complete(Signals& state, id<MTLCommandBuffer> commands, std::uint64_t frame) {
    if (commands.status == MTLCommandBufferStatusCompleted) {
        state.gpuSeconds.store(commands.GPUEndTime - commands.GPUStartTime, std::memory_order_relaxed);
    } else if (commands.status == MTLCommandBufferStatusError) {
        const NSInteger code = commands.error.code;
        const bool lost = isLostError(code);
        std::string message = code == MTLCommandBufferErrorNotPermitted ? "The system refused the GPU work of a frame, which happens while the app is in the background." : std::format("The GPU failed a frame. {}", describeError(commands));
        if (lost && !state.lost.exchange(true)) {
            message += " The GPU stopped working, so the app starts again on a new device.";
        }
        const std::scoped_lock lock(state.mutex);
        state.errors.push_back(std::move(message));
    }

    std::uint64_t known = state.completed.load(std::memory_order_relaxed);
    while (known < frame && !state.completed.compare_exchange_weak(known, frame, std::memory_order_release)) {}
    dispatch_semaphore_signal(state.semaphore);
}

Device::Native::BufferEntry& Device::Native::requireBuffer(Buffer buffer) {
    BufferEntry* entry = buffers.get(buffer.id);
    if (entry == nullptr) {
        throw std::logic_error(std::format("The buffer {} does not exist.", buffer.id));
    }
    return *entry;
}

Device::Native::ImageEntry& Device::Native::requireImage(Image image) {
    ImageEntry* entry = images.get(image.id);
    if (entry == nullptr) {
        throw std::logic_error(std::format("The image {} does not exist.", image.id));
    }
    return *entry;
}

Device::Native::ProgramEntry& Device::Native::requireProgram(Program program) {
    ProgramEntry* entry = programs.get(program.id);
    if (entry == nullptr) {
        throw std::logic_error(std::format("The program {} does not exist.", program.id));
    }
    return *entry;
}

// Objects made with contents copy them through a command buffer of their own, which retains what it reads and runs before the frame that uses them.
id<MTLBlitCommandEncoder> Device::Native::getTransferBlit() {
    if (transferBlit == nil) {
        @autoreleasepool {
            transfer = [queue commandBuffer];
            transfer.label = @"haylen-transfers";
            transferBlit = [transfer blitCommandEncoder];
        }
    }
    return transferBlit;
}

// Pipelines with the same depth and stencil tests share one state object, so switching between them sends Metal nothing.
id<MTLDepthStencilState> Device::Native::getDepthStencilState(const DepthStencil& desc, bool depth, bool stencil) {
    // clang-format off
    const auto packFace = [](const StencilFace& face) {
        return static_cast<std::uint64_t>(face.compare) | (static_cast<std::uint64_t>(face.fail) << 3U) | (static_cast<std::uint64_t>(face.depthFail) << 6U) | (static_cast<std::uint64_t>(face.pass) << 9U);
    };
    // clang-format on
    const bool tested = stencil && desc.stencilEnabled;
    std::uint64_t key = depth ? static_cast<std::uint64_t>(desc.depthCompare) | (static_cast<std::uint64_t>(desc.depthWrite) << 3U) : static_cast<std::uint64_t>(CompareFunction::Always);
    if (tested) {
        key |= (1ULL << 4U) | (packFace(desc.stencilFront) << 5U) | (packFace(desc.stencilBack) << 17U) | (static_cast<std::uint64_t>(desc.stencilReadMask) << 29U) | (static_cast<std::uint64_t>(desc.stencilWriteMask) << 37U);
    }
    if (const auto found = depthStencilStates.find(key); found != depthStencilStates.end()) {
        return found->second;
    }

    MTLDepthStencilDescriptor* made = [MTLDepthStencilDescriptor new];
    made.depthCompareFunction = depth ? Formats::toCompareFunction(desc.depthCompare) : MTLCompareFunctionAlways;
    made.depthWriteEnabled = depth && desc.depthWrite;
    if (tested) {
        // clang-format off
        const auto makeFace = [&desc](const StencilFace& face) {
            MTLStencilDescriptor* described = [MTLStencilDescriptor new];
            described.stencilCompareFunction = Formats::toCompareFunction(face.compare);
            described.stencilFailureOperation = Formats::toStencilOperation(face.fail);
            described.depthFailureOperation = Formats::toStencilOperation(face.depthFail);
            described.depthStencilPassOperation = Formats::toStencilOperation(face.pass);
            described.readMask = desc.stencilReadMask;
            described.writeMask = desc.stencilWriteMask;
            return described;
        };
        // clang-format on
        made.frontFaceStencil = makeFace(desc.stencilFront);
        made.backFaceStencil = makeFace(desc.stencilBack);
    }

    id<MTLDepthStencilState> state = [device newDepthStencilStateWithDescriptor:made];
    if (state == nil) {
        throw std::runtime_error("Metal could not make the depth and stencil state of a pipeline.");
    }
    depthStencilStates.emplace(key, state);
    return state;
}

NSUInteger Device::Native::getRegionOffset(const BufferEntry& entry) const noexcept {
    return entry.dynamic ? frameSlot * entry.region : 0;
}

MTLTextureDescriptor* Device::Native::describeImage(const ImageDesc& desc) const {
    MTLTextureDescriptor* made = [MTLTextureDescriptor new];
    switch (desc.type) {
    case TextureType::Flat:
        made.textureType = desc.sampleCount > 1 ? MTLTextureType2DMultisample : MTLTextureType2D;
        break;
    case TextureType::Array:
        made.textureType = desc.sampleCount > 1 ? MTLTextureType2DMultisampleArray : MTLTextureType2DArray;
        made.arrayLength = static_cast<NSUInteger>(desc.layers);
        break;
    case TextureType::Volume:
        made.textureType = MTLTextureType3D;
        made.depth = static_cast<NSUInteger>(desc.layers);
        break;
    case TextureType::Cube:
        made.textureType = MTLTextureTypeCube;
        break;
    }
    made.pixelFormat = Formats::toPixelFormat(desc.format, depth24Stencil8);
    made.width = static_cast<NSUInteger>(desc.width);
    made.height = static_cast<NSUInteger>(desc.height);
    made.mipmapLevelCount = static_cast<NSUInteger>(desc.mipLevels);
    made.sampleCount = static_cast<NSUInteger>(desc.sampleCount);
    made.storageMode = MTLStorageModePrivate;
    made.usage = desc.kind == ImageKind::RenderTarget || desc.kind == ImageKind::DepthStencil ? MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget : MTLTextureUsageShaderRead;
    return made;
}

void Device::Native::releaseLibraries(const ProgramEntry& entry) {
    if (entry.vertexLibrary != nil) {
        libraries.release(entry.vertexLibrary);
    }
    if (entry.fragmentLibrary != nil) {
        libraries.release(entry.fragmentLibrary);
    }
}

// The pixels of a level sit row after row and layer after layer from the offset, and a volume copies all its slices at once.
void Device::Native::copyLevel(id<MTLBlitCommandEncoder> blit, id<MTLBuffer> source, NSUInteger offset, const ImageEntry& image, int level) {
    const auto width = static_cast<NSUInteger>(std::max(1, image.width >> level));
    const auto height = static_cast<NSUInteger>(std::max(1, image.height >> level));
    const NSUInteger rowBytes = width * Formats::getBytesPerPixel(image.format);
    const NSUInteger layerBytes = rowBytes * height;
    if (image.type == TextureType::Volume) {
        const auto depth = static_cast<NSUInteger>(std::max(1, image.layers >> level));
        [blit copyFromBuffer:source sourceOffset:offset sourceBytesPerRow:rowBytes sourceBytesPerImage:layerBytes sourceSize:MTLSizeMake(width, height, depth) toTexture:image.texture destinationSlice:0 destinationLevel:static_cast<NSUInteger>(level) destinationOrigin:MTLOriginMake(0, 0, 0)];
        return;
    }
    for (int layer = 0; layer < image.layers; ++layer) {
        [blit copyFromBuffer:source sourceOffset:offset + static_cast<NSUInteger>(layer) * layerBytes sourceBytesPerRow:rowBytes sourceBytesPerImage:layerBytes sourceSize:MTLSizeMake(width, height, 1) toTexture:image.texture destinationSlice:static_cast<NSUInteger>(layer) destinationLevel:static_cast<NSUInteger>(level) destinationOrigin:MTLOriginMake(0, 0, 0)];
    }
}

std::unique_ptr<gpu::Device> Device::create(const std::optional<Surface>& surface, const DeviceFactory::Options& options) {
    if (surface && (surface->kind != Surface::Kind::MetalLayer || surface->layer == nullptr)) {
        throw std::runtime_error("The Metal device draws only into a surface that holds a \"CAMetalLayer\".");
    }

    // Metal loads its validation layer with the first device of the process when the environment asks for it.
    if (options.validation) {
        setenv("MTL_DEBUG_LAYER", "1", 0);
    }

    id<MTLDevice> metal = nil;
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    if (surface) {
        metal = ((__bridge CAMetalLayer*)surface->layer).preferredDevice;
    }
#endif
    if (metal == nil) {
        metal = MTLCreateSystemDefaultDevice();
    }
    if (metal == nil) {
        throw std::runtime_error("This machine has no GPU that runs Metal.");
    }

    const int samples = surface ? surface->sampleCount : 1;
    if (samples < 1 || ![metal supportsTextureSampleCount:static_cast<NSUInteger>(samples)]) {
        throw std::runtime_error(std::format("The GPU \"{}\" cannot draw the window with {} samples.", metal.name.UTF8String, samples));
    }

    std::unique_ptr<Device> device(new Device(std::make_unique<Native>(metal, samples, options.validation)));
    if (surface) {
        device->attachSurface(*surface);
    }
    core::Log::info("The Metal device draws on the GPU \"{}\".", metal.name.UTF8String);
    return device;
}

Device::Device(std::unique_ptr<Native> state) : native(std::move(state)) {}

// The frames that the GPU still runs signal the semaphore as they finish, and it goes back to its first count before it goes, as libdispatch requires.
Device::~Device() {
    if (native->encoder.get() != nil) {
        native->encoder.end();
    }
    if (native->transferBlit != nil) {
        [native->transferBlit endEncoding];
        [native->transfer commit];
    }

    const int taken = native->frameOpen ? 1 : 0;
    for (int frame = taken; frame < kFramesInFlight; ++frame) {
        dispatch_semaphore_wait(native->signals->semaphore, DISPATCH_TIME_FOREVER);
    }
    for (int frame = 0; frame < kFramesInFlight; ++frame) {
        dispatch_semaphore_signal(native->signals->semaphore);
    }
    reportErrors();
}

gpu::Device::Language Device::getLanguage() const noexcept {
#if TARGET_OS_SIMULATOR
    return Language::MetalIosSimulator;
#elif TARGET_OS_OSX
    return Language::MetalMacos;
#else
    return Language::MetalIos;
#endif
}

gpu::Device::Api Device::getApi() const noexcept {
    return Api::Metal;
}

const gpu::Device::Capabilities& Device::getCapabilities() const noexcept {
    return native->capabilities;
}

std::string Device::getAdapterName() const {
    return native->device.name.UTF8String;
}

std::optional<double> Device::getGpuFrameSeconds() const noexcept {
    const double seconds = native->signals->gpuSeconds.load(std::memory_order_relaxed);
    return seconds > 0.0 ? std::optional<double>(seconds) : std::nullopt;
}

std::vector<gpu::Device::Pool> Device::getPools() const {
    return {
        {.name = "images", .used = native->images.size(), .size = native->images.getCapacity()}, {.name = "buffers", .used = native->buffers.size(), .size = native->buffers.getCapacity()}, {.name = "samplers", .used = native->samplers.size(), .size = native->samplers.getCapacity()}, {.name = "programs", .used = native->programs.size(), .size = native->programs.getCapacity()}, {.name = "pipelines", .used = native->pipelines.size(), .size = native->pipelines.getCapacity()},
    };
}

bool Device::isLost() const noexcept {
    return native->signals->lost.load(std::memory_order_relaxed);
}

// Dynamic buffers hold a region for each frame in flight, which the CPU writes while the GPU reads the regions of the frames before. Immutable buffers live in shared memory on GPUs that share the memory of the CPU, and in the memory of the GPU elsewhere.
gpu::Device::Buffer Device::makeBuffer(const BufferDesc& desc) {
    const std::size_t size = desc.data.empty() ? desc.size : desc.data.size();
    if (size == 0) {
        throw std::logic_error(std::format("The buffer \"{}\" has no size.", desc.label));
    }
    if (desc.usage == Usage::Immutable && desc.data.empty()) {
        throw std::logic_error(std::format("The immutable buffer \"{}\" has no data.", desc.label));
    }

    Native::BufferEntry entry{.size = size, .dynamic = desc.usage == Usage::Dynamic, .index = desc.index};
    @autoreleasepool {
        if (entry.dynamic) {
            entry.region = (size + Native::kRegionAlignment - 1U) & ~(Native::kRegionAlignment - 1U);
            entry.buffer = [native->device newBufferWithLength:entry.region * kFramesInFlight options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
            entry.contents = entry.buffer == nil ? nullptr : static_cast<std::byte*>(entry.buffer.contents);
            for (std::size_t frame = 0; entry.contents != nullptr && !desc.data.empty() && frame < kFramesInFlight; ++frame) {
                std::memcpy(entry.contents + frame * entry.region, desc.data.data(), size);
            }
        } else if (native->unifiedMemory) {
            entry.buffer = [native->device newBufferWithBytes:desc.data.data() length:size options:MTLResourceStorageModeShared];
        } else {
            entry.buffer = [native->device newBufferWithLength:size options:MTLResourceStorageModePrivate];
            id<MTLBuffer> staging = [native->device newBufferWithBytes:desc.data.data() length:size options:MTLResourceStorageModeShared];
            if (entry.buffer != nil && staging != nil) {
                [native->getTransferBlit() copyFromBuffer:staging sourceOffset:0 toBuffer:entry.buffer destinationOffset:0 size:size];
            }
        }
    }
    if (entry.buffer == nil) {
        throw std::runtime_error(std::format("The GPU has no memory for the buffer \"{}\" of {} bytes.", desc.label, size));
    }
    if (native->labels && !desc.label.empty()) {
        entry.buffer.label = Native::toText(desc.label);
    }
    return {native->buffers.add(std::move(entry))};
}

gpu::Device::Image Device::makeImage(const ImageDesc& desc) {
    const int maxSize = native->capabilities.maxImageSize;
    if (desc.width <= 0 || desc.height <= 0 || desc.width > maxSize || desc.height > maxSize || desc.layers <= 0 || desc.mipLevels <= 0) {
        throw std::logic_error(std::format("The image \"{}\" of {}x{} pixels does not fit the device.", desc.label, desc.width, desc.height));
    }
    if (desc.type == TextureType::Cube && desc.layers != 6) {
        throw std::logic_error(std::format("The cube image \"{}\" needs 6 layers, not {}.", desc.label, desc.layers));
    }
    if (desc.sampleCount > 1 && (desc.kind == ImageKind::Texture || desc.kind == ImageKind::Dynamic || desc.mipLevels > 1 || desc.type == TextureType::Volume || desc.type == TextureType::Cube || desc.sampleCount > native->capabilities.maxSampleCount)) {
        throw std::logic_error(std::format("The image \"{}\" cannot have {} samples, which only flat or array attachments of one level up to {} samples have.", desc.label, desc.sampleCount, native->capabilities.maxSampleCount));
    }
    if (Formats::isDepth(desc.format) != (desc.kind == ImageKind::DepthStencil)) {
        throw std::logic_error(std::format("The image \"{}\" needs a depth format exactly when it is a depth stencil image.", desc.label));
    }
    if ((desc.kind == ImageKind::Texture) != !desc.levels.empty()) {
        throw std::logic_error(std::format("The image \"{}\" comes with pixels exactly when it is a texture.", desc.label));
    }
    if (desc.kind == ImageKind::Texture && desc.levels.size() != static_cast<std::size_t>(desc.mipLevels)) {
        throw std::logic_error(std::format("The texture \"{}\" has {} levels and comes with the pixels of {}.", desc.label, desc.mipLevels, desc.levels.size()));
    }

    Native::ImageEntry entry{.type = desc.type, .width = desc.width, .height = desc.height, .layers = desc.layers, .mipLevels = desc.mipLevels, .sampleCount = desc.sampleCount, .format = desc.format, .kind = desc.kind};
    @autoreleasepool {
        entry.texture = [native->device newTextureWithDescriptor:native->describeImage(desc)];
    }
    if (entry.texture == nil) {
        throw std::runtime_error(std::format("The GPU has no memory for the image \"{}\" of {}x{} pixels.", desc.label, desc.width, desc.height));
    }
    if (native->labels && !desc.label.empty()) {
        entry.texture.label = Native::toText(desc.label);
    }

    const Image image{native->images.add(std::move(entry))};
    if (desc.kind == ImageKind::Texture) {
        try {
            uploadLevels(image, desc.levels);
        } catch (...) {
            destroy(image);
            throw;
        }
    }
    return image;
}

// The pixels go into a staging buffer of their own, which the transfer command buffer retains until it copied them.
void Device::uploadLevels(Image image, std::span<const std::span<const std::byte>> levels) {
    const Native::ImageEntry& entry = native->requireImage(image);
    const std::size_t pixel = Formats::getBytesPerPixel(entry.format);
    std::vector<std::size_t> offsets(levels.size());
    std::size_t total = 0;
    for (std::size_t level = 0; level < levels.size(); ++level) {
        const auto shift = static_cast<int>(level);
        const auto layers = static_cast<std::size_t>(entry.type == TextureType::Volume ? std::max(1, entry.layers >> shift) : entry.layers);
        const std::size_t expected = static_cast<std::size_t>(std::max(1, entry.width >> shift)) * static_cast<std::size_t>(std::max(1, entry.height >> shift)) * layers * pixel;
        if (levels[level].size() != expected) {
            throw std::logic_error(std::format("The level {} of a texture holds {} bytes instead of {}.", level, levels[level].size(), expected));
        }
        offsets[level] = total;
        total += (expected + 15U) & ~std::size_t{15U};
    }

    @autoreleasepool {
        id<MTLBuffer> staging = [native->device newBufferWithLength:total options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
        if (staging == nil) {
            throw std::runtime_error(std::format("The GPU has no memory to stage the {} bytes of a texture.", total));
        }
        auto* contents = static_cast<std::byte*>(staging.contents);
        const id<MTLBlitCommandEncoder> blit = native->getTransferBlit();
        for (std::size_t level = 0; level < levels.size(); ++level) {
            std::memcpy(contents + offsets[level], levels[level].data(), levels[level].size());
            Native::copyLevel(blit, staging, offsets[level], entry, static_cast<int>(level));
        }
    }
}

gpu::Device::Sampler Device::makeSampler(const SamplerDesc& desc) {
    id<MTLSamplerState> sampler = nil;
    @autoreleasepool {
        MTLSamplerDescriptor* made = [MTLSamplerDescriptor new];
        made.minFilter = Formats::toFilter(desc.minification);
        made.magFilter = Formats::toFilter(desc.magnification);
        made.mipFilter = Formats::toMipFilter(desc.mipmap);
        made.sAddressMode = Formats::toAddressMode(desc.wrapU);
        made.tAddressMode = Formats::toAddressMode(desc.wrapV);
        made.rAddressMode = Formats::toAddressMode(desc.wrapW);
        made.lodMinClamp = desc.minLod;
        made.lodMaxClamp = desc.maxLod;
        made.maxAnisotropy = static_cast<NSUInteger>(std::clamp(desc.maxAnisotropy, 1, 16));
        if (desc.compare) {
            made.compareFunction = Formats::toCompareFunction(*desc.compare);
        }
        if (native->labels && !desc.label.empty()) {
            made.label = Native::toText(desc.label);
        }
        sampler = [native->device newSamplerStateWithDescriptor:made];
    }
    if (sampler == nil) {
        throw std::runtime_error(std::format("Metal could not make the sampler \"{}\".", desc.label));
    }
    return {native->samplers.add(sampler)};
}

// The stages compile through the library cache, where a program that `prepareProgram` announced finds them compiled or compiling on the I/O pool.
gpu::Device::Program Device::makeProgram(const ProgramDesc& desc) {
    if (desc.vertex.source.empty() || desc.fragment.source.empty()) {
        throw std::logic_error(std::format("The program \"{}\" has no Metal code.", desc.label));
    }

    Native::ProgramEntry entry;
    for (const BlockBinding& block : desc.blocks) {
        if (block.slot < 0 || block.slot >= static_cast<int>(kMaxBlocks) || block.size == 0 || block.binding < 0 || block.binding >= static_cast<int>(Native::kFirstVertexBuffer)) {
            throw std::logic_error(std::format("The program \"{}\" has a uniform block at slot {} that the device cannot bind.", desc.label, block.slot));
        }
        entry.blocks[static_cast<std::size_t>(block.slot)] = {.used = true, .stage = block.stage, .size = block.size, .binding = static_cast<NSUInteger>(block.binding)};
    }
    for (const TextureBinding& texture : desc.textures) {
        if (texture.slot < 0 || texture.slot >= static_cast<int>(kMaxTextures) || texture.binding < 0 || texture.binding >= static_cast<int>(EncoderState::kTextureSlots)) {
            throw std::logic_error(std::format("The program \"{}\" has a texture at slot {} that the device cannot bind.", desc.label, texture.slot));
        }
        entry.textures.push_back({.slot = static_cast<std::size_t>(texture.slot), .stage = texture.stage, .binding = static_cast<NSUInteger>(texture.binding)});
    }
    for (const SamplerBinding& sampler : desc.samplers) {
        if (sampler.slot < 0 || sampler.slot >= static_cast<int>(kMaxSamplers) || sampler.binding < 0 || sampler.binding >= static_cast<int>(EncoderState::kSamplerSlots)) {
            throw std::logic_error(std::format("The program \"{}\" has a sampler at slot {} that the device cannot bind.", desc.label, sampler.slot));
        }
        entry.samplers.push_back({.slot = static_cast<std::size_t>(sampler.slot), .stage = sampler.stage, .binding = static_cast<NSUInteger>(sampler.binding)});
    }

    const auto started = std::chrono::steady_clock::now();
    std::string error;
    entry.vertexLibrary = native->libraries.acquire(desc.vertex.source, error);
    if (entry.vertexLibrary == nil) {
        throw std::runtime_error(std::format("The vertex stage of the program \"{}\" does not compile for Metal. {}", desc.label, error));
    }
    entry.fragmentLibrary = native->libraries.acquire(desc.fragment.source, error);
    if (entry.fragmentLibrary == nil) {
        native->releaseLibraries(entry);
        throw std::runtime_error(std::format("The fragment stage of the program \"{}\" does not compile for Metal. {}", desc.label, error));
    }

    @autoreleasepool {
        entry.vertex = [entry.vertexLibrary newFunctionWithName:Native::toText(desc.vertex.entry)];
        entry.fragment = [entry.fragmentLibrary newFunctionWithName:Native::toText(desc.fragment.entry)];
    }
    if (entry.vertex == nil || entry.fragment == nil) {
        native->releaseLibraries(entry);
        throw std::runtime_error(std::format("The program \"{}\" has no function \"{}\" for its vertex stage or \"{}\" for its fragment stage.", desc.label, desc.vertex.entry, desc.fragment.entry));
    }
    if (native->labels && !desc.label.empty()) {
        entry.vertex.label = Native::toText(desc.label);
        entry.fragment.label = entry.vertex.label;
    }

    core::Log::debug("The Metal device made the program \"{}\" in {:.2f} ms.", desc.label, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count());
    return {native->programs.add(std::move(entry))};
}

void Device::prepareProgram(const ProgramDesc& desc, core::JobSystem& jobs) {
    if (!desc.vertex.source.empty()) {
        native->libraries.prepare(desc.vertex.source, jobs);
    }
    if (!desc.fragment.source.empty()) {
        native->libraries.prepare(desc.fragment.source, jobs);
    }
}

gpu::Device::Pipeline Device::makePipeline(const PipelineDesc& desc) {
    const Native::ProgramEntry& program = native->requireProgram(desc.program);
    if (desc.colors.size() > kMaxColorTargets || (desc.colors.empty() && !desc.swapchain && !desc.depthStencil.format)) {
        throw std::logic_error(std::format("The pipeline \"{}\" needs a depth format or one to {} color targets.", desc.label, kMaxColorTargets));
    }
    if (desc.buffers.size() > kMaxVertexBuffers) {
        throw std::logic_error(std::format("The pipeline \"{}\" lays out more than {} vertex buffers.", desc.label, kMaxVertexBuffers));
    }
    for (const VertexAttribute& attribute : desc.attributes) {
        if (attribute.buffer < 0 || attribute.buffer >= static_cast<int>(desc.buffers.size()) || attribute.location < 0 || attribute.location >= static_cast<int>(kMaxAttributes)) {
            throw std::logic_error(std::format("An attribute of the pipeline \"{}\" reads a vertex buffer it does not lay out.", desc.label));
        }
    }
    if (!desc.swapchain && desc.depthStencil.format && !Formats::isDepth(*desc.depthStencil.format)) {
        throw std::logic_error(std::format("The depth format of the pipeline \"{}\" is a color format.", desc.label));
    }

    const bool depth = desc.swapchain || desc.depthStencil.format.has_value();
    const bool stencil = desc.swapchain || (desc.depthStencil.format && Formats::hasStencil(*desc.depthStencil.format));
    Native::PipelineEntry entry{.program = desc.program, .primitive = Formats::toPrimitiveType(desc.primitive), .cull = Formats::toCullMode(desc.cullMode), .winding = Formats::toWinding(desc.frontFace), .depthBias = desc.depthStencil.depthBias, .depthBiasSlope = desc.depthStencil.depthBiasSlope, .buffers = desc.buffers.size()};
    if (desc.indexType != IndexType::None) {
        entry.indexType = desc.indexType == IndexType::UInt32 ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
        entry.indexSize = desc.indexType == IndexType::UInt32 ? 4 : 2;
    }

    NSError* error = nil;
    @autoreleasepool {
        MTLRenderPipelineDescriptor* made = [MTLRenderPipelineDescriptor new];
        made.vertexFunction = program.vertex;
        made.fragmentFunction = program.fragment;
        made.inputPrimitiveTopology = Formats::toTopologyClass(desc.primitive);
        made.rasterSampleCount = static_cast<NSUInteger>(desc.swapchain ? native->swapchain.getSampleCount() : desc.sampleCount);
        made.alphaToCoverageEnabled = desc.alphaToCoverage;

        MTLVertexDescriptor* vertices = [MTLVertexDescriptor vertexDescriptor];
        for (const VertexAttribute& attribute : desc.attributes) {
            MTLVertexAttributeDescriptor* described = vertices.attributes[static_cast<NSUInteger>(attribute.location)];
            described.format = Formats::toVertexFormat(attribute.format);
            described.offset = static_cast<NSUInteger>(attribute.offset);
            described.bufferIndex = Native::kFirstVertexBuffer + static_cast<NSUInteger>(attribute.buffer);
        }
        for (std::size_t index = 0; index < desc.buffers.size(); ++index) {
            MTLVertexBufferLayoutDescriptor* layout = vertices.layouts[Native::kFirstVertexBuffer + index];
            layout.stride = static_cast<NSUInteger>(desc.buffers[index].stride);
            layout.stepFunction = desc.buffers[index].perInstance ? MTLVertexStepFunctionPerInstance : MTLVertexStepFunctionPerVertex;
        }
        made.vertexDescriptor = vertices;

        const std::size_t targets = desc.swapchain ? 1U : desc.colors.size();
        for (std::size_t index = 0; index < targets; ++index) {
            const ColorTarget color = index < desc.colors.size() ? desc.colors[index] : ColorTarget{};
            MTLRenderPipelineColorAttachmentDescriptor* target = made.colorAttachments[index];
            target.pixelFormat = desc.swapchain ? Swapchain::kColorFormat : Formats::toPixelFormat(color.format, native->depth24Stencil8);
            target.writeMask = Formats::toColorWriteMask(color.writeMask);
            target.blendingEnabled = color.blend.enabled;
            target.sourceRGBBlendFactor = Formats::toBlendFactor(color.blend.sourceColor);
            target.destinationRGBBlendFactor = Formats::toBlendFactor(color.blend.destinationColor);
            target.rgbBlendOperation = Formats::toBlendOperation(color.blend.colorOperation);
            target.sourceAlphaBlendFactor = Formats::toBlendFactor(color.blend.sourceAlpha);
            target.destinationAlphaBlendFactor = Formats::toBlendFactor(color.blend.destinationAlpha);
            target.alphaBlendOperation = Formats::toBlendOperation(color.blend.alphaOperation);
        }

        const MTLPixelFormat depthFormat = desc.swapchain ? Swapchain::kDepthFormat : (depth ? Formats::toPixelFormat(*desc.depthStencil.format, native->depth24Stencil8) : MTLPixelFormatInvalid);
        made.depthAttachmentPixelFormat = depthFormat;
        made.stencilAttachmentPixelFormat = stencil ? depthFormat : MTLPixelFormatInvalid;
        if (native->labels && !desc.label.empty()) {
            made.label = Native::toText(desc.label);
        }
        entry.state = [native->device newRenderPipelineStateWithDescriptor:made error:&error];
    }
    if (entry.state == nil) {
        throw std::runtime_error(std::format("Metal could not make the pipeline \"{}\". {}", desc.label, error == nil ? "It gave no reason." : error.localizedDescription.UTF8String));
    }

    entry.depthStencil = native->getDepthStencilState(desc.depthStencil, depth, stencil);
    return {native->pipelines.add(std::move(entry))};
}

void Device::destroy(Buffer buffer) noexcept {
    if (std::optional<Native::BufferEntry> entry = native->buffers.remove(buffer.id)) {
        native->graveyard.retire(entry->buffer);
    }
}

void Device::destroy(Image image) noexcept {
    if (std::optional<Native::ImageEntry> entry = native->images.remove(image.id)) {
        native->graveyard.retire(entry->texture);
    }
}

void Device::destroy(Sampler sampler) noexcept {
    if (std::optional<id<MTLSamplerState>> entry = native->samplers.remove(sampler.id)) {
        native->graveyard.retire(*entry);
    }
}

// Pipelines hold the code of their program, so its functions go at once and only the libraries leave the cache.
void Device::destroy(Program program) noexcept {
    if (std::optional<Native::ProgramEntry> entry = native->programs.remove(program.id)) {
        native->releaseLibraries(*entry);
    }
}

void Device::destroy(Pipeline pipeline) noexcept {
    if (std::optional<Native::PipelineEntry> entry = native->pipelines.remove(pipeline.id)) {
        native->graveyard.retire(entry->state);
    }
}

void Device::writeBuffer(Buffer buffer, std::size_t offset, std::span<const std::byte> data) {
    const Native::BufferEntry& entry = native->requireBuffer(buffer);
    if (!entry.dynamic) {
        throw std::logic_error("Only a dynamic buffer takes new contents.");
    }
    if (native->passOpen) {
        throw std::logic_error("Buffers take new contents before the passes of a frame.");
    }
    if (offset + data.size() > entry.size) {
        throw std::logic_error(std::format("The buffer of {} bytes cannot take {} bytes at {}.", entry.size, data.size(), offset));
    }

    openFrame();
    std::memcpy(entry.contents + native->getRegionOffset(entry) + offset, data.data(), data.size());
}

// Every image goes into the upload ring of the frame before the blit copies any of them, and the blit runs before the first pass of the frame.
void Device::writeImages(std::span<const ImageWrite> writes) {
    if (native->frameOpen && native->passes > 0) {
        throw std::logic_error("Images take new pixels before the first pass of a frame.");
    }
    if (native->frameOpen && native->imagesWritten) {
        throw std::logic_error("The images of a frame take their pixels in one call, and this frame made it already.");
    }
    openFrame();
    native->imagesWritten = true;
    if (writes.empty()) {
        return;
    }

    native->staged.clear();
    UploadRing& ring = native->rings[native->frameSlot];
    for (const ImageWrite& write : writes) {
        const Native::ImageEntry& image = native->requireImage(write.image);
        if (image.kind != ImageKind::Dynamic) {
            throw std::logic_error("Only a dynamic image takes new pixels.");
        }
        const std::size_t expected = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * static_cast<std::size_t>(image.layers) * Formats::getBytesPerPixel(image.format);
        if (write.pixels.size() != expected) {
            throw std::logic_error(std::format("The pixels do not fill the image of {}x{}.", image.width, image.height));
        }
        native->staged.push_back(ring.write(write.pixels, 16));
    }

    @autoreleasepool {
        const id<MTLBlitCommandEncoder> blit = [native->commands blitCommandEncoder];
        for (std::size_t index = 0; index < writes.size(); ++index) {
            Native::copyLevel(blit, native->staged[index].buffer, native->staged[index].offset, native->requireImage(writes[index].image), 0);
        }
        [blit endEncoding];
    }
}

void Device::beginPass(const PassDesc& desc) {
    if (native->passOpen) {
        throw std::logic_error("A pass began while another pass was open.");
    }
    openFrame();

    ++native->passSerial;
    native->applied = {};
    native->bindingsApplied = false;
    if (desc.colors.empty() && !desc.depthStencil) {
        beginSwapchainPass(desc);
    } else {
        beginOffscreenPass(desc);
    }
    native->passOpen = true;
    ++native->passes;
}

// A pass that keeps what the swapchain pass before it drew continues in its encoder, so the color, the depth and the stencil stay in tile memory. Otherwise the multisampled color and the depth and stencil start cleared, since no pass stores them.
void Device::beginSwapchainPass(const PassDesc& desc) {
    native->swapchainPass = true;
    if (native->swapchainPending && desc.load == Load::Keep) {
        native->swapchainPending = false;
        native->encoder.resetViewport();
        return;
    }
    endPendingPass();
    if (!native->swapchain.acquire()) {
        return;
    }

    MTLRenderPassDescriptor* pass = native->passDesc;
    const id<MTLTexture> drawable = native->swapchain.getDrawable().texture;
    const id<MTLTexture> multisample = native->swapchain.getMultisampleTexture();
    const MTLLoadAction colorLoad = desc.load == Load::Keep && multisample != nil ? MTLLoadActionClear : Formats::toLoadAction(desc.load);
    for (NSUInteger index = 1; index < kMaxColorTargets; ++index) {
        pass.colorAttachments[index].texture = nil;
    }
    MTLRenderPassColorAttachmentDescriptor* color = pass.colorAttachments[0];
    color.texture = multisample != nil ? multisample : drawable;
    color.resolveTexture = multisample != nil ? drawable : nil;
    color.level = 0;
    color.slice = 0;
    color.depthPlane = 0;
    color.loadAction = colorLoad;
    color.storeAction = multisample != nil ? MTLStoreActionMultisampleResolve : MTLStoreActionStore;
    color.clearColor = Formats::toClearColor(desc.clear);

    const MTLLoadAction depthLoad = desc.load == Load::Keep ? MTLLoadActionClear : Formats::toLoadAction(desc.load);
    pass.depthAttachment.texture = native->swapchain.getDepthTexture();
    pass.depthAttachment.loadAction = depthLoad;
    pass.depthAttachment.storeAction = MTLStoreActionDontCare;
    pass.depthAttachment.clearDepth = desc.clearDepth;
    pass.stencilAttachment.texture = native->swapchain.getDepthTexture();
    pass.stencilAttachment.loadAction = depthLoad;
    pass.stencilAttachment.storeAction = MTLStoreActionDontCare;
    pass.stencilAttachment.clearStencil = desc.clearStencil;

    @autoreleasepool {
        native->encoder.begin([native->commands renderCommandEncoderWithDescriptor:pass], static_cast<int>(drawable.width), static_cast<int>(drawable.height));
    }
    if (native->labels && !desc.label.empty()) {
        native->encoder.get().label = Native::toText(desc.label);
    }
}

void Device::beginOffscreenPass(const PassDesc& desc) {
    native->swapchainPass = false;
    endPendingPass();
    if (desc.colors.size() > kMaxColorTargets) {
        throw std::logic_error(std::format("A pass draws into at most {} color attachments.", kMaxColorTargets));
    }

    MTLRenderPassDescriptor* pass = native->passDesc;
    int width = 0;
    int height = 0;
    for (std::size_t index = 0; index < kMaxColorTargets; ++index) {
        MTLRenderPassColorAttachmentDescriptor* color = pass.colorAttachments[index];
        color.texture = nil;
        color.resolveTexture = nil;
        if (index >= desc.colors.size()) {
            continue;
        }

        const ColorAttachment& attachment = desc.colors[index];
        const Native::ImageEntry& image = native->requireImage(attachment.image);
        if (image.kind != ImageKind::RenderTarget) {
            throw std::logic_error("A pass draws its colors only into render targets.");
        }
        color.texture = image.texture;
        color.level = static_cast<NSUInteger>(attachment.mipLevel);
        color.slice = image.type == TextureType::Volume ? 0 : static_cast<NSUInteger>(attachment.layer);
        color.depthPlane = image.type == TextureType::Volume ? static_cast<NSUInteger>(attachment.layer) : 0;
        color.loadAction = Formats::toLoadAction(attachment.load);
        color.clearColor = Formats::toClearColor(attachment.clear);
        color.storeAction = attachment.store == Store::Store ? MTLStoreActionStore : MTLStoreActionDontCare;
        if (attachment.resolve.isValid()) {
            color.resolveTexture = native->requireImage(attachment.resolve).texture;
            color.resolveLevel = 0;
            color.resolveSlice = 0;
            color.storeAction = attachment.store == Store::Store ? MTLStoreActionStoreAndMultisampleResolve : MTLStoreActionMultisampleResolve;
        }
        width = std::max(1, image.width >> attachment.mipLevel);
        height = std::max(1, image.height >> attachment.mipLevel);
    }

    pass.depthAttachment.texture = nil;
    pass.stencilAttachment.texture = nil;
    if (desc.depthStencil) {
        const DepthStencilAttachment& attachment = *desc.depthStencil;
        const Native::ImageEntry& image = native->requireImage(attachment.image);
        if (image.kind != ImageKind::DepthStencil) {
            throw std::logic_error("A pass draws its depth and stencil only into depth stencil images.");
        }
        pass.depthAttachment.texture = image.texture;
        pass.depthAttachment.loadAction = Formats::toLoadAction(attachment.depthLoad);
        pass.depthAttachment.storeAction = attachment.depthStore == Store::Store ? MTLStoreActionStore : MTLStoreActionDontCare;
        pass.depthAttachment.clearDepth = attachment.clearDepth;
        if (Formats::hasStencil(image.format)) {
            pass.stencilAttachment.texture = image.texture;
            pass.stencilAttachment.loadAction = Formats::toLoadAction(attachment.stencilLoad);
            pass.stencilAttachment.storeAction = attachment.stencilStore == Store::Store ? MTLStoreActionStore : MTLStoreActionDontCare;
            pass.stencilAttachment.clearStencil = attachment.clearStencil;
        }
        if (desc.colors.empty()) {
            width = image.width;
            height = image.height;
        }
    }

    @autoreleasepool {
        native->encoder.begin([native->commands renderCommandEncoderWithDescriptor:pass], width, height);
    }
    if (native->labels && !desc.label.empty()) {
        native->encoder.get().label = Native::toText(desc.label);
    }
}

void Device::requirePass(const char* call) const {
    if (!native->passOpen) {
        throw std::logic_error(std::format("The call \"{}\" happened outside a pass.", call));
    }
}

void Device::applyViewport(const math::Rect& pixels) {
    requirePass("applyViewport");
    if (native->encoder.get() != nil) {
        native->encoder.setViewport(pixels);
    }
}

void Device::applyScissor(const math::Rect& pixels) {
    requirePass("applyScissor");
    if (native->encoder.get() != nil) {
        native->encoder.setScissor(pixels);
    }
}

void Device::applyPipeline(Pipeline pipeline) {
    requirePass("applyPipeline");
    const Native::PipelineEntry* entry = native->pipelines.get(pipeline.id);
    if (entry == nullptr) {
        throw std::logic_error(std::format("The pipeline {} does not exist.", pipeline.id));
    }

    native->applied = {.program = entry->program, .primitive = entry->primitive, .indexType = entry->indexType, .indexSize = entry->indexSize, .buffers = entry->buffers, .valid = true};
    native->bindingsApplied = false;
    if (native->encoder.get() == nil) {
        return;
    }
    native->encoder.setPipeline(entry->state);
    native->encoder.setDepthStencil(entry->depthStencil);
    native->encoder.setRasterizer(entry->cull, entry->winding, entry->depthBias, entry->depthBiasSlope);
}

// Bindings equal to the ones the pass applied last with the same pipeline change nothing, and the others reach Metal only where they differ from what the encoder holds.
void Device::applyBindings(const Bindings& bindings) {
    static_assert(std::has_unique_object_representations_v<Bindings>, "Bindings compare as bytes.");
    requirePass("applyBindings");
    if (!native->applied.valid) {
        throw std::logic_error("Bindings were applied before a pipeline.");
    }
    if (native->encoder.get() == nil || (native->bindingsApplied && std::memcmp(&bindings, &native->bindings, sizeof(Bindings)) == 0)) {
        return;
    }

    for (std::size_t index = 0; index < native->applied.buffers; ++index) {
        const Native::BufferEntry& buffer = native->requireBuffer(bindings.vertexBuffers[index]);
        native->encoder.setBuffer(Stage::Vertex, buffer.buffer, native->getRegionOffset(buffer) + static_cast<NSUInteger>(bindings.vertexOffsets[index]), Native::kFirstVertexBuffer + index);
    }
    if (native->applied.indexSize != 0) {
        const Native::BufferEntry& buffer = native->requireBuffer(bindings.indexBuffer);
        if (!buffer.index) {
            throw std::logic_error("An indexed pipeline needs an index buffer.");
        }
        native->indexBuffer = buffer.buffer;
        native->indexOffset = native->getRegionOffset(buffer) + static_cast<NSUInteger>(bindings.indexOffset);
    }

    const Native::ProgramEntry& program = native->requireProgram(native->applied.program);
    for (const Native::ResourceSlot& texture : program.textures) {
        native->encoder.setTexture(texture.stage, native->requireImage(bindings.textures[texture.slot]).texture, texture.binding);
    }
    for (const Native::ResourceSlot& sampler : program.samplers) {
        const id<MTLSamplerState>* state = native->samplers.get(bindings.samplers[sampler.slot].id);
        if (state == nullptr) {
            throw std::logic_error(std::format("The sampler slot {} of the program is empty.", sampler.slot));
        }
        native->encoder.setSampler(sampler.stage, *state, sampler.binding);
    }

    native->bindings = bindings;
    native->bindingsApplied = true;
}

void Device::applyUniforms(int slot, std::span<const std::byte> data) {
    requirePass("applyUniforms");
    if (!native->applied.valid) {
        throw std::logic_error("Uniforms were applied before a pipeline.");
    }
    const Native::ProgramEntry& program = native->requireProgram(native->applied.program);
    if (slot < 0 || slot >= static_cast<int>(kMaxBlocks) || !program.blocks[static_cast<std::size_t>(slot)].used) {
        throw std::logic_error(std::format("The program has no uniform block at slot {}.", slot));
    }
    const Native::BlockSlot& block = program.blocks[static_cast<std::size_t>(slot)];
    if (data.size() != block.size) {
        throw std::logic_error(std::format("The uniform block at slot {} holds {} bytes, not {}.", slot, block.size, data.size()));
    }
    if (native->encoder.get() == nil) {
        return;
    }

    const UploadRing::Allocation uploaded = native->rings[native->frameSlot].write(data, native->uniformAlignment);
    native->encoder.setBuffer(block.stage, uploaded.buffer, uploaded.offset, block.binding);
}

void Device::applyStencilReference(std::uint8_t value) {
    requirePass("applyStencilReference");
    if (native->encoder.get() != nil) {
        native->encoder.setStencilReference(value);
    }
}

void Device::applyBlendColor(math::Color value) {
    requirePass("applyBlendColor");
    if (native->encoder.get() != nil) {
        native->encoder.setBlendColor(value);
    }
}

void Device::draw(int first, int count, int instances) {
    requirePass("draw");
    if (!native->applied.valid) {
        throw std::logic_error("A draw happened before a pipeline.");
    }
    if (first < 0 || count < 0 || instances < 0) {
        throw std::logic_error("A draw needs counts of zero or more.");
    }
    const id<MTLRenderCommandEncoder> encoder = native->encoder.get();
    if (encoder == nil || count == 0 || instances == 0) {
        return;
    }

    const Native::Applied& applied = native->applied;
    if (applied.indexSize == 0) {
        [encoder drawPrimitives:applied.primitive vertexStart:static_cast<NSUInteger>(first) vertexCount:static_cast<NSUInteger>(count) instanceCount:static_cast<NSUInteger>(instances)];
        return;
    }
    [encoder drawIndexedPrimitives:applied.primitive indexCount:static_cast<NSUInteger>(count) indexType:applied.indexType indexBuffer:native->indexBuffer indexBufferOffset:native->indexOffset + static_cast<NSUInteger>(first) * applied.indexSize instanceCount:static_cast<NSUInteger>(instances)];
}

void Device::endPass() {
    requirePass("endPass");
    closeDebugGroups(native->passSerial);
    native->passOpen = false;
    if (native->encoder.get() == nil) {
        return;
    }
    if (native->swapchainPass) {
        native->swapchainPending = true;
        return;
    }
    native->encoder.end();
}

void Device::pushDebugGroup(std::string_view name) {
    if (!native->labels) {
        return;
    }
    NSString* text = Native::toText(name);
    if (native->passOpen) {
        if (native->encoder.get() == nil) {
            native->debugGroups.push_back(Native::kClosedGroup);
            return;
        }
        [native->encoder.get() pushDebugGroup:text];
        native->debugGroups.push_back(native->passSerial);
        return;
    }
    openFrame();
    endPendingPass();
    [native->commands pushDebugGroup:text];
    native->debugGroups.push_back(Native::kCommandBufferGroup);
}

// A group whose pass ended or whose frame was committed closed with it, so its pop does nothing.
void Device::popDebugGroup() {
    if (!native->labels || native->debugGroups.empty()) {
        return;
    }
    const std::uint32_t owner = native->debugGroups.back();
    native->debugGroups.pop_back();
    if (owner == Native::kCommandBufferGroup) {
        endPendingPass();
        [native->commands popDebugGroup];
        return;
    }
    if (owner == native->passSerial && native->passOpen && native->encoder.get() != nil) {
        [native->encoder.get() popDebugGroup];
    }
}

void Device::closeDebugGroups(std::uint32_t owner) {
    for (std::uint32_t& group : native->debugGroups) {
        if (group != owner) {
            continue;
        }
        group = Native::kClosedGroup;
        if (owner == Native::kCommandBufferGroup) {
            [native->commands popDebugGroup];
        } else if (native->encoder.get() != nil) {
            [native->encoder.get() popDebugGroup];
        }
    }
}

// A frame starts once the GPU finished the frame that used its slot before, whose upload ring and dynamic regions it then reuses, and the objects that waited for finished frames go.
void Device::openFrame() {
    if (native->frameOpen) {
        return;
    }
    dispatch_semaphore_wait(native->signals->semaphore, DISPATCH_TIME_FOREVER);
    native->graveyard.release(native->signals->completed.load(std::memory_order_acquire));
    reportErrors();

    native->frameSlot = static_cast<std::size_t>(native->frameNumber % kFramesInFlight);
    native->rings[native->frameSlot].reset();
    @autoreleasepool {
        native->commands = [native->queue commandBufferWithDescriptor:native->commandsDesc];
    }
    if (native->labels) {
        native->commands.label = @"haylen-frame";
    }
    native->frameOpen = true;
    native->passes = 0;
    native->imagesWritten = false;
}

void Device::endPendingPass() {
    if (native->swapchainPending) {
        native->swapchainPending = false;
        native->encoder.end();
    }
}

void Device::submitTransfers() {
    if (native->transferBlit == nil) {
        return;
    }
    [native->transferBlit endEncoding];
    [native->transfer commit];
    native->transferBlit = nil;
    native->transfer = nil;
}

void Device::reportErrors() {
    Native::Signals& signals = *native->signals;
    const std::scoped_lock lock(signals.mutex);
    for (const std::string& error : signals.errors) {
        core::Log::error("{}", error);
    }
    signals.errors.clear();
}

// The copies of objects made with contents run before the frame, which presents its drawable once the GPU drew it.
void Device::commit() {
    if (native->passOpen) {
        throw std::logic_error("The frame was committed while a pass was open.");
    }
    submitTransfers();
    if (!native->frameOpen) {
        return;
    }
    endPendingPass();
    closeDebugGroups(Native::kCommandBufferGroup);
    native->swapchain.present(native->commands);

    const std::shared_ptr<Native::Signals> signals = native->signals;
    const std::uint64_t frame = native->frameNumber;
    @autoreleasepool {
        [native->commands addCompletedHandler:^(id<MTLCommandBuffer> commands) { Native::complete(*signals, commands, frame); }];
        [native->commands commit];
    }

    native->commands = nil;
    native->frameOpen = false;
    ++native->frameNumber;
    native->graveyard.setFrame(native->frameNumber);
}

void Device::resizeSurface(int width, int height) {
    native->swapchain.resize(width, height);
}

void Device::detachSurface() {
    native->swapchain.detach();
}

void Device::attachSurface(const Surface& surface) {
    native->swapchain.attach(surface);
}

std::unique_ptr<gpu::Device::Readback> Device::readImage(Image image, int width, int height) {
    const Native::ImageEntry& entry = native->requireImage(image);
    if (entry.kind != ImageKind::RenderTarget) {
        throw std::logic_error("Only a render target can be read.");
    }
    if (entry.sampleCount > 1) {
        throw std::logic_error("A multisampled render target is read through the image it resolves into.");
    }
    if (width <= 0 || height <= 0 || width > entry.width || height > entry.height) {
        throw std::logic_error(std::format("The render target of {}x{} pixels has no {}x{} pixels to read.", entry.width, entry.height, width, height));
    }
    return std::make_unique<ImageReadback>(native->queue, entry.texture, entry.format, width, height);
}

} // namespace haylen::gpu::metal
