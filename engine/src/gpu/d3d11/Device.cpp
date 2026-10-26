#include "gpu/d3d11/Device.hpp"

#include <d3d11sdklayers.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <initializer_list>
#include <stdexcept>
#include <utility>

#include "gpu/d3d11/ImageReadback.hpp"
#include "gpu/d3d11/StateObjects.hpp"
#include "gpu/direct3d/Adapter.hpp"
#include "gpu/direct3d/Formats.hpp"
#include "gpu/direct3d/HResult.hpp"
#include "gpu/direct3d/MipLevels.hpp"
#include "gpu/direct3d/ShaderCompiler.hpp"
#include "gpu/direct3d/Swapchain.hpp"
#include "haylen/core/Log.hpp"

namespace haylen::gpu::d3d11 {

using direct3d::Formats;
using direct3d::HResult;
using Microsoft::WRL::ComPtr;

std::unique_ptr<gpu::Device> Device::create(const std::optional<Surface>& target, const DeviceFactory::Options& options) {
    ComPtr<IDXGIFactory6> dxgiFactory = direct3d::Adapter::createFactory(options.validation);
    std::string reasons;
    for (ComPtr<IDXGIAdapter1>& card : direct3d::Adapter::list(*dxgiFactory.Get())) {
        const std::string name = direct3d::Adapter::getName(*card.Get());
        try {
            return std::make_unique<Device>(dxgiFactory, std::move(card), target, options.validation);
        } catch (const std::runtime_error& error) {
            reasons += std::format(" On \"{}\": {}", name, error.what());
        }
    }
    throw std::runtime_error(std::format("Direct3D 11 runs on no GPU of this machine.{}", reasons.empty() ? " Windows lists no GPU." : reasons));
}

Device::Device(ComPtr<IDXGIFactory6> dxgiFactory, ComPtr<IDXGIAdapter1> card, const std::optional<Surface>& target, bool validation) : factory(std::move(dxgiFactory)), adapter(std::move(card)), surface(target) {
    createDevice(validation);
    readCapabilities();
    compiler = std::make_shared<direct3d::ShaderCompiler>(validation);
    uniforms = std::make_unique<DynamicRing>(*device.Get(), kUniformRingSize, D3D11_BIND_CONSTANT_BUFFER, kUniformAlignment, "haylen-uniform-ring");
    timer = std::make_unique<FrameTimer>(*device.Get());

    const D3D11_QUERY_DESC event{.Query = D3D11_QUERY_EVENT, .MiscFlags = 0};
    for (ComPtr<ID3D11Query>& query : frameFinished) {
        HResult::check(device->CreateQuery(&event, query.GetAddressOf()), "create the query that marks the end of a frame");
    }

    // A window that has a drawable takes its swapchain now, so a window that Direct3D 11 cannot draw into fails the device while the platform can still try another API.
    if (surface && surface->width > 0 && surface->height > 0) {
        swapchain = std::make_unique<direct3d::Swapchain>(*factory.Get(), *device.Get(), *surface);
        makeSwapchainTargets();
    }
}

Device::~Device() {
    if (context != nullptr) {
        context->ClearState();
        context->Flush();
    }
    releaseSwapchainTargets();
    swapchain.reset();
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
    HResult::check(made.As(&device), "reach the runtime of Direct3D 11.1");
    HResult::check(immediate.As(&context), "reach the context of Direct3D 11.1");

    D3D11_FEATURE_DATA_D3D11_OPTIONS options{};
    device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &options, sizeof(options));
    if (options.ConstantBufferOffsetting == FALSE || options.DiscardAPIsSeenByDriver == FALSE) {
        throw std::runtime_error("The driver of the GPU lacks the constant buffer offsets of Direct3D 11.1.");
    }

    static_cast<void>(context.As(&annotation));
    ComPtr<IDXGIDevice1> dxgiDevice;
    if (SUCCEEDED(device.As(&dxgiDevice))) {
        dxgiDevice->SetMaximumFrameLatency(static_cast<UINT>(kFramesInFlight));
    }
    if (validation && SUCCEEDED(device.As(&infoQueue))) {
        messages = std::make_unique<direct3d::DebugMessages>();
    }
    adapterName = direct3d::Adapter::getName(*adapter.Get());
}

void Device::readCapabilities() {
    capabilities.maxImageSize = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    capabilities.maxArrayLayers = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
    capabilities.originTopLeft = true;
    capabilities.independentBlend = true;
    capabilities.multisampledRenderTargets = true;
    capabilities.gpuTiming = true;

    // The swapchain and the render targets of every color format share the sample counts of the formats that draws use most.
    for (const UINT count : {8U, 4U, 2U}) {
        UINT color = 0;
        UINT depth = 0;
        device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, count, &color);
        device->CheckMultisampleQualityLevels(DXGI_FORMAT_D24_UNORM_S8_UINT, count, &depth);
        if (color > 0 && depth > 0) {
            capabilities.maxSampleCount = static_cast<int>(count);
            break;
        }
    }

    // Images sample through the view of their format, and depth images draw through the view of their depth.
    for (std::size_t index = 0; index < Formats::kPixelFormatCount; ++index) {
        const auto format = static_cast<PixelFormat>(index);
        const bool depth = Formats::isDepth(format);
        UINT view = 0;
        UINT target = 0;
        static_cast<void>(device->CheckFormatSupport(Formats::getViewFormat(format), &view));
        static_cast<void>(device->CheckFormatSupport(depth ? Formats::getDepthFormat(format) : Formats::getViewFormat(format), &target));
        capabilities.textureFormats[index] = (view & static_cast<UINT>(D3D11_FORMAT_SUPPORT_SHADER_SAMPLE | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE_COMPARISON)) != 0;
        capabilities.renderFormats[index] = (target & static_cast<UINT>(depth ? D3D11_FORMAT_SUPPORT_DEPTH_STENCIL : D3D11_FORMAT_SUPPORT_RENDER_TARGET)) != 0;
        capabilities.blendableFormats[index] = !depth && capabilities.renderFormats[index] && (target & static_cast<UINT>(D3D11_FORMAT_SUPPORT_BLENDABLE)) != 0;
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

std::vector<gpu::Device::Pool> Device::getPools() const {
    const auto pool = [](std::string_view name, std::size_t used, std::size_t size) { return Pool{.name = name, .used = static_cast<int>(used), .size = static_cast<int>(size)}; };
    return {pool("images", images.size(), images.capacity()), pool("buffers", buffers.size(), buffers.capacity()), pool("samplers", samplers.size(), samplers.capacity()), pool("programs", programs.size(), programs.capacity()), pool("pipelines", pipelines.size(), pipelines.capacity())};
}

bool Device::isLost() const noexcept {
    return lost;
}

void Device::loseDevice(HRESULT reason) noexcept {
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

gpu::Device::Buffer Device::makeBuffer(const BufferDesc& desc) {
    const std::size_t size = desc.data.empty() ? desc.size : desc.data.size();
    if (size == 0) {
        throw std::logic_error(std::format("The buffer \"{}\" has no size.", desc.label));
    }
    if (desc.usage == Usage::Immutable && desc.data.empty()) {
        throw std::logic_error(std::format("The immutable buffer \"{}\" has no data.", desc.label));
    }

    D3D11_BUFFER_DESC made{};
    made.ByteWidth = static_cast<UINT>(size);
    made.Usage = desc.usage == Usage::Immutable ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DYNAMIC;
    made.BindFlags = desc.index ? D3D11_BIND_INDEX_BUFFER : D3D11_BIND_VERTEX_BUFFER;
    made.CPUAccessFlags = desc.usage == Usage::Dynamic ? D3D11_CPU_ACCESS_WRITE : 0U;
    const D3D11_SUBRESOURCE_DATA initial{.pSysMem = desc.data.data(), .SysMemPitch = 0, .SysMemSlicePitch = 0};

    BufferRecord record{.size = size, .usage = desc.usage};
    HResult::check(device->CreateBuffer(&made, desc.data.empty() ? nullptr : &initial, record.buffer.GetAddressOf()), std::format("create a buffer of {} bytes", size));
    setName(*record.buffer.Get(), desc.label);
    return {buffers.insert(std::move(record))};
}

// Direct3D orders the subresources of a texture by layer and then by mip level, and those of a volume by mip level alone.
std::vector<D3D11_SUBRESOURCE_DATA> Device::describeLevels(const ImageDesc& desc) {
    direct3d::MipLevels::check(desc);
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

ComPtr<ID3D11ShaderResourceView> Device::makeTextureView(ID3D11Resource& resource, const ImageDesc& desc) {
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = Formats::getViewFormat(desc.format);
    const auto mips = static_cast<UINT>(desc.mipLevels);
    const auto layers = static_cast<UINT>(desc.layers);
    const bool multisampled = desc.sampleCount > 1;

    switch (desc.type) {
    case TextureType::Flat:
        view.ViewDimension = multisampled ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
        view.Texture2D = {.MostDetailedMip = 0, .MipLevels = mips};
        break;
    case TextureType::Array:
        view.ViewDimension = multisampled ? D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        if (multisampled) {
            view.Texture2DMSArray = {.FirstArraySlice = 0, .ArraySize = layers};
        } else {
            view.Texture2DArray = {.MostDetailedMip = 0, .MipLevels = mips, .FirstArraySlice = 0, .ArraySize = layers};
        }
        break;
    case TextureType::Cube:
        view.ViewDimension = layers > 6 ? D3D11_SRV_DIMENSION_TEXTURECUBEARRAY : D3D11_SRV_DIMENSION_TEXTURECUBE;
        if (layers > 6) {
            view.TextureCubeArray = {.MostDetailedMip = 0, .MipLevels = mips, .First2DArrayFace = 0, .NumCubes = layers / 6};
        } else {
            view.TextureCube = {.MostDetailedMip = 0, .MipLevels = mips};
        }
        break;
    case TextureType::Volume:
        view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
        view.Texture3D = {.MostDetailedMip = 0, .MipLevels = mips};
        break;
    }

    ComPtr<ID3D11ShaderResourceView> made;
    HResult::check(device->CreateShaderResourceView(&resource, &view, made.GetAddressOf()), std::format("create the view of the image \"{}\"", desc.label));
    return made;
}

gpu::Device::Image Device::makeImage(const ImageDesc& desc) {
    if (desc.width <= 0 || desc.height <= 0 || desc.width > capabilities.maxImageSize || desc.height > capabilities.maxImageSize) {
        throw std::logic_error(std::format("The image \"{}\" of {}x{} pixels does not fit the device.", desc.label, desc.width, desc.height));
    }
    const bool depth = Formats::isDepth(desc.format);
    if (depth != (desc.kind == ImageKind::DepthStencil)) {
        throw std::logic_error(std::format("The image \"{}\" needs a depth format exactly when it is a depth stencil image.", desc.label));
    }
    if (desc.kind != ImageKind::Texture && !desc.levels.empty()) {
        throw std::logic_error(std::format("The image \"{}\" starts without pixels, since only a texture is made with them.", desc.label));
    }

    UINT bind = D3D11_BIND_SHADER_RESOURCE;
    bind |= desc.kind == ImageKind::RenderTarget ? D3D11_BIND_RENDER_TARGET : 0U;
    bind |= desc.kind == ImageKind::DepthStencil ? D3D11_BIND_DEPTH_STENCIL : 0U;
    const D3D11_USAGE usage = desc.kind == ImageKind::Texture ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DEFAULT;
    const std::vector<D3D11_SUBRESOURCE_DATA> data = desc.kind == ImageKind::Texture ? describeLevels(desc) : std::vector<D3D11_SUBRESOURCE_DATA>{};
    const D3D11_SUBRESOURCE_DATA* initial = data.empty() ? nullptr : data.data();

    ImageRecord record{.type = desc.type, .width = desc.width, .height = desc.height, .layers = desc.layers, .mipLevels = desc.mipLevels, .sampleCount = desc.sampleCount, .format = desc.format, .kind = desc.kind};
    if (desc.type == TextureType::Volume) {
        D3D11_TEXTURE3D_DESC made{};
        made.Width = static_cast<UINT>(desc.width);
        made.Height = static_cast<UINT>(desc.height);
        made.Depth = static_cast<UINT>(desc.layers);
        made.MipLevels = static_cast<UINT>(desc.mipLevels);
        made.Format = Formats::getResourceFormat(desc.format);
        made.Usage = usage;
        made.BindFlags = bind;
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
        made.MiscFlags = desc.type == TextureType::Cube ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0U;
        ComPtr<ID3D11Texture2D> texture;
        HResult::check(device->CreateTexture2D(&made, initial, texture.GetAddressOf()), std::format("create the {}x{} image \"{}\"", desc.width, desc.height, desc.label));
        record.resource = texture;
    }

    ComPtr<ID3D11DeviceChild> child;
    record.resource.As(&child);
    setName(*child.Get(), desc.label);
    record.view = makeTextureView(*record.resource.Get(), desc);
    return {images.insert(std::move(record))};
}

gpu::Device::Sampler Device::makeSampler(const SamplerDesc& desc) {
    ComPtr<ID3D11SamplerState> sampler = StateObjects::makeSampler(*device.Get(), desc);
    setName(*sampler.Get(), desc.label);
    return {samplers.insert(std::move(sampler))};
}

std::vector<Device::Register> Device::readRegisters(std::span<const TextureBinding> textures) {
    std::vector<Register> registers;
    for (const TextureBinding& texture : textures) {
        if (texture.slot < 0 || texture.slot >= static_cast<int>(kMaxTextures) || texture.binding < 0 || texture.binding >= static_cast<int>(kMaxTextures)) {
            throw std::logic_error(std::format("A texture at slot {} binds where the device cannot bind it.", texture.slot));
        }
        registers.push_back({.slot = texture.slot, .stage = texture.stage == Stage::Vertex ? 0U : 1U, .number = static_cast<UINT>(texture.binding)});
    }
    return registers;
}

std::vector<Device::Register> Device::readRegisters(std::span<const SamplerBinding> samplersOfProgram) {
    std::vector<Register> registers;
    for (const SamplerBinding& sampler : samplersOfProgram) {
        if (sampler.slot < 0 || sampler.slot >= static_cast<int>(kMaxSamplers) || sampler.binding < 0 || sampler.binding >= static_cast<int>(kMaxSamplers)) {
            throw std::logic_error(std::format("A sampler at slot {} binds where the device cannot bind it.", sampler.slot));
        }
        registers.push_back({.slot = sampler.slot, .stage = sampler.stage == Stage::Vertex ? 0U : 1U, .number = static_cast<UINT>(sampler.binding)});
    }
    return registers;
}

gpu::Device::Program Device::makeProgram(const ProgramDesc& desc) {
    ProgramRecord record;
    for (const BlockBinding& block : desc.blocks) {
        if (block.slot < 0 || block.slot >= static_cast<int>(kMaxBlocks) || block.size == 0 || block.binding < 0 || block.binding >= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT) {
            throw std::logic_error(std::format("The program \"{}\" has a uniform block at slot {} that the device cannot bind.", desc.label, block.slot));
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
    return {programs.insert(std::move(record))};
}

void Device::prepareProgram(const ProgramDesc& desc, core::JobSystem& jobs) {
    compiler->prepare(desc, jobs);
}

gpu::Device::Pipeline Device::makePipeline(const PipelineDesc& desc) {
    const ProgramRecord& program = programs.get(desc.program.id);
    if (desc.colors.empty() || desc.colors.size() > kMaxColorTargets || desc.buffers.size() > kMaxVertexBuffers) {
        throw std::logic_error(std::format("The pipeline \"{}\" needs one to {} color targets and at most {} vertex buffers.", desc.label, kMaxColorTargets, kMaxVertexBuffers));
    }

    std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
    for (const VertexAttribute& attribute : desc.attributes) {
        if (attribute.buffer < 0 || attribute.buffer >= static_cast<int>(desc.buffers.size())) {
            throw std::logic_error(std::format("An attribute of the pipeline \"{}\" reads a vertex buffer it does not lay out.", desc.label));
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
    return {pipelines.insert(std::move(record))};
}

void Device::destroy(Buffer buffer) noexcept {
    buffers.erase(buffer.id);
}

void Device::destroy(Image image) noexcept {
    images.erase(image.id);
}

void Device::destroy(Sampler sampler) noexcept {
    samplers.erase(sampler.id);
}

void Device::destroy(Program program) noexcept {
    programs.erase(program.id);
}

void Device::destroy(Pipeline pipeline) noexcept {
    pipelines.erase(pipeline.id);
    if (bound.pipeline == pipeline.id) {
        bound.pipeline = 0;
    }
}

// A dynamic buffer holds in a frame only what that frame wrote, so the first write of a frame discards the buffer, which makes the driver hand over a fresh copy while the GPU still reads the old one, and the later writes of the frame fill other parts of the same copy.
void Device::writeBuffer(Buffer buffer, std::size_t offset, std::span<const std::byte> data) {
    BufferRecord& record = buffers.get(buffer.id);
    if (record.usage != Usage::Dynamic) {
        throw std::logic_error("Only a dynamic buffer takes new contents.");
    }
    if (offset + data.size() > record.size) {
        throw std::logic_error(std::format("The buffer of {} bytes cannot take {} bytes at {}.", record.size, data.size(), offset));
    }
    if (data.empty()) {
        return;
    }
    openFrame();

    const D3D11_MAP mode = record.writtenFrame == frameNumber ? D3D11_MAP_WRITE_NO_OVERWRITE : D3D11_MAP_WRITE_DISCARD;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    const HRESULT result = context->Map(record.buffer.Get(), 0, mode, 0, &mapped);
    if (HResult::isDeviceLoss(result)) {
        loseDevice(result);
        return;
    }
    HResult::check(result, "map a dynamic buffer");
    std::memcpy(static_cast<std::byte*>(mapped.pData) + offset, data.data(), data.size());
    context->Unmap(record.buffer.Get(), 0);
    record.writtenFrame = frameNumber;
}

// Direct3D 11 copies no buffer into a texture, so the driver stages the pixels, and replacing a whole image of one subresource lets it hand the GPU a fresh copy instead of waiting for the frames that read the old one.
void Device::writeImages(std::span<const ImageWrite> writes) {
    openFrame();
    for (const ImageWrite& write : writes) {
        const ImageRecord& image = images.get(write.image.id);
        if (image.kind != ImageKind::Dynamic) {
            throw std::logic_error("Only a dynamic image takes new pixels.");
        }
        const std::size_t pixel = Formats::getBytesPerPixel(image.format);
        const std::size_t row = static_cast<std::size_t>(image.width) * pixel;
        const std::size_t slice = row * static_cast<std::size_t>(image.height);
        if (write.pixels.size() != slice * static_cast<std::size_t>(image.layers)) {
            throw std::logic_error(std::format("The pixels do not fill the image of {}x{} with {} layers.", image.width, image.height, image.layers));
        }

        const UINT flags = image.layers == 1 && image.mipLevels == 1 ? D3D11_COPY_DISCARD : 0U;
        if (image.type == TextureType::Volume) {
            context->UpdateSubresource1(image.resource.Get(), 0, nullptr, write.pixels.data(), static_cast<UINT>(row), static_cast<UINT>(slice), flags);
            continue;
        }
        for (int layer = 0; layer < image.layers; ++layer) {
            const UINT subresource = D3D11CalcSubresource(0, static_cast<UINT>(layer), static_cast<UINT>(image.mipLevels));
            context->UpdateSubresource1(image.resource.Get(), subresource, nullptr, write.pixels.data() + static_cast<std::size_t>(layer) * slice, static_cast<UINT>(row), static_cast<UINT>(slice), flags);
        }
    }
}

// The frame waits for the GPU to finish the frame that used the same slot, so the GPU runs at most `kFramesInFlight` frames behind. A lost device answers every query at once.
void Device::openFrame() {
    if (frameOpen) {
        return;
    }
    frameOpen = true;
    if (frameSubmitted[frameIndex]) {
        while (context->GetData(frameFinished[frameIndex].Get(), nullptr, 0, 0) == S_FALSE) {
            SwitchToThread();
        }
    }
    timer->begin(*context.Get());
}

bool Device::prepareSwapchain() {
    if (!surface || surface->width <= 0 || surface->height <= 0 || lost) {
        return false;
    }
    if (swapchain == nullptr) {
        swapchain = std::make_unique<direct3d::Swapchain>(*factory.Get(), *device.Get(), *surface);
        makeSwapchainTargets();
        return true;
    }
    // A new size applies from the first pass of a frame into the swapchain, so the passes of one frame draw into the same buffers.
    if (presentPending || (swapchain->getWidth() == surface->width && swapchain->getHeight() == surface->height)) {
        return true;
    }

    // The buffers resize only once nothing refers to them, the views of the device and the bindings of the context.
    releaseSwapchainTargets();
    context->OMSetRenderTargets(0, nullptr, nullptr);
    context->Flush();
    const HRESULT result = swapchain->resize(surface->width, surface->height);
    if (HResult::isDeviceLoss(result)) {
        loseDevice(result);
        return false;
    }
    HResult::check(result, "resize the swapchain");
    makeSwapchainTargets();
    return true;
}

void Device::makeSwapchainTargets() {
    ComPtr<ID3D11Texture2D> backBuffer;
    HResult::check(swapchain->get().GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf())), "reach the buffer of the swapchain");
    HResult::check(device->CreateRenderTargetView(backBuffer.Get(), nullptr, backBufferView.GetAddressOf()), "create the view of the swapchain");

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(swapchain->getWidth());
    desc.Height = static_cast<UINT>(swapchain->getHeight());
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = static_cast<UINT>(std::max(1, surface->sampleCount));
    desc.Usage = D3D11_USAGE_DEFAULT;

    // Flip model swapchains hold one sample, so a multisampled window draws into a texture of its own that resolves into the swapchain when the frame is presented.
    if (desc.SampleDesc.Count > 1) {
        desc.Format = Formats::kSwapchainColor;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        HResult::check(device->CreateTexture2D(&desc, nullptr, multisampleColor.GetAddressOf()), "create the multisampled color of the swapchain");
        HResult::check(device->CreateRenderTargetView(multisampleColor.Get(), nullptr, multisampleView.GetAddressOf()), "create the view of the multisampled color of the swapchain");
        setName(*multisampleColor.Get(), "haylen-swapchain-color");
    }

    desc.Format = Formats::getDepthFormat(Formats::kSwapchainDepthFormat);
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> depth;
    HResult::check(device->CreateTexture2D(&desc, nullptr, depth.GetAddressOf()), "create the depth and stencil of the swapchain");
    HResult::check(device->CreateDepthStencilView(depth.Get(), nullptr, swapchainDepthView.GetAddressOf()), "create the view of the depth and stencil of the swapchain");
    setName(*depth.Get(), "haylen-swapchain-depth");
}

void Device::releaseSwapchainTargets() {
    backBufferView.Reset();
    multisampleView.Reset();
    multisampleColor.Reset();
    swapchainDepthView.Reset();
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

void Device::beginPass(const PassDesc& desc) {
    if (passOpen) {
        throw std::logic_error("A pass began while another pass was open.");
    }
    openFrame();
    passOpen = true;
    pass = {};
    if (annotation != nullptr && !desc.label.empty()) {
        annotation->BeginEvent(markerText.convert(desc.label));
        pass.labeled = true;
    }
    unbindTextures();

    if (desc.colors.empty() && !desc.depthStencil) {
        beginSwapchainPass(desc);
    } else {
        beginOffscreenPass(desc);
    }
}

void Device::beginSwapchainPass(const PassDesc& desc) {
    skipping = !prepareSwapchain();
    if (skipping) {
        return;
    }
    presentPending = true;
    pass.colors[0] = multisampleView != nullptr ? multisampleView.Get() : backBufferView.Get();
    pass.colorCount = 1;
    pass.depthStencil = swapchainDepthView.Get();
    context->OMSetRenderTargets(1, pass.colors.data(), pass.depthStencil);

    if (desc.load == Load::Clear) {
        const std::array<float, 4> clear{desc.clear.r, desc.clear.g, desc.clear.b, desc.clear.a};
        context->ClearRenderTargetView(pass.colors[0], clear.data());
        context->ClearDepthStencilView(pass.depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, desc.clearDepth, desc.clearStencil);
    } else if (desc.load == Load::DontCare) {
        context->DiscardView(pass.colors[0]);
        context->DiscardView(pass.depthStencil);
    }
    setViewportAndScissor(swapchain->getWidth(), swapchain->getHeight());
}

void Device::beginOffscreenPass(const PassDesc& desc) {
    if (desc.colors.size() > kMaxColorTargets) {
        throw std::logic_error(std::format("A pass draws into at most {} color attachments.", kMaxColorTargets));
    }
    int width = 0;
    int height = 0;

    for (std::size_t index = 0; index < desc.colors.size(); ++index) {
        const ColorAttachment& color = desc.colors[index];
        ImageRecord& image = images.get(color.image.id);
        if (image.kind != ImageKind::RenderTarget) {
            throw std::logic_error("A pass draws only into render targets.");
        }
        ID3D11RenderTargetView* view = getAttachment(image, color.mipLevel, color.layer).color.Get();
        pass.colors[index] = view;
        pass.discardColors[index] = color.store == Store::Discard;
        width = std::max(1, image.width >> color.mipLevel);
        height = std::max(1, image.height >> color.mipLevel);

        if (color.load == Load::Clear) {
            const std::array<float, 4> clear{color.clear.r, color.clear.g, color.clear.b, color.clear.a};
            context->ClearRenderTargetView(view, clear.data());
        } else if (color.load == Load::DontCare) {
            context->DiscardView(view);
        }
        if (color.resolve.isValid()) {
            const ImageRecord& resolve = images.get(color.resolve.id);
            pass.resolves[index] = {.source = image.resource.Get(), .sourceSubresource = static_cast<UINT>(color.layer), .destination = resolve.resource.Get(), .format = Formats::getViewFormat(image.format)};
        }
    }
    pass.colorCount = static_cast<UINT>(desc.colors.size());

    if (desc.depthStencil) {
        const DepthStencilAttachment& attachment = *desc.depthStencil;
        ImageRecord& image = images.get(attachment.image.id);
        if (image.kind != ImageKind::DepthStencil) {
            throw std::logic_error("A pass takes only a depth stencil image as its depth and stencil attachment.");
        }
        pass.depthStencil = getAttachment(image, 0, 0).depth.Get();
        const bool stencil = Formats::hasStencil(image.format);
        pass.discardDepthStencil = attachment.depthStore == Store::Discard && (!stencil || attachment.stencilStore == Store::Discard);
        width = width > 0 ? width : image.width;
        height = height > 0 ? height : image.height;

        UINT clear = attachment.depthLoad == Load::Clear ? D3D11_CLEAR_DEPTH : 0U;
        clear |= stencil && attachment.stencilLoad == Load::Clear ? D3D11_CLEAR_STENCIL : 0U;
        if (clear != 0) {
            context->ClearDepthStencilView(pass.depthStencil, clear, attachment.clearDepth, attachment.clearStencil);
        } else if (attachment.depthLoad == Load::DontCare && (!stencil || attachment.stencilLoad == Load::DontCare)) {
            context->DiscardView(pass.depthStencil);
        }
    }

    context->OMSetRenderTargets(pass.colorCount, pass.colors.data(), pass.depthStencil);
    setViewportAndScissor(width, height);
}

Device::Attachment& Device::getAttachment(ImageRecord& image, int mipLevel, int layer) {
    for (Attachment& attachment : image.attachments) {
        if (attachment.mipLevel == mipLevel && attachment.layer == layer) {
            return attachment;
        }
    }

    Attachment& attachment = image.attachments.emplace_back(Attachment{.mipLevel = mipLevel, .layer = layer});
    const auto mip = static_cast<UINT>(mipLevel);
    const auto slice = static_cast<UINT>(layer);
    const bool multisampled = image.sampleCount > 1;

    if (image.kind == ImageKind::DepthStencil) {
        D3D11_DEPTH_STENCIL_VIEW_DESC view{};
        view.Format = Formats::getDepthFormat(image.format);
        if (image.type == TextureType::Flat) {
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
        return attachment;
    }

    D3D11_RENDER_TARGET_VIEW_DESC view{};
    view.Format = Formats::getViewFormat(image.format);
    switch (image.type) {
    case TextureType::Flat:
        view.ViewDimension = multisampled ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
        view.Texture2D.MipSlice = mip;
        break;
    case TextureType::Array:
    case TextureType::Cube:
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
    return attachment;
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
        vertexBuffers[index] = buffers.get(bindings.vertexBuffers[index].id).buffer.Get();
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
        ID3D11Buffer* index = buffers.get(bindings.indexBuffer.id).buffer.Get();
        const auto offset = static_cast<UINT>(bindings.indexOffset);
        if (index != bound.indexBuffer || offset != bound.indexOffset || pipeline.indexFormat != bound.indexFormat) {
            context->IASetIndexBuffer(index, pipeline.indexFormat, offset);
            bound.indexBuffer = index;
            bound.indexOffset = offset;
            bound.indexFormat = pipeline.indexFormat;
        }
    }

    applyTextures(pipeline, bindings);
    applySamplers(pipeline, bindings);
}

void Device::applyTextures(const PipelineRecord& pipeline, const Bindings& bindings) {
    for (const Register& texture : pipeline.textures) {
        ID3D11ShaderResourceView* view = images.get(bindings.textures[static_cast<std::size_t>(texture.slot)].id).view.Get();
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

// Each draw reads its uniforms from its own window of the ring, which Direct3D 11.1 binds by its offset in constants of 16 bytes, a multiple of 16 constants.
void Device::applyUniforms(int slot, std::span<const std::byte> data) {
    if (skipping) {
        return;
    }
    const PipelineRecord& pipeline = pipelines.get(bound.pipeline);
    const bool known = slot >= 0 && slot < static_cast<int>(kMaxBlocks) && pipeline.blocks[static_cast<std::size_t>(slot)].has_value();
    if (!known || data.size() != pipeline.blocks[static_cast<std::size_t>(slot)]->size) {
        throw std::logic_error(std::format("The program has no uniform block of {} bytes at slot {}.", data.size(), slot));
    }
    const Register& block = *pipeline.blocks[static_cast<std::size_t>(slot)];

    const std::optional<UINT> offset = uniforms->write(*context.Get(), data);
    if (!offset) {
        loseDevice(DXGI_ERROR_DEVICE_REMOVED);
        return;
    }
    ID3D11Buffer* buffer = uniforms->get();
    const UINT first = *offset / 16;
    const UINT count = (block.size + kUniformAlignment - 1) / kUniformAlignment * (kUniformAlignment / 16);
    if (block.stage == 0) {
        context->VSSetConstantBuffers1(block.number, 1, &buffer, &first, &count);
    } else {
        context->PSSetConstantBuffers1(block.number, 1, &buffer, &first, &count);
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
        throw std::logic_error("A pass ended while no pass was open.");
    }
    if (!skipping) {
        for (UINT index = 0; index < pass.colorCount; ++index) {
            const PassState::Resolve& resolve = pass.resolves[index];
            if (resolve.source != nullptr) {
                context->ResolveSubresource(resolve.destination, 0, resolve.source, resolve.sourceSubresource, resolve.format);
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
        throw std::logic_error("The frame was committed while a pass was open.");
    }
    openFrame();

    const bool presenting = presentPending && swapchain != nullptr;
    if (presenting && multisampleColor != nullptr) {
        ComPtr<ID3D11Resource> backBuffer;
        backBufferView->GetResource(backBuffer.GetAddressOf());
        context->ResolveSubresource(backBuffer.Get(), 0, multisampleColor.Get(), 0, Formats::kSwapchainColor);
    }

    // The queries close the work of the frame before it is presented, so the time of the GPU leaves out the wait for the display.
    timer->end(*context.Get());
    context->End(frameFinished[frameIndex].Get());
    frameSubmitted[frameIndex] = true;
    frameIndex = (frameIndex + 1) % kFramesInFlight;
    ++frameNumber;
    frameOpen = false;
    presentPending = false;

    if (presenting) {
        const HRESULT result = swapchain->present();
        if (HResult::isDeviceLoss(result)) {
            loseDevice(result);
        }
    }

    if (!lost && FAILED(device->GetDeviceRemovedReason())) {
        loseDevice(device->GetDeviceRemovedReason());
    }
    drainMessages();
}

void Device::drainMessages() {
    if (messages != nullptr) {
        messages->drain<ID3D11InfoQueue, D3D11_MESSAGE>(*infoQueue.Get(), "Direct3D 11");
    }
}

void Device::resizeSurface(int width, int height) {
    if (surface) {
        surface->width = width;
        surface->height = height;
    }
}

void Device::detachSurface() {
    context->OMSetRenderTargets(0, nullptr, nullptr);
    releaseSwapchainTargets();
    swapchain.reset();
    context->Flush();
    surface.reset();
}

void Device::attachSurface(const Surface& target) {
    detachSurface();
    surface = target;
}

std::unique_ptr<gpu::Device::Readback> Device::readImage(Image image, int width, int height) {
    const ImageRecord& record = images.get(image.id);
    if (record.kind != ImageKind::RenderTarget || record.sampleCount > 1 || record.type == TextureType::Volume) {
        throw std::logic_error("Only a render target of one sample can be read.");
    }
    openFrame();

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = Formats::getViewFormat(record.format);
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    HResult::check(device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf()), "find memory to read the pixels of a render target into");

    const D3D11_QUERY_DESC event{.Query = D3D11_QUERY_EVENT, .MiscFlags = 0};
    ComPtr<ID3D11Query> finished;
    HResult::check(device->CreateQuery(&event, finished.GetAddressOf()), "create the query of a readback");

    const D3D11_BOX box{.left = 0, .top = 0, .front = 0, .right = static_cast<UINT>(width), .bottom = static_cast<UINT>(height), .back = 1};
    context->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, record.resource.Get(), 0, &box);
    context->End(finished.Get());
    return std::make_unique<ImageReadback>(context, std::move(staging), std::move(finished), record.format, width, height);
}

} // namespace haylen::gpu::d3d11
