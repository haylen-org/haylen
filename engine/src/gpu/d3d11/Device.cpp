#include "gpu/d3d11/Device.hpp"

#include <d3d11sdklayers.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <initializer_list>
#include <stdexcept>
#include <utility>

#include "gpu/ImageInfo.hpp"
#include "gpu/ImageRules.hpp"
#include "gpu/PassRules.hpp"
#include "gpu/SamplerRules.hpp"
#include "gpu/d3d11/BufferReadback.hpp"
#include "gpu/d3d11/ImageReadback.hpp"
#include "gpu/d3d11/StagingCopy.hpp"
#include "gpu/d3d11/StateObjects.hpp"
#include "gpu/direct3d/Adapter.hpp"
#include "gpu/direct3d/Formats.hpp"
#include "gpu/direct3d/HResult.hpp"
#include "gpu/direct3d/MipLevels.hpp"
#include "gpu/direct3d/ShaderCompiler.hpp"
#include "gpu/direct3d/ShaderRegisters.hpp"
#include "gpu/direct3d/Swapchain.hpp"
#include "gpu/direct3d/WindowSamples.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/gpu/ContractError.hpp"

namespace haylen::gpu::d3d11 {

using direct3d::Formats;
using direct3d::HResult;
using Microsoft::WRL::ComPtr;

std::unique_ptr<gpu::Device> Device::create(const std::optional<Surface>& target, const DeviceOptions& options) {
    ComPtr<IDXGIFactory6> dxgiFactory = direct3d::Adapter::createFactory(options.validation);
    std::string reasons;
    for (ComPtr<IDXGIAdapter1>& card : direct3d::Adapter::choose(*dxgiFactory.Get(), options.software, options.lowPower)) {
        const std::string name = direct3d::Adapter::getName(*card.Get());
        try {
            return std::make_unique<Device>(dxgiFactory, std::move(card), target, options.validation);
        } catch (const std::runtime_error& error) {
            reasons += std::format(" On \"{}\": {}", name, error.what());
        }
    }

    throw std::runtime_error(std::format("Direct3D 11 runs on no GPU of this machine.{}", reasons.empty() ? " Windows lists no GPU." : reasons));
}

Device::Device(ComPtr<IDXGIFactory6> dxgiFactory, ComPtr<IDXGIAdapter1> card, const std::optional<Surface>& target, bool validation) : factory(std::move(dxgiFactory)), adapter(std::move(card)) {
    createDevice(validation);
    readCapabilities();
    direct3d::WindowSamples::check(target, direct3d::WindowSamples::getSwapchainCounts(capabilities), "Direct3D 11");
    compiler = std::make_shared<direct3d::ShaderCompiler>(validation);
    uniforms = std::make_unique<DynamicRing>(*device.Get(), kUniformRingSize, D3D11_BIND_CONSTANT_BUFFER, kUniformAlignment, "haylen-uniform-ring");

    // The ring of uploads only feeds copies, and it binds as a vertex buffer because Direct3D 11 maps dynamic buffers without waiting only when they bind as vertex, index or constant buffers.
    uploads = std::make_unique<DynamicRing>(*device.Get(), kUploadRingSize, D3D11_BIND_VERTEX_BUFFER, 16, "haylen-upload-ring");
    timer = std::make_unique<FrameTimer>(*device.Get());

    // A window that has a drawable takes its swapchain now, so a window that Direct3D 11 cannot draw into fails the device while the platform can still try another API.
    chains.push_back(std::make_unique<SurfaceChain>(SurfaceChain{.surface = target}));
    swapchainSampleCount = target ? std::max(1, target->sampleCount) : 1;
    SurfaceChain& window = *chains.front();
    if (target && target->size.width > 0 && target->size.height > 0) {
        window.swapchain = std::make_unique<direct3d::Swapchain>(*factory.Get(), *device.Get(), *target);
        makeSwapchainTargets(window);
    }
}

Device::~Device() {
    if (context != nullptr) {
        context->ClearState();
        context->Flush();
    }

    chains.clear();
}

// Validation asks for the debug layer, which only the Graphics Tools of Windows bring, so a machine without them runs the device unvalidated and says so.
void Device::createDevice(bool validation) {
    static constexpr std::array<D3D_FEATURE_LEVEL, 2> kLevels{D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    ComPtr<ID3D11Device> made;
    ComPtr<ID3D11DeviceContext> immediate;

    HRESULT result = E_FAIL;
    if (validation) {
        result = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags | D3D11_CREATE_DEVICE_DEBUG, kLevels.data(), static_cast<UINT>(kLevels.size()), D3D11_SDK_VERSION, made.GetAddressOf(), nullptr, immediate.GetAddressOf());
        if (FAILED(result)) {
            core::Log::warning("Direct3D 11 runs without its debug layer, because {}. Add the optional feature \"Graphics Tools\" of Windows to validate it.", HResult::describe(result));
        }
    }

    if (FAILED(result)) {
        result = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags, kLevels.data(), static_cast<UINT>(kLevels.size()), D3D11_SDK_VERSION, made.GetAddressOf(), nullptr, immediate.GetAddressOf());
    }

    HResult::check(result, "start Direct3D 11 at feature level 11_0");
    if (FAILED(made.As(&device)) || FAILED(immediate.As(&context))) {
        throw std::runtime_error("Windows lacks the runtime of Direct3D 11.4, which Windows 10 version 1703 and later have.");
    }

    D3D11_FEATURE_DATA_D3D11_OPTIONS options{};
    device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &options, sizeof(options));
    if (options.ConstantBufferOffsetting == FALSE || options.MapNoOverwriteOnDynamicConstantBuffer == FALSE || options.DiscardAPIsSeenByDriver == FALSE) {
        throw std::runtime_error("The driver of the GPU lacks the constant buffer offsets of Direct3D 11.1 and the maps of constant buffers that do not wait.");
    }

    // The CPU sleeps on the event of the fence while the GPU finishes a frame, and only drivers of the display driver model WDDM 2.0 and later make fences.
    if (FAILED(device->CreateFence(0, D3D11_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())))) {
        throw std::runtime_error("The driver of the GPU lacks the fences of Direct3D 11.4, which drivers of the display driver model WDDM 2.0 and later have.");
    }

    fenceEvent.Attach(CreateEventW(nullptr, FALSE, FALSE, nullptr));
    if (!fenceEvent.IsValid()) {
        throw std::runtime_error("The graphics device could not create the event that waits for the GPU.");
    }

    static_cast<void>(context.As(&annotation));

    if (validation && SUCCEEDED(device.As(&infoQueue))) {
        messages = std::make_unique<direct3d::DebugMessages>();
    }

    adapterName = direct3d::Adapter::getName(*adapter.Get());
}

void Device::readCapabilities() {
    capabilities.maxImageSize = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    capabilities.maxArrayLayers = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
    capabilities.maxVolumeSize = D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
    capabilities.maxCubeSize = D3D11_REQ_TEXTURECUBE_DIMENSION;
    capabilities.originTopLeft = true;
    capabilities.independentBlend = true;
    capabilities.gpuTiming = true;
    capabilities.maxAnisotropy = 16;
    capabilities.maxLodBias = 15.99F;
    capabilities.borderColors = true;

    // Feature level 10_1 brings cube arrays, and a view of a range of the mip levels and layers of an image is a shader resource view.
    capabilities.cubeArrays = true;
    capabilities.depthLayers = true;
    capabilities.imageViews = true;

    // Feature level 11_0 brings storage buffers and the arguments of indirect draws, and a constant buffer binds at most 4096 constants of 16 bytes to one block.
    capabilities.storageBuffers = true;
    capabilities.indirectBuffers = true;
    capabilities.maxUniformBlockSize = kMaxBlockSize;

    // Images sample through the view of their format, depth images draw through the view of their depth, the driver generates the mip levels of the color formats that it marks for it, and a format takes a sample count where the GPU draws into it with that many samples and, for a color, resolves it.
    for (std::size_t index = 0; index < capabilities.formats.size(); ++index) {
        const auto format = static_cast<PixelFormat>(index);
        const bool depth = ImageInfo::isDepth(format);
        // Direct3D has no ETC2, EAC or ASTC format, so those formats stay unsupported.
        if (Formats::getViewFormat(format) == DXGI_FORMAT_UNKNOWN) {
            continue;
        }

        UINT view = 0;
        UINT target = 0;
        static_cast<void>(device->CheckFormatSupport(Formats::getViewFormat(format), &view));
        static_cast<void>(device->CheckFormatSupport(depth ? Formats::getDepthFormat(format) : Formats::getViewFormat(format), &target));
        capabilities.formats[index].sample = (view & static_cast<UINT>(D3D11_FORMAT_SUPPORT_SHADER_SAMPLE | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE_COMPARISON)) != 0;
        capabilities.formats[index].render = (target & static_cast<UINT>(depth ? D3D11_FORMAT_SUPPORT_DEPTH_STENCIL : D3D11_FORMAT_SUPPORT_RENDER_TARGET)) != 0;
        capabilities.formats[index].blend = !depth && capabilities.formats[index].render && (target & static_cast<UINT>(D3D11_FORMAT_SUPPORT_BLENDABLE)) != 0;
        capabilities.formats[index].filter = !depth && (view & static_cast<UINT>(D3D11_FORMAT_SUPPORT_SHADER_SAMPLE)) != 0;
        capabilities.formats[index].storage = !depth && (view & static_cast<UINT>(D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW)) != 0;
        capabilities.formats[index].mipmaps = !depth && (view & static_cast<UINT>(D3D11_FORMAT_SUPPORT_MIP_AUTOGEN)) != 0;
        capabilities.formats[index].sampleCounts = capabilities.formats[index].render ? std::uint8_t{1} : std::uint8_t{0};

        const UINT multisampled = static_cast<UINT>(depth ? D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET : D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET | D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE);
        if (!capabilities.formats[index].render || (target & multisampled) != multisampled) {
            continue;
        }

        for (const UINT count : {2U, 4U, 8U}) {
            UINT levels = 0;
            if (SUCCEEDED(device->CheckMultisampleQualityLevels(depth ? Formats::getDepthFormat(format) : Formats::getViewFormat(format), count, &levels)) && levels > 0) {
                capabilities.formats[index].sampleCounts |= static_cast<std::uint8_t>(count);
            }
        }
    }
}

gpu::Device::Language Device::getLanguage() const noexcept {
    return Language::Hlsl5;
}

gpu::Device::Api Device::getApi() const noexcept {
    return Api::Direct3D11;
}

const gpu::Device::Capabilities& Device::getCapabilities() const noexcept {
    return capabilities;
}

std::string Device::getAdapterName() const {
    return adapterName;
}

std::optional<double> Device::getGpuFrameSeconds() const noexcept {
    return timer->getSeconds();
}

void Device::readPools(std::vector<Pool>& pools) const {
    const auto pool = [](std::string_view name, int used, int size) { return Pool{.name = name, .used = used, .size = size}; };
    pools.assign({pool("images", images.size(), images.capacity()), pool("buffers", buffers.size(), buffers.capacity()), pool("samplers", samplers.size(), samplers.capacity()), pool("programs", programs.size(), programs.capacity()), pool("pipelines", pipelines.size(), pipelines.capacity())});
}

bool Device::isLost() const noexcept {
    return lost;
}

void Device::loseDevice(HRESULT reason) {
    if (lost) {
        return;
    }

    lost = true;
    const HRESULT removed = device->GetDeviceRemovedReason();
    core::Log::error("Direct3D 11 lost its device, because {}.", HResult::describe(FAILED(removed) ? removed : reason));
}

void Device::setName(ID3D11DeviceChild& object, std::string_view label) noexcept {
    if (!label.empty()) {
        object.SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(label.size()), label.data());
    }
}

// A uniform buffer holds whole windows of 16 constants, so the window that a block binds at any offset that the buffer takes stays inside it.
UINT Device::getByteWidth(const BufferRules::Description& description) noexcept {
    if (!description.usage.uniform) {
        return static_cast<UINT>(description.size);
    }

    return static_cast<UINT>((description.size + kUniformBufferAlignment - 1) / kUniformBufferAlignment * kUniformBufferAlignment);
}

// The CPU maps a stream buffer, which hands the GPU a fresh copy every frame, and an immutable buffer keeps its data, while the other buffers, the destinations of copies, storage and the buffers that only serve as the source of copies, which Direct3D 11 neither maps nor makes immutable without a bind flag, keep their bytes in GPU memory and take new ones through copies.
gpu::Device::Buffer Device::makeBuffer(const BufferDesc& desc) {
    const BufferRules::Description description = BufferRules::check(desc, capabilities);
    const BufferUsage& usage = description.usage;
    const bool mapped = description.update == BufferUpdate::Stream && !usage.storage;

    // Storage reads and writes its buffer through raw views, which take any buffer without a stride.
    D3D11_BUFFER_DESC made{};
    made.ByteWidth = getByteWidth(description);
    made.BindFlags |= usage.vertex ? static_cast<UINT>(D3D11_BIND_VERTEX_BUFFER) : 0U;
    made.BindFlags |= usage.index ? static_cast<UINT>(D3D11_BIND_INDEX_BUFFER) : 0U;
    made.BindFlags |= usage.uniform ? static_cast<UINT>(D3D11_BIND_CONSTANT_BUFFER) : 0U;
    made.BindFlags |= usage.storage ? static_cast<UINT>(D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE) : 0U;

    const bool immutable = description.update == BufferUpdate::Immutable && !usage.copyDestination && !usage.storage && made.BindFlags != 0U;
    made.Usage = immutable ? D3D11_USAGE_IMMUTABLE : (mapped ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT);
    made.CPUAccessFlags = mapped ? static_cast<UINT>(D3D11_CPU_ACCESS_WRITE) : 0U;
    made.MiscFlags |= usage.storage ? static_cast<UINT>(D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS) : 0U;
    made.MiscFlags |= usage.indirect ? static_cast<UINT>(D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS) : 0U;

    // The data fills the whole width of the buffer, which a uniform buffer rounds up past the data.
    std::span<const std::byte> initial = desc.data;
    std::vector<std::byte> padded;
    if (!initial.empty() && initial.size() < made.ByteWidth) {
        padded.resize(made.ByteWidth);
        std::ranges::copy(initial, padded.begin());
        initial = padded;
    }

    const D3D11_SUBRESOURCE_DATA data{.pSysMem = initial.data(), .SysMemPitch = 0, .SysMemSlicePitch = 0};
    BufferRecord record{.description = description, .mapped = mapped};
    HResult::check(device->CreateBuffer(&made, initial.empty() ? nullptr : &data, record.buffer.GetAddressOf()), std::format("create a buffer of {} bytes", description.size));
    setName(*record.buffer.Get(), desc.label);
    return {buffers.add(std::move(record))};
}

// Direct3D orders the subresources of a texture by layer and then by mip level, and those of a volume by mip level alone.
std::vector<D3D11_SUBRESOURCE_DATA> Device::describeLevels(const ImageDesc& desc) {
    const bool volume = desc.type == TextureType::Volume;
    std::vector<D3D11_SUBRESOURCE_DATA> data(volume ? desc.levels.size() : desc.levels.size() * static_cast<std::size_t>(desc.layers));
    for (std::size_t index = 0; index < desc.levels.size(); ++index) {
        const direct3d::MipLevels::Level level = direct3d::MipLevels::describe(desc.type, desc.width, desc.height, desc.layers, desc.format, static_cast<int>(index));
        if (volume) {
            data[index] = {.pSysMem = desc.levels[index].data(), .SysMemPitch = static_cast<UINT>(level.rowBytes), .SysMemSlicePitch = static_cast<UINT>(level.sliceBytes)};
            continue;
        }

        for (std::size_t layer = 0; layer < level.layers; ++layer) {
            data[index + layer * desc.levels.size()] = {.pSysMem = desc.levels[index].data() + layer * level.sliceBytes, .SysMemPitch = static_cast<UINT>(level.rowBytes), .SysMemSlicePitch = static_cast<UINT>(level.sliceBytes)};
        }
    }

    return data;
}

// A view reads the mip levels and layers of its description from the first ones it names, which are the first of the image for an image itself, and a cube array reads whole cubes from its first face.
ComPtr<ID3D11ShaderResourceView> Device::makeTextureView(ID3D11Resource& resource, const ImageRules::Description& description, std::string_view label) {
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = Formats::getViewFormat(description.format);
    const auto firstMip = static_cast<UINT>(description.baseMipLevel);
    const auto mips = static_cast<UINT>(description.mipLevels);
    const auto firstLayer = static_cast<UINT>(description.baseLayer);
    const auto layers = static_cast<UINT>(description.layers);
    const bool multisampled = description.sampleCount > 1;

    switch (description.type) {
    case TextureType::Flat:
        view.ViewDimension = multisampled ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
        view.Texture2D = {.MostDetailedMip = firstMip, .MipLevels = mips};
        break;
    case TextureType::Array:
        view.ViewDimension = multisampled ? D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        if (multisampled) {
            view.Texture2DMSArray = {.FirstArraySlice = firstLayer, .ArraySize = layers};
        } else {
            view.Texture2DArray = {.MostDetailedMip = firstMip, .MipLevels = mips, .FirstArraySlice = firstLayer, .ArraySize = layers};
        }

        break;
    case TextureType::Cube:
        view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        view.TextureCube = {.MostDetailedMip = firstMip, .MipLevels = mips};
        break;
    case TextureType::CubeArray:
        view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
        view.TextureCubeArray = {.MostDetailedMip = firstMip, .MipLevels = mips, .First2DArrayFace = firstLayer, .NumCubes = layers / 6};
        break;
    case TextureType::Volume:
        view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
        view.Texture3D = {.MostDetailedMip = firstMip, .MipLevels = mips};
        break;
    }

    ComPtr<ID3D11ShaderResourceView> made;
    HResult::check(device->CreateShaderResourceView(&resource, &view, made.GetAddressOf()), std::format("create the view of the image \"{}\"", label));
    setName(*made.Get(), label);
    return made;
}

// Direct3D 11 counts the subresources of a texture by layer and then by mip level, and those of a volume by mip level alone, whose slices a box reaches.
UINT Device::getSubresource(const ImageRules::Description& description, int mipLevel, int layer) noexcept {
    const int slice = description.type == TextureType::Volume ? 0 : layer;
    return static_cast<UINT>(mipLevel) + (static_cast<UINT>(slice) * static_cast<UINT>(description.mipLevels));
}

// A dynamic image or a render target of more than one mip level, of a format whose mip levels the driver generates, takes the binds that `GenerateMips` draws through. Cubes and cube arrays are arrays of faces that views read as cubes, and depth images are typeless resources that depth stencil views write and shader resource views sample.
gpu::Device::Image Device::makeImage(const ImageDesc& desc) {
    const ImageRules::Description description = ImageRules::check(desc, capabilities);
    const bool generates = (desc.kind == ImageKind::Dynamic || desc.kind == ImageKind::RenderTarget) && desc.mipLevels > 1 && capabilities.getFormat(desc.format).mipmaps;

    UINT bind = D3D11_BIND_SHADER_RESOURCE;
    bind |= desc.kind == ImageKind::RenderTarget || generates ? static_cast<UINT>(D3D11_BIND_RENDER_TARGET) : 0U;
    bind |= desc.kind == ImageKind::DepthStencil ? static_cast<UINT>(D3D11_BIND_DEPTH_STENCIL) : 0U;
    UINT misc = generates ? static_cast<UINT>(D3D11_RESOURCE_MISC_GENERATE_MIPS) : 0U;
    misc |= desc.type == TextureType::Cube || desc.type == TextureType::CubeArray ? static_cast<UINT>(D3D11_RESOURCE_MISC_TEXTURECUBE) : 0U;
    const D3D11_USAGE usage = desc.kind == ImageKind::Texture ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DEFAULT;
    const std::vector<D3D11_SUBRESOURCE_DATA> data = desc.kind == ImageKind::Texture ? describeLevels(desc) : std::vector<D3D11_SUBRESOURCE_DATA>{};
    const D3D11_SUBRESOURCE_DATA* initial = data.empty() ? nullptr : data.data();

    ImageRecord record{.description = description};
    if (desc.type == TextureType::Volume) {
        D3D11_TEXTURE3D_DESC made{};
        made.Width = static_cast<UINT>(desc.width);
        made.Height = static_cast<UINT>(desc.height);
        made.Depth = static_cast<UINT>(desc.layers);
        made.MipLevels = static_cast<UINT>(desc.mipLevels);
        made.Format = Formats::getResourceFormat(desc.format);
        made.Usage = usage;
        made.BindFlags = bind;
        made.MiscFlags = misc;
        ComPtr<ID3D11Texture3D> texture;
        HResult::check(device->CreateTexture3D(&made, initial, texture.GetAddressOf()), std::format("create the {}x{}x{} volume \"{}\"", desc.width, desc.height, desc.layers, desc.label));
        record.resource = texture;
    } else {
        D3D11_TEXTURE2D_DESC made{};
        made.Width = static_cast<UINT>(desc.width);
        made.Height = static_cast<UINT>(desc.height);
        made.MipLevels = static_cast<UINT>(desc.mipLevels);
        made.ArraySize = static_cast<UINT>(desc.layers);
        made.Format = Formats::getResourceFormat(desc.format);
        made.SampleDesc.Count = static_cast<UINT>(desc.sampleCount);
        made.Usage = usage;
        made.BindFlags = bind;
        made.MiscFlags = misc;
        ComPtr<ID3D11Texture2D> texture;
        HResult::check(device->CreateTexture2D(&made, initial, texture.GetAddressOf()), std::format("create the {}x{} image \"{}\"", desc.width, desc.height, desc.label));
        record.resource = texture;
    }

    ComPtr<ID3D11DeviceChild> child;
    record.resource.As(&child);
    setName(*child.Get(), desc.label);
    record.view = makeTextureView(*record.resource.Get(), description, desc.label);
    return {images.add(std::move(record))};
}

// The view shares the resource of its image, which keeps the id of the view, so destroying the image destroys the view with it. The image takes room for the id before the view exists, so a view that the table holds always joins its image.
gpu::Device::Image Device::makeView(const ViewDesc& desc) {
    ImageRecord& image = images.get(desc.image.id);
    const ImageRules::Description description = ImageRules::checkView(image.description, desc, capabilities);
    image.views.reserve(image.views.size() + 1);

    ImageRecord view{.resource = image.resource, .description = description};
    view.view = makeTextureView(*image.resource.Get(), description, desc.label);
    const std::uint64_t id = images.add(std::move(view));
    image.views.push_back(id);
    return {id};
}

gpu::Device::Sampler Device::makeSampler(const SamplerDesc& desc) {
    SamplerRules::check(desc, capabilities);
    ComPtr<ID3D11SamplerState> sampler = StateObjects::makeSampler(*device.Get(), desc);
    setName(*sampler.Get(), desc.label);
    return {samplers.add(std::move(sampler))};
}

// The context keeps the views it binds by their registers, as many as the slots of the device.
std::vector<Device::Register> Device::readRegisters(std::span<const TextureBinding> textures) {
    std::vector<Register> registers;
    for (const TextureBinding& texture : textures) {
        if (texture.binding >= static_cast<int>(kMaxTextures)) {
            throw ContractError(ContractError::Rule::Program);
        }

        registers.push_back({.slot = texture.slot, .stage = texture.stage == Stage::Vertex ? 0U : 1U, .number = static_cast<UINT>(texture.binding)});
    }

    return registers;
}

std::vector<Device::Register> Device::readRegisters(std::span<const SamplerBinding> samplersOfProgram) {
    std::vector<Register> registers;
    for (const SamplerBinding& sampler : samplersOfProgram) {
        if (sampler.binding >= static_cast<int>(kMaxSamplers)) {
            throw ContractError(ContractError::Rule::Program);
        }

        registers.push_back({.slot = sampler.slot, .stage = sampler.stage == Stage::Vertex ? 0U : 1U, .number = static_cast<UINT>(sampler.binding)});
    }

    return registers;
}

gpu::Device::Program Device::makeProgram(const ProgramDesc& desc) {
    direct3d::ShaderRegisters::check(desc);

    ProgramRecord record;
    for (const BlockBinding& block : desc.blocks) {
        if (block.size > capabilities.maxUniformBlockSize || block.binding >= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT) {
            throw ContractError(ContractError::Rule::Program);
        }

        record.blocks[static_cast<std::size_t>(block.slot)] = Register{.slot = block.slot, .stage = block.stage == Stage::Vertex ? 0U : 1U, .number = static_cast<UINT>(block.binding), .size = block.size};
    }

    record.textures = readRegisters(desc.textures);
    record.samplers = readRegisters(desc.samplers);

    record.vertexCode = compiler->compile(desc.vertex, desc.label);
    const ComPtr<ID3DBlob> fragmentCode = compiler->compile(desc.fragment, desc.label);
    HResult::check(device->CreateVertexShader(record.vertexCode->GetBufferPointer(), record.vertexCode->GetBufferSize(), nullptr, record.vertex.GetAddressOf()), std::format("create the vertex shader of \"{}\"", desc.label));
    HResult::check(device->CreatePixelShader(fragmentCode->GetBufferPointer(), fragmentCode->GetBufferSize(), nullptr, record.fragment.GetAddressOf()), std::format("create the pixel shader of \"{}\"", desc.label));
    setName(*record.vertex.Get(), desc.label);
    setName(*record.fragment.Get(), desc.label);
    return {programs.add(std::move(record))};
}

void Device::prepareProgram(const ProgramDesc& desc, core::JobSystem&) {
    compiler->prepare(desc);
}

gpu::Device::Pipeline Device::makePipeline(const PipelineDesc& desc) {
    const ProgramRecord& program = programs.get(desc.program.id);

    if ((desc.colors.empty() && !desc.depthStencil.format) || desc.colors.size() > kMaxColorTargets || desc.buffers.size() > kMaxVertexBuffers) {
        throw ContractError(ContractError::Rule::Pipeline);
    }

    std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
    for (const VertexAttribute& attribute : desc.attributes) {
        if (attribute.buffer < 0 || attribute.buffer >= static_cast<int>(desc.buffers.size())) {
            throw ContractError(ContractError::Rule::Pipeline);
        }

        const bool instanced = desc.buffers[static_cast<std::size_t>(attribute.buffer)].perInstance;
        elements.push_back({.SemanticName = "TEXCOORD", .SemanticIndex = static_cast<UINT>(attribute.location), .Format = Formats::getVertexFormat(attribute.format), .InputSlot = static_cast<UINT>(attribute.buffer), .AlignedByteOffset = static_cast<UINT>(attribute.offset), .InputSlotClass = instanced ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA, .InstanceDataStepRate = instanced ? 1U : 0U});
    }

    PipelineRecord record{.vertex = program.vertex, .fragment = program.fragment, .blocks = program.blocks, .textures = program.textures, .samplers = program.samplers};
    if (!elements.empty()) {
        HResult::check(device->CreateInputLayout(elements.data(), static_cast<UINT>(elements.size()), program.vertexCode->GetBufferPointer(), program.vertexCode->GetBufferSize(), record.layout.GetAddressOf()), std::format("create the vertex layout of the pipeline \"{}\"", desc.label));
        setName(*record.layout.Get(), desc.label);
    }

    record.blend = StateObjects::makeBlend(*device.Get(), desc);
    record.rasterizer = StateObjects::makeRasterizer(*device.Get(), desc);
    record.depthStencil = StateObjects::makeDepthStencil(*device.Get(), desc);
    record.topology = StateObjects::toTopology(desc.primitive);
    record.bufferCount = static_cast<UINT>(desc.buffers.size());
    for (std::size_t index = 0; index < desc.buffers.size(); ++index) {
        record.strides[index] = static_cast<UINT>(desc.buffers[index].stride);
    }

    record.indexFormat = Formats::getIndexFormat(desc.indexType);
    return {pipelines.add(std::move(record))};
}

void Device::destroy(Buffer buffer) noexcept {
    buffers.remove(buffer.id);
}

// An image destroys its views with it, and a view leaves its image, which forgets the id of the view.
void Device::destroy(Image image) noexcept {
    const std::optional<ImageRecord> record = images.remove(image.id);
    if (!record) {
        return;
    }

    if (record->description.isView()) {
        ImageRecord* viewed = images.find(record->description.viewed.id);
        if (viewed != nullptr) {
            std::erase(viewed->views, image.id);
        }

        return;
    }

    for (const std::uint64_t view : record->views) {
        images.remove(view);
    }
}

void Device::destroy(Sampler sampler) noexcept {
    samplers.remove(sampler.id);
}

void Device::destroy(Program program) noexcept {
    programs.remove(program.id);
}

void Device::destroy(Pipeline pipeline) noexcept {
    pipelines.remove(pipeline.id);

    if (bound.pipeline == pipeline.id) {
        bound.pipeline = 0;
    }
}

void Device::writeBuffer(Buffer buffer, std::size_t offset, std::span<const std::byte> data) {
    if (passOpen) {
        throw ContractError(ContractError::Rule::BufferWrite);
    }

    BufferRecord& record = buffers.get(buffer.id);
    BufferRules::checkWrite(record.description, offset, data.size());
    if (data.empty()) {
        return;
    }

    openFrame();

    if (record.mapped) {
        writeMapped(record, offset, data);
        return;
    }

    writeCopied(record, offset, data);
}

// A stream buffer holds in a frame only what that frame wrote, so the first write of a frame discards the buffer, which makes the driver hand over a fresh copy while the GPU still reads the old one, and the later writes of the frame fill other parts of the same copy.
void Device::writeMapped(BufferRecord& record, std::size_t offset, std::span<const std::byte> data) {
    const D3D11_MAP mode = record.writtenFrame == frameNumber ? D3D11_MAP_WRITE_NO_OVERWRITE : D3D11_MAP_WRITE_DISCARD;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    const HRESULT result = context->Map(record.buffer.Get(), 0, mode, 0, &mapped);
    if (HResult::isDeviceLoss(result)) {
        loseDevice(result);
        return;
    }

    HResult::check(result, "map a stream buffer");
    std::memcpy(static_cast<std::byte*>(mapped.pData) + offset, data.data(), data.size());
    context->Unmap(record.buffer.Get(), 0);
    record.writtenFrame = frameNumber;
}

// The bytes go through the ring of uploads in pieces that fit it, and the context copies each piece into the buffer at this point of its commands, so the passes before the write read the old bytes and the passes after it the new ones. A ring that starts over hands the GPU a fresh copy of itself, so the copies recorded before keep reading their pieces.
void Device::writeCopied(const BufferRecord& record, std::size_t offset, std::span<const std::byte> data) {
    for (std::size_t written = 0; written < data.size(); written += kUploadRingSize) {
        const std::span<const std::byte> part = data.subspan(written, std::min<std::size_t>(kUploadRingSize, data.size() - written));
        const std::optional<DynamicRing::Piece> piece = uploads->write(*context.Get(), part);
        if (!piece) {
            loseDevice(DXGI_ERROR_DEVICE_REMOVED);
            return;
        }

        const D3D11_BOX box{.left = piece->offset, .top = 0, .front = 0, .right = piece->offset + static_cast<UINT>(part.size()), .bottom = 1, .back = 1};
        context->CopySubresourceRegion(record.buffer.Get(), 0, static_cast<UINT>(offset + written), 0, 0, uploads->get(), 0, &box);
    }
}

// The context orders the copy with the passes, writes and copies around it.
void Device::copyBuffer(Buffer source, std::size_t sourceOffset, Buffer destination, std::size_t destinationOffset, std::size_t size) {
    if (passOpen) {
        throw ContractError(ContractError::Rule::BufferWrite);
    }

    const BufferRecord& from = buffers.get(source.id);
    const BufferRecord& to = buffers.get(destination.id);
    BufferRules::checkCopy(source, from.description, sourceOffset, destination, to.description, destinationOffset, size);
    openFrame();

    const D3D11_BOX box{.left = static_cast<UINT>(sourceOffset), .top = 0, .front = 0, .right = static_cast<UINT>(sourceOffset + size), .bottom = 1, .back = 1};
    context->CopySubresourceRegion(to.buffer.Get(), 0, static_cast<UINT>(destinationOffset), 0, 0, from.buffer.Get(), 0, &box);
}

// Direct3D 11 copies no buffer into a texture, so the driver stages the pixels of the box in the subresource of the mip level and layer, or in the slice of a volume, and a write of the whole of an image of one subresource lets it hand the GPU a fresh copy instead of waiting for the frames that read the old one.
void Device::writeImages(std::span<const ImageWrite> writes) {
    for (const ImageWrite& write : writes) {
        ImageRules::checkWrite(images.get(write.image.id).description, write);
    }

    ImageRules::checkDistinctWrites(writes);
    openFrame();

    for (const ImageWrite& write : writes) {
        const ImageRecord& image = images.get(write.image.id);
        const ImageRules::Description& description = image.description;
        const ImageInfo::Extent level = description.getLevelExtent(write.mipLevel);
        const Region& region = write.region;
        const auto front = static_cast<UINT>(description.type == TextureType::Volume ? write.layer : 0);
        const D3D11_BOX box{.left = static_cast<UINT>(region.x), .top = static_cast<UINT>(region.y), .front = front, .right = static_cast<UINT>(region.x + region.width), .bottom = static_cast<UINT>(region.y + region.height), .back = front + 1};

        const auto row = static_cast<UINT>(ImageInfo::getRowBytes(description.format, region.width));
        const bool whole = description.mipLevels == 1 && level.depth == 1 && region.width == level.width && region.height == level.height;
        const UINT flags = whole ? static_cast<UINT>(D3D11_COPY_DISCARD) : 0U;
        context->UpdateSubresource1(image.resource.Get(), getSubresource(description, write.mipLevel, write.layer), &box, write.pixels.data(), row, static_cast<UINT>(write.pixels.size()), flags);
    }
}

// The driver fills the smaller mip levels from the first one through the view of the whole image, at this point of the commands of the context.
void Device::generateMipmaps(Image image) {
    if (passOpen) {
        throw ContractError(ContractError::Rule::Mipmaps);
    }

    const ImageRecord& record = images.get(image.id);
    ImageRules::checkMipmaps(record.description, capabilities);
    openFrame();
    context->GenerateMips(record.view.Get());
}

// The frame waits for the GPU to finish the frame that used the same slot, so the GPU runs at most `kFramesInFlight` frames behind. The signal of that frame may still wait in the commands that the driver holds, which the context hands over before the CPU sleeps. A removed device passes every fence value at once, so no wait outlives it.
void Device::openFrame() {
    if (frameOpen) {
        return;
    }

    frameOpen = true;
    const std::uint64_t value = frameFences[frameIndex];
    if (!lost && fence->GetCompletedValue() < value) {
        context->Flush();

        HResult::check(fence->SetEventOnCompletion(value, fenceEvent.Get()), "wait for a frame of the GPU");
        WaitForSingleObject(fenceEvent.Get(), INFINITE);
    }

    timer->begin(*context.Get());
}

bool Device::prepareSwapchain(SurfaceChain& chain) {
    if (!chain.surface || chain.surface->size.width <= 0 || chain.surface->size.height <= 0 || lost) {
        return false;
    }

    if (chain.swapchain == nullptr) {
        chain.swapchain = std::make_unique<direct3d::Swapchain>(*factory.Get(), *device.Get(), *chain.surface);
        makeSwapchainTargets(chain);
        return true;
    }

    // A new size applies from the first pass of a frame into the swapchain, so the passes of one frame draw into the same buffers.
    if (chain.presentPending || chain.swapchain->getSize() == chain.surface->size) {
        return true;
    }

    // The buffers resize only once nothing refers to them, the views of the device and the bindings of the context.
    releaseSwapchainTargets(chain);
    context->OMSetRenderTargets(0, nullptr, nullptr);
    context->Flush();
    const HRESULT result = chain.swapchain->resize(chain.surface->size);
    if (HResult::isDeviceLoss(result)) {
        loseDevice(result);
        return false;
    }

    HResult::check(result, "resize the swapchain");

    // The driver keeps the buffers of every earlier size until the device is trimmed, which piles them up while a drag resizes the window every frame.
    ComPtr<IDXGIDevice3> dxgiDevice;
    HResult::check(device.As(&dxgiDevice), "reach the DXGI device to trim it");
    dxgiDevice->Trim();
    makeSwapchainTargets(chain);
    return true;
}

void Device::makeSwapchainTargets(SurfaceChain& chain) {
    ComPtr<ID3D11Texture2D> backBuffer;
    HResult::check(chain.swapchain->get().GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf())), "reach the buffer of the swapchain");
    HResult::check(device->CreateRenderTargetView(backBuffer.Get(), nullptr, chain.backBufferView.GetAddressOf()), "create the view of the swapchain");

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(chain.swapchain->getSize().width);
    desc.Height = static_cast<UINT>(chain.swapchain->getSize().height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = static_cast<UINT>(swapchainSampleCount);
    desc.Usage = D3D11_USAGE_DEFAULT;

    // Flip model swapchains hold one sample, so a multisampled window draws into a texture of its own that resolves into the swapchain when the frame is presented.
    if (desc.SampleDesc.Count > 1) {
        desc.Format = Formats::kSwapchainColor;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        HResult::check(device->CreateTexture2D(&desc, nullptr, chain.multisampleColor.GetAddressOf()), "create the multisampled color of the swapchain");
        HResult::check(device->CreateRenderTargetView(chain.multisampleColor.Get(), nullptr, chain.multisampleView.GetAddressOf()), "create the view of the multisampled color of the swapchain");
        setName(*chain.multisampleColor.Get(), "haylen-swapchain-color");
    }

    desc.Format = Formats::getDepthFormat(Formats::kSwapchainDepthFormat);
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> depth;
    HResult::check(device->CreateTexture2D(&desc, nullptr, depth.GetAddressOf()), "create the depth and stencil of the swapchain");
    HResult::check(device->CreateDepthStencilView(depth.Get(), nullptr, chain.depthView.GetAddressOf()), "create the view of the depth and stencil of the swapchain");
    setName(*depth.Get(), "haylen-swapchain-depth");
}

void Device::releaseSwapchainTargets(SurfaceChain& chain) {
    chain.backBufferView.Reset();
    chain.multisampleView.Reset();
    chain.multisampleColor.Reset();
    chain.depthView.Reset();
}

// The context lets go of the views before the swapchain goes, so its window takes another swapchain at once.
void Device::releaseChain(SurfaceChain& chain) {
    context->OMSetRenderTargets(0, nullptr, nullptr);
    releaseSwapchainTargets(chain);
    chain.swapchain.reset();
    context->Flush();
}

Device::SurfaceChain& Device::requireChain(int number) const {
    if (number < 0 || static_cast<std::size_t>(number) >= chains.size() || chains[static_cast<std::size_t>(number)] == nullptr) {
        throw ContractError(ContractError::Rule::Surface);
    }

    return *chains[static_cast<std::size_t>(number)];
}

Device::SurfaceChain& Device::requireAddedChain(int number) const {
    if (number == 0) {
        throw ContractError(ContractError::Rule::Surface);
    }

    return requireChain(number);
}

// A render target sampled by one pass is drawn by a later one, and the context refuses to bind an image both ways, so a pass starts with no texture bound.
void Device::unbindTextures() {
    static constexpr std::array<ID3D11ShaderResourceView*, kMaxTextures> kNone{};
    if (bound.textureEnds[0] > 0) {
        context->VSSetShaderResources(0, bound.textureEnds[0], kNone.data());
    }

    if (bound.textureEnds[1] > 0) {
        context->PSSetShaderResources(0, bound.textureEnds[1], kNone.data());
    }

    bound.textures = {};
    bound.textureEnds = {};
}

void Device::setViewportAndScissor(int width, int height) {
    const D3D11_VIEWPORT viewport{.TopLeftX = 0.0F, .TopLeftY = 0.0F, .Width = static_cast<float>(width), .Height = static_cast<float>(height), .MinDepth = 0.0F, .MaxDepth = 1.0F};
    const D3D11_RECT scissor{.left = 0, .top = 0, .right = width, .bottom = height};
    context->RSSetViewports(1, &viewport);
    context->RSSetScissorRects(1, &scissor);
}

// The pass opens once its attachments are bound, so a pass that fails to begin leaves no pass open.
void Device::beginPass(const PassDesc& desc) {
    if (passOpen) {
        throw ContractError(ContractError::Rule::PassOrder);
    }

    openFrame();
    pass = {};
    unbindTextures();

    if (desc.colors.empty() && !desc.depthStencil) {
        beginSwapchainPass(desc);
    } else {
        beginOffscreenPass(desc);
    }

    passOpen = true;

    if (annotation != nullptr && !desc.label.empty()) {
        annotation->BeginEvent(markerText.convert(desc.label));
        pass.labeled = true;
    }
}

void Device::beginSwapchainPass(const PassDesc& desc) {
    SurfaceChain& chain = requireChain(desc.surface);
    skipping = !prepareSwapchain(chain);
    if (skipping) {
        return;
    }

    chain.presentPending = true;
    pass.colors[0] = chain.multisampleView != nullptr ? chain.multisampleView.Get() : chain.backBufferView.Get();
    pass.colorCount = 1;
    pass.depthStencil = chain.depthView.Get();
    context->OMSetRenderTargets(1, pass.colors.data(), pass.depthStencil);

    if (desc.load == Load::Clear) {
        const std::array<float, 4> clear{desc.clear.r, desc.clear.g, desc.clear.b, desc.clear.a};
        context->ClearRenderTargetView(pass.colors[0], clear.data());
        context->ClearDepthStencilView(pass.depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, desc.clearDepth, desc.clearStencil);
    } else if (desc.load == Load::DontCare) {
        context->DiscardView(pass.colors[0]);
        context->DiscardView(pass.depthStencil);
    }

    setViewportAndScissor(chain.swapchain->getSize().width, chain.swapchain->getSize().height);
}

void Device::beginOffscreenPass(const PassDesc& desc) {
    const PassRules::Extent extent = PassRules::check(desc, capabilities, [this](Image image) -> const ImageRules::Description& { return images.get(image.id).description; });

    for (std::size_t index = 0; index < desc.colors.size(); ++index) {
        const ColorAttachment& color = desc.colors[index];
        ImageRecord& image = images.get(color.image.id);
        ID3D11RenderTargetView* view = getAttachment(image, color.mipLevel, color.layer).color.Get();
        pass.colors[index] = view;
        pass.discardColors[index] = color.store == Store::Discard;

        if (color.load == Load::Clear) {
            const std::array<float, 4> clear{color.clear.r, color.clear.g, color.clear.b, color.clear.a};
            context->ClearRenderTargetView(view, clear.data());
        } else if (color.load == Load::DontCare) {
            context->DiscardView(view);
        }

        if (color.resolve.isValid()) {
            const ImageRecord& resolve = images.get(color.resolve.id);
            const UINT source = getSubresource(image.description, color.mipLevel, color.layer);
            const UINT destination = getSubresource(resolve.description, color.resolveMipLevel, color.resolveLayer);
            pass.resolves[index] = {.source = image.resource.Get(), .sourceSubresource = source, .destination = resolve.resource.Get(), .destinationSubresource = destination, .format = Formats::getViewFormat(image.description.format)};
        }
    }

    pass.colorCount = static_cast<UINT>(desc.colors.size());

    if (desc.depthStencil) {
        const DepthStencilAttachment& attachment = *desc.depthStencil;
        ImageRecord& image = images.get(attachment.image.id);
        pass.depthStencil = getAttachment(image, attachment.mipLevel, attachment.layer).depth.Get();
        const bool stencil = ImageInfo::hasStencil(image.description.format);
        pass.discardDepthStencil = attachment.depthStore == Store::Discard && (!stencil || attachment.stencilStore == Store::Discard);

        UINT clear = attachment.depthLoad == Load::Clear ? static_cast<UINT>(D3D11_CLEAR_DEPTH) : 0U;
        clear |= stencil && attachment.stencilLoad == Load::Clear ? static_cast<UINT>(D3D11_CLEAR_STENCIL) : 0U;
        if (clear != 0) {
            context->ClearDepthStencilView(pass.depthStencil, clear, attachment.clearDepth, attachment.clearStencil);
        } else if (attachment.depthLoad == Load::DontCare && (!stencil || attachment.stencilLoad == Load::DontCare)) {
            context->DiscardView(pass.depthStencil);
        }
    }

    context->OMSetRenderTargets(pass.colorCount, pass.colors.data(), pass.depthStencil);
    setViewportAndScissor(extent.width, extent.height);
}

Device::Attachment& Device::getAttachment(ImageRecord& image, int mipLevel, int layer) {
    for (Attachment& attachment : image.attachments) {
        if (attachment.mipLevel == mipLevel && attachment.layer == layer) {
            return attachment;
        }
    }

    // The view joins the attachments of the image only once it exists, so a view that fails to be made leaves nothing behind.
    Attachment attachment{.mipLevel = mipLevel, .layer = layer};
    const auto mip = static_cast<UINT>(mipLevel);
    const auto slice = static_cast<UINT>(layer);
    const ImageRules::Description& description = image.description;
    const bool multisampled = description.sampleCount > 1;

    // A depth stencil view of an array, a cube or a cube array writes one of its layers or faces at a mip level.
    if (description.kind == ImageKind::DepthStencil) {
        D3D11_DEPTH_STENCIL_VIEW_DESC view{};
        view.Format = Formats::getDepthFormat(description.format);
        if (description.type == TextureType::Flat) {
            view.ViewDimension = multisampled ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
            view.Texture2D.MipSlice = mip;
        } else {
            view.ViewDimension = multisampled ? D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY : D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
            if (multisampled) {
                view.Texture2DMSArray = {.FirstArraySlice = slice, .ArraySize = 1};
            } else {
                view.Texture2DArray = {.MipSlice = mip, .FirstArraySlice = slice, .ArraySize = 1};
            }
        }

        HResult::check(device->CreateDepthStencilView(image.resource.Get(), &view, attachment.depth.GetAddressOf()), "create the depth stencil view of a pass");
        return image.attachments.emplace_back(std::move(attachment));
    }

    D3D11_RENDER_TARGET_VIEW_DESC view{};
    view.Format = Formats::getViewFormat(description.format);
    switch (description.type) {
    case TextureType::Flat:
        view.ViewDimension = multisampled ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
        view.Texture2D.MipSlice = mip;
        break;
    case TextureType::Array:
    case TextureType::Cube:
    case TextureType::CubeArray:
        view.ViewDimension = multisampled ? D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        if (multisampled) {
            view.Texture2DMSArray = {.FirstArraySlice = slice, .ArraySize = 1};
        } else {
            view.Texture2DArray = {.MipSlice = mip, .FirstArraySlice = slice, .ArraySize = 1};
        }

        break;
    case TextureType::Volume:
        view.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
        view.Texture3D = {.MipSlice = mip, .FirstWSlice = slice, .WSize = 1};
        break;
    }

    HResult::check(device->CreateRenderTargetView(image.resource.Get(), &view, attachment.color.GetAddressOf()), "create the render target view of a pass");
    return image.attachments.emplace_back(std::move(attachment));
}

void Device::applyViewport(const math::Rect& pixels) {
    if (skipping) {
        return;
    }

    const D3D11_VIEWPORT viewport{.TopLeftX = pixels.x, .TopLeftY = pixels.y, .Width = pixels.width, .Height = pixels.height, .MinDepth = 0.0F, .MaxDepth = 1.0F};
    context->RSSetViewports(1, &viewport);
}

void Device::applyScissor(const math::Rect& pixels) {
    if (skipping) {
        return;
    }

    const D3D11_RECT scissor{.left = std::lround(pixels.x), .top = std::lround(pixels.y), .right = std::lround(pixels.x + pixels.width), .bottom = std::lround(pixels.y + pixels.height)};
    context->RSSetScissorRects(1, &scissor);
}

void Device::applyPipeline(Pipeline pipeline) {
    blocksApplied = {};
    blocksFromBuffers = {};

    if (skipping || bound.pipeline == pipeline.id) {
        return;
    }

    const PipelineRecord& record = pipelines.get(pipeline.id);
    bound.pipeline = pipeline.id;
    bound.indexed = record.indexFormat != DXGI_FORMAT_UNKNOWN;

    if (bound.layout != record.layout.Get()) {
        bound.layout = record.layout.Get();
        context->IASetInputLayout(bound.layout);
    }

    if (bound.topology != record.topology) {
        bound.topology = record.topology;
        context->IASetPrimitiveTopology(bound.topology);
    }

    if (bound.vertex != record.vertex.Get()) {
        bound.vertex = record.vertex.Get();
        context->VSSetShader(bound.vertex, nullptr, 0);
    }

    if (bound.fragment != record.fragment.Get()) {
        bound.fragment = record.fragment.Get();
        context->PSSetShader(bound.fragment, nullptr, 0);
    }

    if (bound.blend != record.blend.Get()) {
        bound.blend = record.blend.Get();
        context->OMSetBlendState(bound.blend, blendColor.data(), D3D11_DEFAULT_SAMPLE_MASK);
    }

    if (bound.rasterizer != record.rasterizer.Get()) {
        bound.rasterizer = record.rasterizer.Get();
        context->RSSetState(bound.rasterizer);
    }

    if (bound.depthStencil != record.depthStencil.Get()) {
        bound.depthStencil = record.depthStencil.Get();
        context->OMSetDepthStencilState(bound.depthStencil, stencilReference);
    }
}

void Device::applyBindings(const Bindings& bindings) {
    if (skipping) {
        return;
    }

    const PipelineRecord& pipeline = pipelines.get(bound.pipeline);

    std::array<ID3D11Buffer*, kMaxVertexBuffers> vertexBuffers{};
    std::array<UINT, kMaxVertexBuffers> offsets{};
    bool changed = false;
    for (std::size_t index = 0; index < pipeline.bufferCount; ++index) {
        const BufferRecord& buffer = buffers.get(bindings.vertexBuffers[index].id);
        BufferRules::checkVertex(buffer.description);
        vertexBuffers[index] = buffer.buffer.Get();
        offsets[index] = static_cast<UINT>(bindings.vertexOffsets[index]);
        changed = changed || vertexBuffers[index] != bound.vertexBuffers[index] || offsets[index] != bound.vertexOffsets[index] || pipeline.strides[index] != bound.vertexStrides[index];
    }

    if (changed) {
        context->IASetVertexBuffers(0, pipeline.bufferCount, vertexBuffers.data(), pipeline.strides.data(), offsets.data());
        bound.vertexBuffers = vertexBuffers;
        bound.vertexOffsets = offsets;
        bound.vertexStrides = pipeline.strides;
    }

    if (pipeline.indexFormat != DXGI_FORMAT_UNKNOWN) {
        const BufferRecord& buffer = buffers.get(bindings.indexBuffer.id);
        BufferRules::checkIndex(buffer.description);
        ID3D11Buffer* index = buffer.buffer.Get();
        const auto offset = static_cast<UINT>(bindings.indexOffset);
        if (index != bound.indexBuffer || offset != bound.indexOffset || pipeline.indexFormat != bound.indexFormat) {
            context->IASetIndexBuffer(index, pipeline.indexFormat, offset);
            bound.indexBuffer = index;
            bound.indexOffset = offset;
            bound.indexFormat = pipeline.indexFormat;
        }
    }

    bindUniformBuffers(pipeline, bindings);
    applyTextures(pipeline, bindings);
    applySamplers(pipeline, bindings);
}

// A block reads the range of the uniform buffer that the bindings name at its slot, which `applyUniforms` never fed since the pipeline, and the ring of uniforms feeds the other blocks.
void Device::bindUniformBuffers(const PipelineRecord& pipeline, const Bindings& bindings) {
    for (std::size_t slot = 0; slot < kMaxBlocks; ++slot) {
        const std::optional<Register>& block = pipeline.blocks[slot];
        const BufferRange& range = bindings.uniformBuffers[slot];
        if (!block || !range.buffer.isValid()) {
            blocksFromBuffers[slot] = false;
            continue;
        }

        if (blocksApplied[slot]) {
            throw ContractError(ContractError::Rule::Uniforms);
        }

        const BufferRecord& buffer = buffers.get(range.buffer.id);
        BufferRules::checkUniform(buffer.description, range.offset, block->size);
        bindBlock(*block, buffer.buffer.Get(), static_cast<UINT>(range.offset));
        blocksFromBuffers[slot] = true;
    }
}

void Device::applyTextures(const PipelineRecord& pipeline, const Bindings& bindings) {
    for (const Register& texture : pipeline.textures) {
        const ImageRecord& image = images.get(bindings.textures[static_cast<std::size_t>(texture.slot)].id);
        ImageRules::checkSampled(image.description);
        ID3D11ShaderResourceView* view = image.view.Get();
        ID3D11ShaderResourceView*& held = bound.textures[texture.stage][texture.number];
        if (held == view) {
            continue;
        }

        held = view;
        bound.textureEnds[texture.stage] = std::max(bound.textureEnds[texture.stage], texture.number + 1);

        if (texture.stage == 0) {
            context->VSSetShaderResources(texture.number, 1, &view);
        } else {
            context->PSSetShaderResources(texture.number, 1, &view);
        }
    }
}

void Device::applySamplers(const PipelineRecord& pipeline, const Bindings& bindings) {
    for (const Register& sampler : pipeline.samplers) {
        ID3D11SamplerState* state = samplers.get(bindings.samplers[static_cast<std::size_t>(sampler.slot)].id).Get();
        ID3D11SamplerState*& held = bound.samplers[sampler.stage][sampler.number];
        if (held == state) {
            continue;
        }

        held = state;

        if (sampler.stage == 0) {
            context->VSSetSamplers(sampler.number, 1, &state);
        } else {
            context->PSSetSamplers(sampler.number, 1, &state);
        }
    }
}

// Each draw reads its uniforms from its own window of the ring.
void Device::applyUniforms(int slot, std::span<const std::byte> data) {
    if (skipping) {
        return;
    }

    const PipelineRecord& pipeline = pipelines.get(bound.pipeline);
    const bool known = slot >= 0 && slot < static_cast<int>(kMaxBlocks) && pipeline.blocks[static_cast<std::size_t>(slot)].has_value();
    if (!known || data.size() != pipeline.blocks[static_cast<std::size_t>(slot)]->size) {
        throw ContractError(ContractError::Rule::Uniforms);
    }

    const auto index = static_cast<std::size_t>(slot);
    if (blocksFromBuffers[index]) {
        throw ContractError(ContractError::Rule::Uniforms);
    }

    blockBytes[index].assign(data.begin(), data.end());
    blocksApplied[index] = true;

    const std::optional<DynamicRing::Piece> piece = uniforms->write(*context.Get(), data);
    if (!piece) {
        loseDevice(DXGI_ERROR_DEVICE_REMOVED);
        return;
    }

    bindBlock(*pipeline.blocks[index], uniforms->get(), piece->offset);

    if (piece->restarted) {
        rewriteBlocks(pipeline, index);
    }
}

// Direct3D 11.1 binds a window of a constant buffer by its offset in constants of 16 bytes, a multiple of 16 constants.
void Device::bindBlock(const Register& block, ID3D11Buffer* buffer, UINT offset) {
    const UINT first = offset / 16;
    const UINT count = (block.size + kUniformAlignment - 1) / kUniformAlignment * (kUniformAlignment / 16);
    if (block.stage == 0) {
        context->VSSetConstantBuffers1(block.number, 1, &buffer, &first, &count);
    } else {
        context->PSSetConstantBuffers1(block.number, 1, &buffer, &first, &count);
    }
}

// A ring that started over hands the GPU a fresh copy of its buffer, which lacks the other blocks that the next draw reads, so they go into it again.
void Device::rewriteBlocks(const PipelineRecord& pipeline, std::size_t written) {
    for (std::size_t index = 0; index < kMaxBlocks; ++index) {
        if (index == written || !blocksApplied[index]) {
            continue;
        }

        const std::optional<DynamicRing::Piece> piece = uniforms->write(*context.Get(), blockBytes[index]);
        if (!piece) {
            loseDevice(DXGI_ERROR_DEVICE_REMOVED);
            return;
        }

        bindBlock(*pipeline.blocks[index], uniforms->get(), piece->offset);
    }
}

void Device::applyStencilReference(std::uint8_t value) {
    stencilReference = value;
    if (!skipping && bound.depthStencil != nullptr) {
        context->OMSetDepthStencilState(bound.depthStencil, stencilReference);
    }
}

void Device::applyBlendColor(math::Color value) {
    blendColor = {value.r, value.g, value.b, value.a};
    if (!skipping && bound.blend != nullptr) {
        context->OMSetBlendState(bound.blend, blendColor.data(), D3D11_DEFAULT_SAMPLE_MASK);
    }
}

void Device::draw(int first, int count, int instances) {
    if (!passOpen) {
        throw ContractError(ContractError::Rule::PassOrder);
    }

    if (skipping || count <= 0 || instances <= 0) {
        return;
    }

    if (bound.indexed) {
        context->DrawIndexedInstanced(static_cast<UINT>(count), static_cast<UINT>(instances), static_cast<UINT>(first), 0, 0);
        return;
    }

    context->DrawInstanced(static_cast<UINT>(count), static_cast<UINT>(instances), static_cast<UINT>(first), 0);
}

// Resolving reads the multisampled attachment, so the discards that tile GPUs skip writing back come after it.
void Device::endPass() {
    if (!passOpen) {
        throw ContractError(ContractError::Rule::PassOrder);
    }

    if (!skipping) {
        for (UINT index = 0; index < pass.colorCount; ++index) {
            const PassState::Resolve& resolve = pass.resolves[index];
            if (resolve.source != nullptr) {
                context->ResolveSubresource(resolve.destination, resolve.destinationSubresource, resolve.source, resolve.sourceSubresource, resolve.format);
            }

            if (pass.discardColors[index]) {
                context->DiscardView(pass.colors[index]);
            }
        }

        if (pass.discardDepthStencil) {
            context->DiscardView(pass.depthStencil);
        }
    }

    if (pass.labeled) {
        annotation->EndEvent();
    }

    passOpen = false;
    skipping = false;
}

void Device::pushDebugGroup(std::string_view name) {
    if (annotation != nullptr) {
        annotation->BeginEvent(markerText.convert(name));
    }
}

void Device::popDebugGroup() {
    if (annotation != nullptr) {
        annotation->EndEvent();
    }
}

void Device::commit() {
    if (passOpen) {
        throw ContractError(ContractError::Rule::PassOrder);
    }

    openFrame();

    for (const std::unique_ptr<SurfaceChain>& chain : chains) {
        if (chain != nullptr && chain->presentPending && chain->multisampleColor != nullptr) {
            ComPtr<ID3D11Resource> backBuffer;
            chain->backBufferView->GetResource(backBuffer.GetAddressOf());
            context->ResolveSubresource(backBuffer.Get(), 0, chain->multisampleColor.Get(), 0, Formats::kSwapchainColor);
        }
    }

    // The queries close the work of the frame before it is presented, so the time of the GPU leaves out the wait for the display.
    timer->end(*context.Get());

    // A value that the context never signals would hold the frame that waits for it, so the frames count only the values the context took.
    const HRESULT signaled = context->Signal(fence.Get(), fenceValue + 1);
    if (FAILED(signaled)) {
        loseDevice(signaled);
    } else {
        ++fenceValue;
        frameFences[frameIndex] = fenceValue;
    }

    frameIndex = (frameIndex + 1) % kFramesInFlight;
    ++frameNumber;
    frameOpen = false;

    // Every surface presents and the frame ends before a present that failed for another reason than a loss of the device throws.
    HRESULT failedPresent = S_OK;
    for (const std::unique_ptr<SurfaceChain>& chain : chains) {
        if (chain == nullptr || !chain->presentPending) {
            continue;
        }

        chain->presentPending = false;
        const HRESULT result = chain->swapchain->present();
        chain->presented = SUCCEEDED(result);
        if (HResult::isDeviceLoss(result)) {
            loseDevice(result);
        } else if (FAILED(result) && SUCCEEDED(failedPresent)) {
            failedPresent = result;
        }
    }

    const HRESULT removed = device->GetDeviceRemovedReason();
    if (FAILED(removed)) {
        loseDevice(removed);
    }

    drainMessages();
    HResult::check(failedPresent, "present the swapchain of a window");
}

// The age of the input is the one of the window, whose frames the input of the app reaches.
std::optional<double> Device::getInputAge() const noexcept {
    const std::unique_ptr<SurfaceChain>& window = chains.front();
    if (window == nullptr || !window->swapchain) {
        return std::nullopt;
    }

    return window->swapchain->getInputAge();
}

bool Device::paceWindow(int, std::function<void()>) {
    return false;
}

// Every surface that presented waits on its frame latency waitable object, so the platform reads the input of the next frame only once the displays took the frames before it, and the swapchains note that moment for the age of the input.
void Device::waitForDisplay() {
    if (passOpen) {
        throw ContractError(ContractError::Rule::PassOrder);
    }

    for (const std::unique_ptr<SurfaceChain>& chain : chains) {
        if (chain == nullptr || !std::exchange(chain->presented, false) || lost) {
            continue;
        }

        chain->swapchain->waitForFrame();
    }

    for (const std::unique_ptr<SurfaceChain>& chain : chains) {
        if (chain != nullptr && chain->swapchain) {
            chain->swapchain->readStatistics();
            chain->swapchain->markInput();
        }
    }
}

// The driver gives back only what no object of the context holds, so the context lets go of every bound object first, and the next draws bind their state again.
void Device::trim() {
    if (passOpen) {
        throw ContractError(ContractError::Rule::PassOrder);
    }

    if (lost) {
        return;
    }

    context->ClearState();
    bound = {};

    ComPtr<IDXGIDevice3> dxgi;
    HResult::check(device.As(&dxgi), "reach the DXGI device of the GPU");
    dxgi->Trim();
}

void Device::drainMessages() {
    if (messages != nullptr) {
        messages->drain<ID3D11InfoQueue, D3D11_MESSAGE>(*infoQueue.Get(), "Direct3D 11");
    }
}

void Device::resizeSurface(const Surface::Size& size) {
    SurfaceChain& window = *chains.front();
    if (window.surface) {
        window.surface->size = size;
    }
}

void Device::detachSurface() {
    SurfaceChain& window = *chains.front();
    releaseChain(window);
    window.surface.reset();
}

// A window that comes back with another sample count makes the attachments of every surface beyond it again, which the context let go of as the window went.
void Device::attachSurface(const Surface& target) {
    detachSurface();
    chains.front()->surface = target;

    const int samples = std::max(1, target.sampleCount);
    if (samples == swapchainSampleCount) {
        return;
    }

    swapchainSampleCount = samples;
    for (const std::unique_ptr<SurfaceChain>& chain : chains) {
        if (chain != nullptr && chain->swapchain != nullptr) {
            releaseSwapchainTargets(*chain);
            makeSwapchainTargets(*chain);
        }
    }
}

std::unique_ptr<gpu::Device::Readback> Device::readImage(Image image, int width, int height) {
    const ImageRecord& record = images.get(image.id);
    const ImageRules::Description& description = record.description;
    ImageRules::checkReadback(description, width, height);
    if (description.type == TextureType::Volume) {
        throw ContractError(ContractError::Rule::Readback);
    }

    openFrame();

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = Formats::getViewFormat(description.format);
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    HResult::check(device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf()), "find memory to read the pixels of a render target into");

    const D3D11_BOX box{.left = 0, .top = 0, .front = 0, .right = static_cast<UINT>(width), .bottom = static_cast<UINT>(height), .back = 1};
    context->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, record.resource.Get(), 0, &box);
    return std::make_unique<ImageReadback>(StagingCopy(*device.Get(), context, std::move(staging)), description.format, width, height);
}

// The copy follows the commands that the context holds, which hold the frames committed before it.
std::unique_ptr<gpu::Device::Readback> Device::readBuffer(Buffer buffer, std::size_t offset, std::size_t size) {
    if (passOpen) {
        throw ContractError(ContractError::Rule::Readback);
    }

    const BufferRecord& record = buffers.get(buffer.id);
    BufferRules::checkRead(record.description, offset, size);
    openFrame();

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = static_cast<UINT>(size);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Buffer> staging;
    HResult::check(device->CreateBuffer(&desc, nullptr, staging.GetAddressOf()), "find memory to read the bytes of a buffer into");

    const D3D11_BOX box{.left = static_cast<UINT>(offset), .top = 0, .front = 0, .right = static_cast<UINT>(offset + size), .bottom = 1, .back = 1};
    context->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, record.buffer.Get(), 0, &box);
    return std::make_unique<BufferReadback>(StagingCopy(*device.Get(), context, std::move(staging)), size);
}

// A surface beyond the window draws through the pipelines of the swapchain, so its attachments take the samples of the window, and it makes its swapchain now, so a surface that Direct3D 11 cannot draw into fails at once.
int Device::addSurface(const Surface& added) {
    auto chain = std::make_unique<SurfaceChain>(SurfaceChain{.surface = added});
    if (added.size.width > 0 && added.size.height > 0) {
        chain->swapchain = std::make_unique<direct3d::Swapchain>(*factory.Get(), *device.Get(), added);
        makeSwapchainTargets(*chain);
    }

    const auto free = std::find(chains.begin() + 1, chains.end(), nullptr);
    if (free != chains.end()) {
        *free = std::move(chain);
        return static_cast<int>(free - chains.begin());
    }

    chains.push_back(std::move(chain));
    return static_cast<int>(chains.size()) - 1;
}

void Device::resizeSurface(int number, const Surface::Size& size) {
    SurfaceChain& chain = requireAddedChain(number);
    chain.surface->size = size;
}

void Device::removeSurface(int number) {
    SurfaceChain& chain = requireAddedChain(number);
    if (chain.presentPending) {
        throw ContractError(ContractError::Rule::Surface);
    }

    releaseChain(chain);
    chains[static_cast<std::size_t>(number)].reset();
}

gpu::Device::CaptureSupport Device::getCaptureSupport() const {
    if (!renderDoc) {
        return {.tool = CaptureTool::None, .missing = std::string(renderdoc::Capturer::kMissing)};
    }

    return {.tool = CaptureTool::RenderDoc, .missing = {}};
}

void Device::beginCapture(const std::filesystem::path& path) {
    if (passOpen) {
        throw ContractError(ContractError::Rule::Capture);
    }

    if (!renderDoc) {
        throw std::logic_error(std::string(renderdoc::Capturer::kMissing));
    }

    renderDoc->begin(device.Get(), path);
}

std::filesystem::path Device::endCapture() {
    if (!renderDoc) {
        throw ContractError(ContractError::Rule::Capture);
    }

    return renderDoc->end(device.Get());
}

} // namespace haylen::gpu::d3d11
