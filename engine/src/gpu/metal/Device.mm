#include "gpu/metal/Device.hpp"

#import <Metal/Metal.h>
#import <QuartzCore/CABase.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gpu/BufferRules.hpp"
#include "gpu/HandleTable.hpp"
#include "gpu/ImageInfo.hpp"
#include "gpu/ImageRules.hpp"
#include "gpu/ProgramSlots.hpp"
#include "gpu/SamplerRules.hpp"
#include "gpu/metal/BufferReadback.hpp"
#include "gpu/metal/EncoderState.hpp"
#include "gpu/metal/Formats.hpp"
#include "gpu/metal/Graveyard.hpp"
#include "gpu/metal/ImageReadback.hpp"
#include "gpu/metal/LibraryCache.hpp"
#include "gpu/metal/Swapchain.hpp"
#include "gpu/metal/UploadRing.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/gpu/ContractError.hpp"

namespace haylen::gpu::metal {

// The Metal objects of the device and the state of the frame and the pass that record.
struct Device::Native {
    static constexpr std::string_view kMissingCaptureTool = "Metal writes GPU traces only for an app that starts with the environment variable \"MTL_CAPTURE_ENABLED\" set to 1 or the key \"MetalCaptureEnabled\" set to true in its \"Info.plist\". The Debug runs of \"haylen app run\" start the app with the variable, so run the app with it, or set one of them.";

    // Uniform blocks take the buffer indices from 0 of each stage, so vertex buffers take the ones after them.
    static constexpr NSUInteger kFirstVertexBuffer = kMaxBlocks;

    // Every region of a stream buffer starts at a multiple of this many bytes, which every Metal GPU accepts as the offset of a vertex buffer and `kUniformBufferAlignment` asks of a uniform buffer.
    static constexpr std::size_t kRegionAlignment = 256;

    // The bytes of a write into a dynamic buffer start in the upload ring at a multiple of this many bytes, which blits between buffers ask of their offsets on macOS.
    static constexpr std::size_t kCopyAlignment = 4;

    static constexpr std::size_t kUploadRingSize = 256 * 1024;

    // The entries of the texture argument table of the Apple2 and Apple3 families.
    static constexpr int kLegacyTextureBindings = 31;

    // The id of a debug group pushed into the command buffer, and of one pushed while a pass that records nothing was open.
    static constexpr std::uint32_t kCommandBufferGroup = 0;
    static constexpr std::uint32_t kClosedGroup = UINT32_MAX;

    struct BufferEntry {
        id<MTLBuffer> buffer = nil;
        std::byte* contents = nullptr;
        std::size_t region = 0;
        BufferRules::Description description{};
    };

    // An image or a view, whose texture is a texture view of Metal. An image keeps the ids of its views, which go with it.
    struct ImageEntry {
        id<MTLTexture> texture = nil;
        ImageRules::Description description{};
        std::vector<std::uint64_t> views;
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
        std::atomic<double> inputAge = 0.0;
        std::atomic<bool> lost = false;
        std::mutex mutex;
        std::vector<std::string> errors;
    };

    // The number of the first frame, since the count of the frames that the GPU finished is 0 before any.
    static constexpr std::uint64_t kFirstFrame = 1;

    Native(id<MTLDevice> metal, int swapchainSamples, bool debug);

    [[nodiscard]] static Capabilities describe(id<MTLDevice> metal);
    [[nodiscard]] static bool hasTileMemory(id<MTLDevice> metal);
    [[nodiscard]] static NSString* toText(std::string_view text);
    [[nodiscard]] static std::string describeError(id<MTLCommandBuffer> commands);
    [[nodiscard]] static bool isLostError(NSInteger code) noexcept;
    static void complete(Signals& state, id<MTLCommandBuffer> commands, std::uint64_t frame);

    [[nodiscard]] BufferEntry& requireBuffer(Buffer buffer);
    [[nodiscard]] Swapchain& requireSwapchain(int surface);

    // The number of the last committed frame of a slot, or 0 for a slot that no frame used yet.
    [[nodiscard]] std::uint64_t getLastFrame(std::size_t slot) const noexcept;
    [[nodiscard]] std::unique_ptr<Swapchain>& requireAddedSurface(int surface);
    [[nodiscard]] ImageEntry& requireImage(Image image);
    [[nodiscard]] ProgramEntry& requireProgram(Program program);
    [[nodiscard]] id<MTLBlitCommandEncoder> getTransferBlit();
    [[nodiscard]] id<MTLDepthStencilState> getDepthStencilState(const DepthStencil& desc, bool depth, bool stencil);
    [[nodiscard]] NSUInteger getRegionOffset(const BufferEntry& entry) const noexcept;
    void upload(id<MTLBuffer> target, std::span<const std::byte> data);
    [[nodiscard]] std::uint32_t bindUniformBuffers(const ProgramEntry& program, const Bindings& bound);
    static void checkPipeline(const PipelineDesc& desc);
    [[nodiscard]] MTLTextureDescriptor* describeImage(const ImageDesc& desc) const;
    void releaseLibraries(const ProgramEntry& entry);

    // Holds the place in the graveyard of the object that the device makes next, and of every object that lives, so a destroy never allocates.
    void reserveRetirement() {
        graveyard.reserve(static_cast<std::size_t>(buffers.size() + images.size() + samplers.size() + pipelines.size()) + 1);
    }
    static void copyLevel(id<MTLBlitCommandEncoder> blit, id<MTLBuffer> source, NSUInteger offset, const ImageEntry& image, int level);

    id<MTLDevice> device = nil;
    id<MTLCommandQueue> queue = nil;
    Capabilities capabilities;
    bool labels = false;
    bool unifiedMemory = false;
    std::size_t uniformAlignment = 256;

    // The texture indices that a stage binds, all the slots of the contract except on the A8 to A10X of the Apple TVs before the second Apple TV 4K, which bind 31. The simulator binds them on the GPU of the Mac.
    int textureBindings = static_cast<int>(kMaxTextures);
    std::shared_ptr<Signals> signals = std::make_shared<Signals>();
    Graveyard graveyard{kFirstFrame};
    Swapchain swapchain;

    // The swapchains of the surfaces beyond the window, at their numbers less one, with an empty place for a surface that went away.
    std::vector<std::unique_ptr<Swapchain>> surfaces;
    LibraryCache libraries;
    std::vector<UploadRing> rings;
    // The slots whose ring a trim makes smaller once the frame in flight that reads it finished.
    std::array<bool, kFramesInFlight> trims{};

    HandleTable<BufferEntry> buffers{"buffer"};
    HandleTable<ImageEntry> images{"image"};
    HandleTable<id<MTLSamplerState>> samplers{"sampler"};
    HandleTable<ProgramEntry> programs{"program"};
    HandleTable<PipelineEntry> pipelines{"pipeline"};
    std::unordered_map<std::uint64_t, id<MTLDepthStencilState>> depthStencilStates;

    MTLCommandBufferDescriptor* commandsDesc = nil;
    MTLRenderPassDescriptor* passDesc = nil;

    // The frame that records, or the next one between frames.
    std::uint64_t frameNumber = kFirstFrame;
    std::size_t frameSlot = 0;
    bool frameOpen = false;

    // Whether the wait for the display claimed the slot of the next frame, whether a frame presented since the last wait, and the host time in seconds when the last wait returned, when the next frame reads its input.
    bool slotClaimed = false;
    bool presentedSinceWait = false;
    CFTimeInterval inputTime = 0.0;
    int passes = 0;
    bool imagesWritten = false;
    bool mipmapsGenerated = false;
    id<MTLCommandBuffer> commands = nil;
    id<MTLCommandBuffer> transfer = nil;
    id<MTLBlitCommandEncoder> transferBlit = nil;
    std::vector<UploadRing::Allocation> staged;

    // The pass that records. A swapchain pass that ended keeps its encoder open while nothing else records, so a swapchain pass that keeps what it drew continues in it.
    EncoderState encoder;
    bool passOpen = false;
    bool swapchainPass = false;
    bool swapchainPending = false;
    int pendingSurface = 0;
    std::uint32_t passSerial = 0;
    Applied applied;
    Bindings bindings;
    bool bindingsApplied = false;

    // The uniform blocks that `applyUniforms` fed since the pipeline was applied, and those that the uniform buffers of the bindings feed, as bits of their slots.
    std::uint32_t uniformBlocks = 0;
    std::uint32_t bufferBlocks = 0;

    id<MTLBuffer> indexBuffer = nil;
    NSUInteger indexOffset = 0;
    std::vector<std::uint32_t> debugGroups;

    // The file of the capture that records, which Metal writes once it stops.
    std::optional<std::filesystem::path> capturePath;
};

Device::Native::Native(id<MTLDevice> metal, int swapchainSamples, bool debug) : device(metal), capabilities(describe(metal)), labels(debug), unifiedMemory(metal.hasUnifiedMemory), swapchain(metal, graveyard, swapchainSamples, hasTileMemory(metal)), libraries(metal) {
#if !TARGET_OS_SIMULATOR
    if ([metal supportsFamily:MTLGPUFamilyApple1]) {
        uniformAlignment = 16;
    }

    if ([metal supportsFamily:MTLGPUFamilyApple1] && ![metal supportsFamily:MTLGPUFamilyApple4]) {
        textureBindings = kLegacyTextureBindings;
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

// Every Metal GPU samples and draws into every uncompressed format of the contract, with `Depth24Stencil8` as `Depth32FloatStencil8`, and blends into its color formats, except Apple GPUs before the A14 and M1, which blend into no format of 32 bit floats. A blit generates the mip levels of every color format that the GPU draws into and filters. Every Metal GPU makes texture views of ranges of levels and slices, also as 2D arrays of the faces of cubes, while cube arrays need the A11 and later or a Mac, which the families Apple4 and Mac2 hold. The sizes and the samples follow the feature set tables of Metal: volumes of 2048 pixels on a side, cubes as large as flat images, and the sample counts the GPU takes for every format, except that a format of 32 bit floats resolves only on a GPU that filters it. Apple GPUs sample the ETC2, EAC and ASTC formats, with the HDR profile of ASTC from the A13 and M1 on, and the GPUs that report it sample the BC formats, which every Mac and the iPads of the M1 on do. Shaders write the uncompressed color formats as storage images, and the sRGB one only on Apple GPUs. Shaders of every Metal GPU read and write buffers, the GPUs from the A9 on draw with indirect arguments, and a uniform block reads any range of a buffer, so it holds as many bytes as the largest buffer. A GPU outside the Apple families is the GPU of an Intel Mac, which takes images of 16384 pixels, draws with indirect arguments and blends, filters and resolves every format.
gpu::Device::Capabilities Device::Native::describe(id<MTLDevice> metal) {
    Capabilities made;
    const bool apple = [metal supportsFamily:MTLGPUFamilyApple1];
    made.maxImageSize = [metal supportsFamily:MTLGPUFamilyApple3] || !apple ? 16384 : 8192;
    made.maxArrayLayers = 2048;
    made.maxVolumeSize = 2048;
    made.maxCubeSize = made.maxImageSize;
    made.cubeArrays = [metal supportsFamily:MTLGPUFamilyApple4] || !apple;
    made.depthLayers = true;
    made.imageViews = true;

    std::uint8_t counts = 1;
    for (const int samples : {2, 4, 8}) {
        if ([metal supportsTextureSampleCount:static_cast<NSUInteger>(samples)]) {
            counts |= static_cast<std::uint8_t>(samples);
        }
    }

    made.originTopLeft = true;
    made.independentBlend = true;
    made.gpuTiming = true;
    made.maxAnisotropy = 16;
    made.borderColors = [metal supportsFamily:MTLGPUFamilyApple7] || !apple;
    made.storageBuffers = true;
    made.indirectBuffers = [metal supportsFamily:MTLGPUFamilyApple3] || !apple;
    made.maxUniformBlockSize = static_cast<std::size_t>(metal.maxBufferLength);

    bool bc = false;
    if (@available(macOS 11.0, iOS 16.4, tvOS 16.4, *)) {
        bc = metal.supportsBCTextureCompression;
    }

    const bool astcHdr = [metal supportsFamily:MTLGPUFamilyApple6];
    const bool floats = [metal supportsFamily:MTLGPUFamilyApple7] || !apple;
    const bool floatFiltering = metal.supports32BitFloatFiltering;
    for (std::size_t index = 0; index < made.formats.size(); ++index) {
        const auto format = static_cast<PixelFormat>(index);
        FormatSupport& support = made.formats[index];
        if (ImageInfo::isCompressed(format)) {
            const ImageInfo::Compression compression = ImageInfo::getCompression(format);
            support.sample = compression == ImageInfo::Compression::Bc ? bc : apple && (compression != ImageInfo::Compression::AstcHdr || astcHdr);
            support.filter = support.sample;
            continue;
        }

        const bool wide = format == PixelFormat::R32Float || format == PixelFormat::Rg32Float || format == PixelFormat::Rgba32Float;
        const bool depth = ImageInfo::isDepth(format);
        support.sample = true;
        support.render = true;
        support.blend = !depth && (!wide || floats);
        support.filter = !depth && (!wide || floatFiltering);
        support.storage = !depth && (format != PixelFormat::Rgba8Srgb || apple);
        support.mipmaps = support.filter;
        support.sampleCounts = !wide || floatFiltering || !apple ? counts : std::uint8_t{1};
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

// Only an Intel Mac can lose its GPU itself, such as an external GPU that was unplugged, so only builds that run on macOS before 27 check it, while every system can revoke the access of the app to the GPU or fail inside its driver.
bool Device::Native::isLostError(NSInteger code) noexcept {
#if TARGET_OS_OSX && __MAC_OS_X_VERSION_MIN_REQUIRED < 270000
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

std::uint64_t Device::Native::getLastFrame(std::size_t slot) const noexcept {
    for (std::uint64_t frame = frameNumber; frame-- > kFirstFrame;) {
        if (static_cast<std::size_t>(frame % kFramesInFlight) == slot) {
            return frame;
        }
    }

    return 0;
}

// The surface 0 is the window, and the others are the surfaces that `addSurface` made.
Swapchain& Device::Native::requireSwapchain(int surface) {
    if (surface == 0) {
        return swapchain;
    }

    return *requireAddedSurface(surface);
}

std::unique_ptr<Swapchain>& Device::Native::requireAddedSurface(int surface) {
    if (surface < 1 || static_cast<std::size_t>(surface) > surfaces.size() || surfaces[static_cast<std::size_t>(surface - 1)] == nullptr) {
        throw ContractError(ContractError::Rule::Surface);
    }

    return surfaces[static_cast<std::size_t>(surface - 1)];
}

Device::Native::BufferEntry& Device::Native::requireBuffer(Buffer buffer) {
    BufferEntry* entry = buffers.find(buffer.id);
    if (entry == nullptr) {
        throw ContractError(ContractError::Rule::MissingObject);
    }

    return *entry;
}

Device::Native::ImageEntry& Device::Native::requireImage(Image image) {
    ImageEntry* entry = images.find(image.id);
    if (entry == nullptr) {
        throw ContractError(ContractError::Rule::MissingObject);
    }

    return *entry;
}

Device::Native::ProgramEntry& Device::Native::requireProgram(Program program) {
    ProgramEntry* entry = programs.find(program.id);
    if (entry == nullptr) {
        throw ContractError(ContractError::Rule::MissingObject);
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
    return entry.description.update == BufferUpdate::Stream ? frameSlot * entry.region : 0;
}

// The bytes go into a staging buffer of their own, which the transfer command buffer retains until it copied them before the frame.
void Device::Native::upload(id<MTLBuffer> target, std::span<const std::byte> data) {
    id<MTLBuffer> staging = [device newBufferWithBytes:data.data() length:data.size() options:MTLResourceStorageModeShared];
    if (staging == nil) {
        throw std::runtime_error(std::format("The GPU has no memory to stage the {} bytes of a buffer.", data.size()));
    }

    [getTransferBlit() copyFromBuffer:staging sourceOffset:0 toBuffer:target destinationOffset:0 size:data.size()];
}

// A uniform block reads the uniform buffer that the bindings name at its slot, from the region of the frame for a stream buffer, unless `applyUniforms` fed the slot since the pipeline was applied.
std::uint32_t Device::Native::bindUniformBuffers(const ProgramEntry& program, const Bindings& bound) {
    std::uint32_t fed = 0;
    for (std::size_t slot = 0; slot < kMaxBlocks; ++slot) {
        const BlockSlot& block = program.blocks[slot];
        const BufferRange& range = bound.uniformBuffers[slot];
        if (!block.used || !range.buffer.isValid()) {
            continue;
        }

        const std::uint32_t bit = 1U << slot;
        if ((uniformBlocks & bit) != 0) {
            throw ContractError(ContractError::Rule::Uniforms);
        }

        const BufferEntry& buffer = requireBuffer(range.buffer);
        BufferRules::checkUniform(buffer.description, range.offset, block.size);
        encoder.setBuffer(block.stage, buffer.buffer, getRegionOffset(buffer) + range.offset, block.binding);
        fed |= bit;
    }

    return fed;
}

void Device::Native::checkPipeline(const PipelineDesc& desc) {
    if (desc.colors.size() > kMaxColorTargets || (desc.colors.empty() && !desc.swapchain && !desc.depthStencil.format)) {
        throw ContractError(ContractError::Rule::Pipeline);
    }

    if (desc.buffers.size() > kMaxVertexBuffers) {
        throw ContractError(ContractError::Rule::Pipeline);
    }

    for (const VertexAttribute& attribute : desc.attributes) {
        if (attribute.buffer < 0 || attribute.buffer >= static_cast<int>(desc.buffers.size()) || attribute.location < 0 || attribute.location >= static_cast<int>(kMaxAttributes)) {
            throw ContractError(ContractError::Rule::Pipeline);
        }
    }

    if (!desc.swapchain && desc.depthStencil.format && !ImageInfo::isDepth(*desc.depthStencil.format)) {
        throw ContractError(ContractError::Rule::Pipeline);
    }
}

MTLTextureDescriptor* Device::Native::describeImage(const ImageDesc& desc) const {
    MTLTextureDescriptor* made = [MTLTextureDescriptor new];
    made.textureType = Formats::toTextureType(desc.type, desc.sampleCount > 1);
    switch (desc.type) {
    case TextureType::Flat:
    case TextureType::Cube:
        break;
    case TextureType::Array:
        made.arrayLength = static_cast<NSUInteger>(desc.layers);
        break;
    case TextureType::Volume:
        made.depth = static_cast<NSUInteger>(desc.layers);
        break;
    case TextureType::CubeArray:
        made.arrayLength = static_cast<NSUInteger>(desc.layers / 6);
        break;
    }

    made.pixelFormat = Formats::toPixelFormat(desc.format);
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

// The pixels of a level sit in rows of blocks, layer after layer from the offset, and a volume copies all its slices at once. The slices of a cube array in Metal are its layers, six faces for each cube.
void Device::Native::copyLevel(id<MTLBlitCommandEncoder> blit, id<MTLBuffer> source, NSUInteger offset, const ImageEntry& image, int level) {
    const ImageRules::Description& described = image.description;
    const ImageInfo::Extent extent = described.getLevelExtent(level);
    const auto width = static_cast<NSUInteger>(extent.width);
    const auto height = static_cast<NSUInteger>(extent.height);
    const NSUInteger rowBytes = ImageInfo::getRowBytes(described.format, extent.width);
    const NSUInteger layerBytes = ImageInfo::getRegionBytes(described.format, extent.width, extent.height);
    if (described.type == TextureType::Volume) {
        [blit copyFromBuffer:source sourceOffset:offset sourceBytesPerRow:rowBytes sourceBytesPerImage:layerBytes sourceSize:MTLSizeMake(width, height, static_cast<NSUInteger>(extent.depth)) toTexture:image.texture destinationSlice:0 destinationLevel:static_cast<NSUInteger>(level) destinationOrigin:MTLOriginMake(0, 0, 0)];
        return;
    }

    for (int layer = 0; layer < described.layers; ++layer) {
        [blit copyFromBuffer:source sourceOffset:offset + static_cast<NSUInteger>(layer) * layerBytes sourceBytesPerRow:rowBytes sourceBytesPerImage:layerBytes sourceSize:MTLSizeMake(width, height, 1) toTexture:image.texture destinationSlice:static_cast<NSUInteger>(layer) destinationLevel:static_cast<NSUInteger>(level) destinationOrigin:MTLOriginMake(0, 0, 0)];
    }
}

std::unique_ptr<gpu::Device> Device::create(const std::optional<Surface>& surface, const DeviceOptions& options) {
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

    const int taken = native->frameOpen || native->slotClaimed ? 1 : 0;
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

std::optional<double> Device::getInputAge() const noexcept {
    const double seconds = native->signals->inputAge.load(std::memory_order_relaxed);
    return seconds > 0.0 ? std::optional<double>(seconds) : std::nullopt;
}

void Device::readPools(std::vector<Pool>& pools) const {
    pools.assign({
        {.name = "images", .used = native->images.size(), .size = native->images.capacity()},
        {.name = "buffers", .used = native->buffers.size(), .size = native->buffers.capacity()},
        {.name = "samplers", .used = native->samplers.size(), .size = native->samplers.capacity()},
        {.name = "programs", .used = native->programs.size(), .size = native->programs.capacity()},
        {.name = "pipelines", .used = native->pipelines.size(), .size = native->pipelines.capacity()},
    });
}

bool Device::isLost() const noexcept {
    return native->signals->lost.load(std::memory_order_relaxed);
}

// Stream buffers hold a region for each frame in flight in shared memory, which the CPU writes while the GPU reads the regions of the frames before. Dynamic buffers live in the memory of the GPU, where the blits of the frames write them. Immutable buffers live in shared memory on GPUs that share the memory of the CPU, and in the memory of the GPU elsewhere. A Metal buffer serves every usage, so the usages only decide what the contract lets the buffer do.
gpu::Device::Buffer Device::makeBuffer(const BufferDesc& desc) {
    const BufferRules::Description description = BufferRules::check(desc, native->capabilities);
    const std::size_t size = description.size;

    Native::BufferEntry entry{.description = description};
    @autoreleasepool {
        if (description.update == BufferUpdate::Stream) {
            entry.region = (size + Native::kRegionAlignment - 1U) & ~(Native::kRegionAlignment - 1U);
            entry.buffer = [native->device newBufferWithLength:entry.region * kFramesInFlight options:MTLResourceStorageModeShared];
            entry.contents = entry.buffer == nil ? nullptr : static_cast<std::byte*>(entry.buffer.contents);
            for (std::size_t frame = 0; entry.contents != nullptr && !desc.data.empty() && frame < kFramesInFlight; ++frame) {
                std::memcpy(entry.contents + frame * entry.region, desc.data.data(), size);
            }
        } else if (description.update == BufferUpdate::Immutable && native->unifiedMemory) {
            entry.buffer = [native->device newBufferWithBytes:desc.data.data() length:size options:MTLResourceStorageModeShared];
        } else {
            entry.buffer = [native->device newBufferWithLength:size options:MTLResourceStorageModePrivate];
            if (entry.buffer != nil && !desc.data.empty()) {
                native->upload(entry.buffer, desc.data);
            }
        }
    }

    if (entry.buffer == nil) {
        throw std::runtime_error(std::format("The GPU has no memory for the buffer \"{}\" of {} bytes.", desc.label, size));
    }

    if (native->labels && !desc.label.empty()) {
        entry.buffer.label = Native::toText(desc.label);
    }

    native->reserveRetirement();
    return {native->buffers.add(std::move(entry))};
}

gpu::Device::Image Device::makeImage(const ImageDesc& desc) {
    Native::ImageEntry entry{.description = ImageRules::check(desc, native->capabilities)};
    @autoreleasepool {
        entry.texture = [native->device newTextureWithDescriptor:native->describeImage(desc)];
    }

    if (entry.texture == nil) {
        throw std::runtime_error(std::format("The GPU has no memory for the image \"{}\" of {}x{} pixels.", desc.label, desc.width, desc.height));
    }

    if (native->labels && !desc.label.empty()) {
        entry.texture.label = Native::toText(desc.label);
    }

    native->reserveRetirement();
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

// The pixels go into a staging buffer of their own, which the transfer command buffer retains until it copied them. The rules of the image checked the size of every level.
void Device::uploadLevels(Image image, std::span<const std::span<const std::byte>> levels) {
    const Native::ImageEntry& entry = native->requireImage(image);
    std::vector<std::size_t> offsets(levels.size());
    std::size_t total = 0;
    for (std::size_t level = 0; level < levels.size(); ++level) {
        offsets[level] = total;
        total += (levels[level].size() + 15U) & ~std::size_t{15U};
    }

    @autoreleasepool {
        id<MTLBuffer> staging = [native->device newBufferWithLength:total options:MTLResourceStorageModeShared];
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

// A view is a texture view of Metal of the same pixel format, which changes the type of a cube or a cube array to a 2D array of its faces without a usage of its own. Metal counts the faces of cubes as slices, as the contract counts layers, and a volume as one slice.
gpu::Device::Image Device::makeView(const ViewDesc& desc) {
    Native::ImageEntry& image = native->requireImage(desc.image);
    Native::ImageEntry entry{.description = ImageRules::checkView(image.description, desc, native->capabilities)};
    const NSRange levels = NSMakeRange(static_cast<NSUInteger>(desc.baseMipLevel), static_cast<NSUInteger>(desc.mipLevels));
    const NSRange slices = desc.type == TextureType::Volume ? NSMakeRange(0, 1) : NSMakeRange(static_cast<NSUInteger>(desc.baseLayer), static_cast<NSUInteger>(desc.layers));
    @autoreleasepool {
        entry.texture = [image.texture newTextureViewWithPixelFormat:image.texture.pixelFormat textureType:Formats::toTextureType(desc.type, false) levels:levels slices:slices];
    }

    if (entry.texture == nil) {
        throw std::runtime_error(std::format("Metal could not make the view \"{}\" of an image.", desc.label));
    }

    if (native->labels && !desc.label.empty()) {
        entry.texture.label = Native::toText(desc.label);
    }

    // The records of the table never move, so the image keeps its place while the view takes one, and its list holds room for the view first.
    image.views.reserve(image.views.size() + 1);
    native->reserveRetirement();
    const std::uint64_t view = native->images.add(std::move(entry));
    image.views.push_back(view);
    return {view};
}

gpu::Device::Sampler Device::makeSampler(const SamplerDesc& desc) {
    SamplerRules::check(desc, native->capabilities);
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
        made.maxAnisotropy = static_cast<NSUInteger>(desc.maxAnisotropy);
        made.borderColor = Formats::toBorderColor(desc.borderColor);
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

    native->reserveRetirement();
    return {native->samplers.add(std::move(sampler))};
}

// The stages come from the library cache, where a program that `prepareProgram` announced finds its libraries made or in the making on the I/O pool.
gpu::Device::Program Device::makeProgram(const ProgramDesc& desc) {
    ProgramSlots::check(desc);

    if (desc.vertex.source.empty() || desc.fragment.source.empty()) {
        throw ContractError(ContractError::Rule::Program);
    }

    Native::ProgramEntry entry;
    for (const BlockBinding& block : desc.blocks) {
        if (block.binding >= static_cast<int>(Native::kFirstVertexBuffer) || block.size > native->capabilities.maxUniformBlockSize) {
            throw ContractError(ContractError::Rule::Program);
        }

        entry.blocks[static_cast<std::size_t>(block.slot)] = {.used = true, .stage = block.stage, .size = block.size, .binding = static_cast<NSUInteger>(block.binding)};
    }

    for (const TextureBinding& texture : desc.textures) {
        if (texture.binding >= static_cast<int>(EncoderState::kTextureSlots)) {
            throw ContractError(ContractError::Rule::Program);
        }

        if (texture.binding >= native->textureBindings) {
            throw std::runtime_error(std::format("The program \"{}\" binds a texture at index {}, and the GPU \"{}\" binds textures up to index {}.", desc.label, texture.binding, native->device.name.UTF8String, native->textureBindings - 1));
        }

        if (texture.type == TextureType::CubeArray && !native->capabilities.cubeArrays) {
            throw ContractError(ContractError::Rule::Program);
        }

        entry.textures.push_back({.slot = static_cast<std::size_t>(texture.slot), .stage = texture.stage, .binding = static_cast<NSUInteger>(texture.binding)});
    }

    for (const SamplerBinding& sampler : desc.samplers) {
        if (sampler.binding >= static_cast<int>(EncoderState::kSamplerSlots)) {
            throw ContractError(ContractError::Rule::Program);
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
    native->checkPipeline(desc);

    const bool depth = desc.swapchain || desc.depthStencil.format.has_value();
    const bool stencil = desc.swapchain || (desc.depthStencil.format && ImageInfo::hasStencil(*desc.depthStencil.format));
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
            target.pixelFormat = desc.swapchain ? Swapchain::kColorFormat : Formats::toPixelFormat(color.format);
            target.writeMask = Formats::toColorWriteMask(color.writeMask);
            target.blendingEnabled = color.blend.enabled;
            target.sourceRGBBlendFactor = Formats::toBlendFactor(color.blend.sourceColor);
            target.destinationRGBBlendFactor = Formats::toBlendFactor(color.blend.destinationColor);
            target.rgbBlendOperation = Formats::toBlendOperation(color.blend.colorOperation);
            target.sourceAlphaBlendFactor = Formats::toBlendFactor(color.blend.sourceAlpha);
            target.destinationAlphaBlendFactor = Formats::toBlendFactor(color.blend.destinationAlpha);
            target.alphaBlendOperation = Formats::toBlendOperation(color.blend.alphaOperation);
        }

        const MTLPixelFormat depthFormat = desc.swapchain ? Swapchain::kDepthFormat : (depth ? Formats::toPixelFormat(*desc.depthStencil.format) : MTLPixelFormatInvalid);
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
    native->reserveRetirement();
    return {native->pipelines.add(std::move(entry))};
}

void Device::destroy(Buffer buffer) noexcept {
    if (std::optional<Native::BufferEntry> entry = native->buffers.remove(buffer.id)) {
        native->graveyard.retire(entry->buffer);
    }
}

// An image takes its views with it, and a view leaves the list of its image.
void Device::destroy(Image image) noexcept {
    std::optional<Native::ImageEntry> entry = native->images.remove(image.id);
    if (!entry) {
        return;
    }

    native->graveyard.retire(entry->texture);
    if (entry->description.isView()) {
        Native::ImageEntry* viewed = native->images.find(entry->description.viewed.id);
        if (viewed != nullptr) {
            std::erase(viewed->views, image.id);
        }

        return;
    }

    for (const std::uint64_t view : entry->views) {
        if (std::optional<Native::ImageEntry> removed = native->images.remove(view)) {
            native->graveyard.retire(removed->texture);
        }
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

// A stream buffer takes the bytes in the region of the frame at once. A dynamic buffer takes them through the upload ring of the frame and a blit between the passes around the write.
void Device::writeBuffer(Buffer buffer, std::size_t offset, std::span<const std::byte> data) {
    if (native->passOpen) {
        throw ContractError(ContractError::Rule::BufferWrite);
    }

    const Native::BufferEntry& entry = native->requireBuffer(buffer);
    BufferRules::checkWrite(entry.description, offset, data.size());

    if (entry.description.update == BufferUpdate::Stream) {
        openFrame();
        std::memcpy(entry.contents + native->getRegionOffset(entry) + offset, data.data(), data.size());
        return;
    }

    prepareCopy();
    const UploadRing::Allocation staged = native->rings[native->frameSlot].write(data, Native::kCopyAlignment);
    @autoreleasepool {
        const id<MTLBlitCommandEncoder> blit = [native->commands blitCommandEncoder];
        [blit copyFromBuffer:staged.buffer sourceOffset:staged.offset toBuffer:entry.buffer destinationOffset:offset size:data.size()];
        [blit endEncoding];
    }
}

void Device::copyBuffer(Buffer source, std::size_t sourceOffset, Buffer destination, std::size_t destinationOffset, std::size_t size) {
    if (native->passOpen) {
        throw ContractError(ContractError::Rule::BufferWrite);
    }

    const Native::BufferEntry& from = native->requireBuffer(source);
    const Native::BufferEntry& to = native->requireBuffer(destination);
    BufferRules::checkCopy(source, from.description, sourceOffset, destination, to.description, destinationOffset, size);

    prepareCopy();
    @autoreleasepool {
        const id<MTLBlitCommandEncoder> blit = [native->commands blitCommandEncoder];
        [blit copyFromBuffer:from.buffer sourceOffset:sourceOffset toBuffer:to.buffer destinationOffset:destinationOffset size:size];
        [blit endEncoding];
    }
}

// Every image goes into the upload ring of the frame before the blit copies any of them, and the blit runs before the first pass and the first mip generation of the frame. A layer of a volume is a slice of Metal at the depth of its origin, and every other layer a slice of the texture.
void Device::writeImages(std::span<const ImageWrite> writes) {
    if (native->frameOpen && (native->passes > 0 || native->imagesWritten || native->mipmapsGenerated)) {
        throw ContractError(ContractError::Rule::ImageWrite);
    }

    openFrame();
    native->imagesWritten = true;

    for (const ImageWrite& write : writes) {
        ImageRules::checkWrite(native->requireImage(write.image).description, write);
    }

    ImageRules::checkDistinctWrites(writes);
    if (writes.empty()) {
        return;
    }

    native->staged.clear();
    UploadRing& ring = native->rings[native->frameSlot];
    for (const ImageWrite& write : writes) {
        native->staged.push_back(ring.write(write.pixels, 16));
    }

    @autoreleasepool {
        const id<MTLBlitCommandEncoder> blit = [native->commands blitCommandEncoder];
        for (std::size_t index = 0; index < writes.size(); ++index) {
            const ImageWrite& write = writes[index];
            const Native::ImageEntry& image = native->requireImage(write.image);
            const bool volume = image.description.type == TextureType::Volume;
            const Region& region = write.region;
            const NSUInteger rowBytes = ImageInfo::getRowBytes(image.description.format, region.width);
            const MTLSize size = MTLSizeMake(static_cast<NSUInteger>(region.width), static_cast<NSUInteger>(region.height), 1);
            const MTLOrigin origin = MTLOriginMake(static_cast<NSUInteger>(region.x), static_cast<NSUInteger>(region.y), volume ? static_cast<NSUInteger>(write.layer) : 0);
            const NSUInteger slice = volume ? 0 : static_cast<NSUInteger>(write.layer);
            [blit copyFromBuffer:native->staged[index].buffer sourceOffset:native->staged[index].offset sourceBytesPerRow:rowBytes sourceBytesPerImage:rowBytes * size.height sourceSize:size toTexture:image.texture destinationSlice:slice destinationLevel:static_cast<NSUInteger>(write.mipLevel) destinationOrigin:origin];
        }

        [blit endEncoding];
    }
}

// A blit fills every smaller level of every slice from the first level, after the passes and copies before it and before the passes after it.
void Device::generateMipmaps(Image image) {
    if (native->passOpen) {
        throw ContractError(ContractError::Rule::Mipmaps);
    }

    const Native::ImageEntry& entry = native->requireImage(image);
    ImageRules::checkMipmaps(entry.description, native->capabilities);

    prepareCopy();
    native->mipmapsGenerated = true;
    @autoreleasepool {
        const id<MTLBlitCommandEncoder> blit = [native->commands blitCommandEncoder];
        [blit generateMipmapsForTexture:entry.texture];
        [blit endEncoding];
    }
}

void Device::beginPass(const PassDesc& desc) {
    if (native->passOpen) {
        throw ContractError(ContractError::Rule::PassOrder);
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
    Swapchain& chain = native->requireSwapchain(desc.surface);
    native->swapchainPass = true;
    if (native->swapchainPending && native->pendingSurface == desc.surface && desc.load == Load::Keep) {
        native->swapchainPending = false;
        native->encoder.resetViewport();
        return;
    }

    endPendingPass();
    native->pendingSurface = desc.surface;

    if (!chain.acquire()) {
        return;
    }

    MTLRenderPassDescriptor* pass = native->passDesc;
    const id<MTLTexture> drawable = chain.getDrawable().texture;
    const id<MTLTexture> multisample = chain.getMultisampleTexture();
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
    color.resolveLevel = 0;
    color.resolveSlice = 0;
    color.resolveDepthPlane = 0;
    color.loadAction = colorLoad;
    color.storeAction = multisample != nil ? MTLStoreActionMultisampleResolve : MTLStoreActionStore;
    color.clearColor = Formats::toClearColor(desc.clear);

    const MTLLoadAction depthLoad = desc.load == Load::Keep ? MTLLoadActionClear : Formats::toLoadAction(desc.load);
    pass.depthAttachment.texture = chain.getDepthTexture();
    pass.depthAttachment.level = 0;
    pass.depthAttachment.slice = 0;
    pass.depthAttachment.loadAction = depthLoad;
    pass.depthAttachment.storeAction = MTLStoreActionDontCare;
    pass.depthAttachment.clearDepth = desc.clearDepth;
    pass.stencilAttachment.texture = chain.getDepthTexture();
    pass.stencilAttachment.level = 0;
    pass.stencilAttachment.slice = 0;
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
        throw ContractError(ContractError::Rule::PassAttachments);
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
        const ImageRules::Description& described = image.description;
        if (described.isView() || described.kind != ImageKind::RenderTarget) {
            throw ContractError(ContractError::Rule::PassAttachments);
        }

        const bool volume = described.type == TextureType::Volume;
        color.texture = image.texture;
        color.level = static_cast<NSUInteger>(attachment.mipLevel);
        color.slice = volume ? 0 : static_cast<NSUInteger>(attachment.layer);
        color.depthPlane = volume ? static_cast<NSUInteger>(attachment.layer) : 0;
        color.loadAction = Formats::toLoadAction(attachment.load);
        color.clearColor = Formats::toClearColor(attachment.clear);
        color.storeAction = attachment.store == Store::Store ? MTLStoreActionStore : MTLStoreActionDontCare;
        if (attachment.resolve.isValid()) {
            const Native::ImageEntry& resolve = native->requireImage(attachment.resolve);
            if (resolve.description.isView()) {
                throw ContractError(ContractError::Rule::PassAttachments);
            }

            color.resolveTexture = resolve.texture;
            color.resolveLevel = static_cast<NSUInteger>(attachment.resolveMipLevel);
            color.resolveSlice = static_cast<NSUInteger>(attachment.resolveLayer);
            color.resolveDepthPlane = 0;
            color.storeAction = attachment.store == Store::Store ? MTLStoreActionStoreAndMultisampleResolve : MTLStoreActionMultisampleResolve;
        }

        const ImageInfo::Extent extent = described.getLevelExtent(attachment.mipLevel);
        width = extent.width;
        height = extent.height;
    }

    pass.depthAttachment.texture = nil;
    pass.stencilAttachment.texture = nil;
    if (desc.depthStencil) {
        const DepthStencilAttachment& attachment = *desc.depthStencil;
        const Native::ImageEntry& image = native->requireImage(attachment.image);
        const ImageRules::Description& described = image.description;
        if (described.isView() || described.kind != ImageKind::DepthStencil) {
            throw ContractError(ContractError::Rule::PassAttachments);
        }

        const auto level = static_cast<NSUInteger>(attachment.mipLevel);
        const auto slice = static_cast<NSUInteger>(attachment.layer);
        pass.depthAttachment.texture = image.texture;
        pass.depthAttachment.level = level;
        pass.depthAttachment.slice = slice;
        pass.depthAttachment.loadAction = Formats::toLoadAction(attachment.depthLoad);
        pass.depthAttachment.storeAction = attachment.depthStore == Store::Store ? MTLStoreActionStore : MTLStoreActionDontCare;
        pass.depthAttachment.clearDepth = attachment.clearDepth;
        if (ImageInfo::hasStencil(described.format)) {
            pass.stencilAttachment.texture = image.texture;
            pass.stencilAttachment.level = level;
            pass.stencilAttachment.slice = slice;
            pass.stencilAttachment.loadAction = Formats::toLoadAction(attachment.stencilLoad);
            pass.stencilAttachment.storeAction = attachment.stencilStore == Store::Store ? MTLStoreActionStore : MTLStoreActionDontCare;
            pass.stencilAttachment.clearStencil = attachment.clearStencil;
        }

        if (desc.colors.empty()) {
            const ImageInfo::Extent extent = described.getLevelExtent(attachment.mipLevel);
            width = extent.width;
            height = extent.height;
        }
    }

    @autoreleasepool {
        native->encoder.begin([native->commands renderCommandEncoderWithDescriptor:pass], width, height);
    }

    if (native->labels && !desc.label.empty()) {
        native->encoder.get().label = Native::toText(desc.label);
    }
}

void Device::requirePass() const {
    if (!native->passOpen) {
        throw ContractError(ContractError::Rule::PassOrder);
    }
}

void Device::applyViewport(const math::Rect& pixels) {
    requirePass();

    if (native->encoder.get() != nil) {
        native->encoder.setViewport(pixels);
    }
}

void Device::applyScissor(const math::Rect& pixels) {
    requirePass();

    if (native->encoder.get() != nil) {
        native->encoder.setScissor(pixels);
    }
}

void Device::applyPipeline(Pipeline pipeline) {
    requirePass();
    const Native::PipelineEntry* entry = native->pipelines.find(pipeline.id);
    if (entry == nullptr) {
        throw ContractError(ContractError::Rule::MissingObject);
    }

    native->applied = {.program = entry->program, .primitive = entry->primitive, .indexType = entry->indexType, .indexSize = entry->indexSize, .buffers = entry->buffers, .valid = true};
    native->bindingsApplied = false;
    native->uniformBlocks = 0;
    native->bufferBlocks = 0;
    if (native->encoder.get() == nil) {
        return;
    }

    native->encoder.setPipeline(entry->state);
    native->encoder.setDepthStencil(entry->depthStencil);
    native->encoder.setRasterizer(entry->cull, entry->winding, entry->depthBias, entry->depthBiasSlope);
}

// Bindings equal to the ones the pass applied last with the same pipeline change nothing, and the others reach Metal only where they differ from what the encoder holds.
void Device::applyBindings(const Bindings& bindings) {
    requirePass();

    if (!native->applied.valid) {
        throw ContractError(ContractError::Rule::PipelineState);
    }

    if (native->encoder.get() == nil || (native->bindingsApplied && bindings == native->bindings)) {
        return;
    }

    for (std::size_t index = 0; index < native->applied.buffers; ++index) {
        const Native::BufferEntry& buffer = native->requireBuffer(bindings.vertexBuffers[index]);
        BufferRules::checkVertex(buffer.description);
        native->encoder.setBuffer(Stage::Vertex, buffer.buffer, native->getRegionOffset(buffer) + static_cast<NSUInteger>(bindings.vertexOffsets[index]), Native::kFirstVertexBuffer + index);
    }

    if (native->applied.indexSize != 0) {
        const Native::BufferEntry& buffer = native->requireBuffer(bindings.indexBuffer);
        BufferRules::checkIndex(buffer.description);
        native->indexBuffer = buffer.buffer;
        native->indexOffset = native->getRegionOffset(buffer) + static_cast<NSUInteger>(bindings.indexOffset);
    }

    const Native::ProgramEntry& program = native->requireProgram(native->applied.program);
    native->bufferBlocks = native->bindUniformBuffers(program, bindings);
    for (const Native::ResourceSlot& texture : program.textures) {
        native->encoder.setTexture(texture.stage, native->requireImage(bindings.textures[texture.slot]).texture, texture.binding);
    }

    for (const Native::ResourceSlot& sampler : program.samplers) {
        const id<MTLSamplerState>* state = native->samplers.find(bindings.samplers[sampler.slot].id);
        if (state == nullptr) {
            throw ContractError(ContractError::Rule::Bindings);
        }

        native->encoder.setSampler(sampler.stage, *state, sampler.binding);
    }

    native->bindings = bindings;
    native->bindingsApplied = true;
}

void Device::applyUniforms(int slot, std::span<const std::byte> data) {
    requirePass();

    if (!native->applied.valid) {
        throw ContractError(ContractError::Rule::PipelineState);
    }

    const Native::ProgramEntry& program = native->requireProgram(native->applied.program);
    if (slot < 0 || slot >= static_cast<int>(kMaxBlocks) || !program.blocks[static_cast<std::size_t>(slot)].used) {
        throw ContractError(ContractError::Rule::Uniforms);
    }

    const Native::BlockSlot& block = program.blocks[static_cast<std::size_t>(slot)];
    const std::uint32_t bit = 1U << static_cast<unsigned>(slot);
    if (data.size() != block.size || (native->bufferBlocks & bit) != 0) {
        throw ContractError(ContractError::Rule::Uniforms);
    }

    native->uniformBlocks |= bit;

    if (native->encoder.get() == nil) {
        return;
    }

    if (data.size() <= EncoderState::kMaxBytes) {
        native->encoder.setBytes(block.stage, data, block.binding);
        return;
    }

    const UploadRing::Allocation uploaded = native->rings[native->frameSlot].write(data, native->uniformAlignment);
    native->encoder.setBuffer(block.stage, uploaded.buffer, uploaded.offset, block.binding);
}

void Device::applyStencilReference(std::uint8_t value) {
    requirePass();

    if (native->encoder.get() != nil) {
        native->encoder.setStencilReference(value);
    }
}

void Device::applyBlendColor(math::Color value) {
    requirePass();

    if (native->encoder.get() != nil) {
        native->encoder.setBlendColor(value);
    }
}

void Device::draw(int first, int count, int instances) {
    requirePass();

    if (!native->applied.valid) {
        throw ContractError(ContractError::Rule::PipelineState);
    }

    if (first < 0 || count < 0 || instances < 0) {
        throw ContractError(ContractError::Rule::Draw);
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
    requirePass();
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

    if (!std::exchange(native->slotClaimed, false)) {
        dispatch_semaphore_wait(native->signals->semaphore, DISPATCH_TIME_FOREVER);
    }

    native->graveyard.release(native->signals->completed.load(std::memory_order_acquire));
    reportErrors();

    native->frameSlot = static_cast<std::size_t>(native->frameNumber % kFramesInFlight);
    if (std::exchange(native->trims[native->frameSlot], false)) {
        native->rings[native->frameSlot].trim();
    }

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
    native->mipmapsGenerated = false;
}

void Device::endPendingPass() {
    if (native->swapchainPending) {
        native->swapchainPending = false;
        native->encoder.end();
    }
}

// A copy between passes ends the swapchain encoder that waits for a pass that keeps its contents, and Metal orders the blit after the passes before it and before the passes after it.
void Device::prepareCopy() {
    openFrame();
    endPendingPass();
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
        throw ContractError(ContractError::Rule::PassOrder);
    }

    submitTransfers();

    if (!native->frameOpen) {
        return;
    }

    endPendingPass();
    closeDebugGroups(Native::kCommandBufferGroup);

    // The drawable of the window tells when the display showed it, a time of the same clock as the moment the frame read its input, and a drawable that the display never showed has no time. The drawables of the simulators tell nothing of it.
#if TARGET_OS_SIMULATOR
    native->inputTime = 0.0;
#else
    const CFTimeInterval input = std::exchange(native->inputTime, 0.0);
    const std::shared_ptr<Native::Signals> shown = native->signals;
    if (native->swapchain.isDrawn() && input > 0.0) {
        // clang-format off
        [native->swapchain.getDrawable() addPresentedHandler:^(id<MTLDrawable> drawable) {
            if (drawable.presentedTime > 0.0) {
                shown->inputAge.store(drawable.presentedTime - input, std::memory_order_relaxed);
            }
        }];
        // clang-format on
    }
#endif

    native->presentedSinceWait = native->swapchain.present(native->commands) || native->presentedSinceWait;

    for (const std::unique_ptr<Swapchain>& chain : native->surfaces) {
        if (chain != nullptr) {
            native->presentedSinceWait = chain->present(native->commands) || native->presentedSinceWait;
        }
    }

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

bool Device::paceWindow(int framesPerSecond, std::function<void()> due) {
    if (native->passOpen) {
        throw ContractError(ContractError::Rule::PassOrder);
    }

    if (!due) {
        return native->swapchain.pace(framesPerSecond, {});
    }

    Native* const state = native.get();
    // clang-format off
    return native->swapchain.pace(framesPerSecond, [state, offered = std::move(due)] {
        state->inputTime = CACurrentMediaTime();
        offered();
    });
    // clang-format on
}

// A frame whose drawables wait to be shown keeps the next frame from taking a drawable until the display took the frame before, so the drawables come here, before the input of the next frame, instead of in its first swapchain pass. A layer that gives no drawable in time leaves the swapchain pass to ask again.
void Device::waitForDisplay() {
    if (native->passOpen) {
        throw ContractError(ContractError::Rule::PassOrder);
    }

    // A frame that a readback or an upload between frames already opened holds its slot.
    if (!native->slotClaimed && !native->frameOpen) {
        dispatch_semaphore_wait(native->signals->semaphore, DISPATCH_TIME_FOREVER);
        native->slotClaimed = true;
    }

    if (std::exchange(native->presentedSinceWait, false)) {
        native->swapchain.prepare();
        for (const std::unique_ptr<Swapchain>& chain : native->surfaces) {
            if (chain != nullptr) {
                chain->prepare();
            }
        }
    }

    native->inputTime = CACurrentMediaTime();
}

// The objects that wait for frames go once the GPU finished those frames, and all of them once every committed frame finished between frames, since no command buffer records them then.
void Device::trim() {
    if (native->passOpen) {
        throw ContractError(ContractError::Rule::PassOrder);
    }

    const std::uint64_t completed = native->signals->completed.load(std::memory_order_acquire);
    const bool idle = !native->frameOpen && completed + 1 >= native->frameNumber;
    native->graveyard.release(idle ? native->frameNumber : completed);

    for (std::size_t slot = 0; slot < native->rings.size(); ++slot) {
        const bool reading = (native->frameOpen && slot == native->frameSlot) || native->getLastFrame(slot) > completed;
        if (reading) {
            native->trims[slot] = true;
            continue;
        }

        native->rings[slot].trim();
    }
}

std::size_t Device::getSpareBytes() const noexcept {
    std::size_t spare = 0;
    for (const UploadRing& ring : native->rings) {
        spare += ring.getSpareBytes();
    }

    return spare;
}

void Device::resizeSurface(const Surface::Size& size) {
    native->swapchain.resize(size);
}

void Device::detachSurface() {
    native->swapchain.detach();
}

void Device::attachSurface(const Surface& surface) {
    native->swapchain.attach(surface);
}

int Device::addSurface(const Surface& surface) {
    auto chain = std::make_unique<Swapchain>(native->device, native->graveyard, native->swapchain.getSampleCount(), native->swapchain.isMemoryless());
    chain->attach(surface);
    const auto free = std::ranges::find_if(native->surfaces, [](const std::unique_ptr<Swapchain>& slot) { return slot == nullptr; });
    if (free != native->surfaces.end()) {
        *free = std::move(chain);
        return static_cast<int>(free - native->surfaces.begin()) + 1;
    }

    native->surfaces.push_back(std::move(chain));
    return static_cast<int>(native->surfaces.size());
}

void Device::resizeSurface(int surface, const Surface::Size& size) {
    native->requireSwapchain(surface).resize(size);
}

void Device::removeSurface(int surface) {
    native->requireAddedSurface(surface).reset();
}

std::unique_ptr<gpu::Device::Readback> Device::readImage(Image image, int width, int height) {
    const Native::ImageEntry& entry = native->requireImage(image);
    const ImageRules::Description& described = entry.description;
    if (described.isView() || described.kind != ImageKind::RenderTarget) {
        throw ContractError(ContractError::Rule::Readback);
    }

    if (described.sampleCount > 1) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    if (width <= 0 || height <= 0 || width > described.width || height > described.height) {
        throw ContractError(ContractError::Rule::Readback);
    }

    return std::make_unique<ImageReadback>(native->queue, entry.texture, described.format, width, height);
}

// A stream buffer is never a copy source, so the buffer holds its bytes from its start.
std::unique_ptr<gpu::Device::Readback> Device::readBuffer(Buffer buffer, std::size_t offset, std::size_t size) {
    if (native->passOpen) {
        throw ContractError(ContractError::Rule::Readback);
    }

    const Native::BufferEntry& entry = native->requireBuffer(buffer);
    BufferRules::checkRead(entry.description, offset, size);
    return std::make_unique<BufferReadback>(native->queue, entry.buffer, offset, size);
}

gpu::Device::CaptureSupport Device::getCaptureSupport() const {
    if (![[MTLCaptureManager sharedCaptureManager] supportsDestination:MTLCaptureDestinationGPUTraceDocument]) {
        return {.tool = CaptureTool::None, .missing = std::string(Native::kMissingCaptureTool)};
    }

    return {.tool = CaptureTool::MetalFrameCapture, .missing = {}};
}

// The capture covers every command queue of the device, and its document must not exist yet.
void Device::beginCapture(const std::filesystem::path& path) {
    if (native->passOpen) {
        throw ContractError(ContractError::Rule::Capture);
    }

    if (native->capturePath) {
        throw ContractError(ContractError::Rule::Capture);
    }

    MTLCaptureManager* manager = [MTLCaptureManager sharedCaptureManager];
    if (![manager supportsDestination:MTLCaptureDestinationGPUTraceDocument]) {
        throw std::logic_error(std::string(Native::kMissingCaptureTool));
    }

    std::filesystem::path document = path;
    document += ".gputrace";
    MTLCaptureDescriptor* descriptor = [MTLCaptureDescriptor new];
    descriptor.captureObject = native->device;
    descriptor.destination = MTLCaptureDestinationGPUTraceDocument;
    descriptor.outputURL = [NSURL fileURLWithPath:Native::toText(document.string())];
    NSError* error = nil;
    if (![manager startCaptureWithDescriptor:descriptor error:&error]) {
        throw std::runtime_error(std::format("Metal did not start the capture: {}", error == nil ? "it gave no reason." : error.localizedDescription.UTF8String));
    }

    native->capturePath = std::move(document);
}

std::filesystem::path Device::endCapture() {
    if (!native->capturePath) {
        throw ContractError(ContractError::Rule::Capture);
    }

    [[MTLCaptureManager sharedCaptureManager] stopCapture];
    std::filesystem::path document = *std::exchange(native->capturePath, std::nullopt);
    std::error_code error;
    if (!std::filesystem::exists(document, error)) {
        throw std::runtime_error(std::format("Metal wrote no GPU trace at \"{}\".", document.string()));
    }

    return document;
}

} // namespace haylen::gpu::metal
