#include "gpu/d3d12/Device.hpp"

#include <d3d12sdklayers.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <initializer_list>
#include <stdexcept>
#include <utility>

#include "gpu/d3d12/ImageReadback.hpp"
#include "gpu/d3d12/StateDescriptions.hpp"
#include "gpu/direct3d/Adapter.hpp"
#include "gpu/direct3d/Formats.hpp"
#include "gpu/direct3d/HResult.hpp"
#include "gpu/direct3d/MipLevels.hpp"
#include "gpu/direct3d/ShaderCompiler.hpp"
#include "gpu/direct3d/WideText.hpp"
#include "haylen/core/Log.hpp"

namespace haylen::gpu::d3d12 {

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
    throw std::runtime_error(std::format("Direct3D 12 runs on no GPU of this machine.{}", reasons.empty() ? " Windows lists no GPU." : reasons));
}

// The debug layer and the breadcrumbs of removed devices apply to the devices made after them in the process. Only the Graphics Tools of Windows bring the debug layer, so a machine without them runs the device unvalidated and says so.
void Device::enableDebugLayers(bool validation) {
    if (!validation) {
        return;
    }
    ComPtr<ID3D12Debug> debug;
    const HRESULT result = D3D12GetDebugInterface(IID_PPV_ARGS(debug.GetAddressOf()));
    if (SUCCEEDED(result)) {
        debug->EnableDebugLayer();
    } else {
        core::Log::warning("Direct3D 12 runs without its debug layer, because {}. Add the optional feature \"Graphics Tools\" of Windows to validate it.", HResult::describe(result));
    }

    ComPtr<ID3D12DeviceRemovedExtendedDataSettings> settings;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(settings.GetAddressOf())))) {
        settings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        settings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    }
}

Device::Device(ComPtr<IDXGIFactory6> dxgiFactory, ComPtr<IDXGIAdapter1> card, const std::optional<Surface>& target, bool validation) : factory(std::move(dxgiFactory)), adapter(std::move(card)), dred(validation), surface(target) {
    createDevice(validation);
    readCapabilities();
    compiler = std::make_shared<direct3d::ShaderCompiler>(validation);

    textureViews = std::make_unique<DescriptorHeap>(*device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024);
    samplerViews = std::make_unique<DescriptorHeap>(*device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 256);
    targetViews = std::make_unique<DescriptorHeap>(*device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 256);
    depthViews = std::make_unique<DescriptorHeap>(*device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 64);
    textureTables = std::make_unique<DescriptorRing>(*device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kTextureHeapSize, D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1, "haylen-texture-tables");
    samplerTables = std::make_unique<DescriptorRing>(*device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE, D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE, "haylen-sampler-tables");
    uploads = std::make_unique<UploadAllocator>(*device.Get());
    rootSignatures = std::make_unique<RootSignatures>(*device.Get());
    timer = std::make_unique<FrameTimer>(*device.Get(), *queue.Get(), kFramesInFlight);
    for (D3D12_CPU_DESCRIPTOR_HANDLE& view : backBufferViews) {
        view = targetViews->allocate();
    }
    multisampleView = targetViews->allocate();
    swapchainDepthView = depthViews->allocate();
    pipelineSampleCount = getSwapchainSampleCount();

    // A window that has a drawable takes its swapchain now, so a window that Direct3D 12 cannot draw into fails the device while the platform can still try another API.
    if (surface && surface->width > 0 && surface->height > 0) {
        swapchain = std::make_unique<direct3d::Swapchain>(*factory.Get(), *queue.Get(), *surface);
        makeSwapchainTargets();
    }
}

Device::~Device() {
    if (queue != nullptr && idleFence != nullptr && fenceEvent != nullptr) {
        waitIdle();
    }
    releaseSwapchainTargets();
    swapchain.reset();
    if (fenceEvent != nullptr) {
        CloseHandle(fenceEvent);
    }
}

void Device::createDevice(bool validation) {
    enableDebugLayers(validation);
    HResult::check(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.GetAddressOf())), "start Direct3D 12 at feature level 11_0");
    adapterName = direct3d::Adapter::getName(*adapter.Get());

    D3D12_FEATURE_DATA_ROOT_SIGNATURE signature{.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1};
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &signature, sizeof(signature))) || signature.HighestVersion < D3D_ROOT_SIGNATURE_VERSION_1_1) {
        throw std::runtime_error("The runtime of Direct3D 12 lacks the root signatures of version 1.1.");
    }

    const D3D12_COMMAND_QUEUE_DESC queueDesc{.Type = D3D12_COMMAND_LIST_TYPE_DIRECT, .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL, .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE, .NodeMask = 0};
    HResult::check(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(queue.GetAddressOf())), "create the command queue");
    queue->SetName(L"haylen-queue");
    HResult::check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())), "create the fence of the frames");
    HResult::check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(idleFence.GetAddressOf())), "create the fence that waits for the GPU to finish");
    fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (fenceEvent == nullptr) {
        throw std::runtime_error("The graphics device could not create the event that waits for the GPU.");
    }

    for (Frame& frame : frames) {
        HResult::check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(frame.commands.GetAddressOf())), "create the command memory of a frame");
        HResult::check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(frame.transfers.GetAddressOf())), "create the transfer memory of a frame");
    }

    // Both lists start closed, since each frame resets them when it first records into them.
    ComPtr<ID3D12GraphicsCommandList> list;
    HResult::check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frames[0].commands.Get(), nullptr, IID_PPV_ARGS(list.GetAddressOf())), "create the command list of the frames");
    HResult::check(list.As(&commands), "reach the render passes of Direct3D 12, which need Windows 10 version 1809 or later");
    commands->Close();
    commands->SetName(L"haylen-commands");
    list.Reset();
    HResult::check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frames[0].transfers.Get(), nullptr, IID_PPV_ARGS(list.GetAddressOf())), "create the transfer list of the frames");
    HResult::check(list.As(&transfers), "reach the transfer list of Direct3D 12");
    transfers->Close();
    transfers->SetName(L"haylen-transfers");

    if (validation && SUCCEEDED(device.As(&infoQueue))) {
        messages = std::make_unique<direct3d::DebugMessages>();
    }
}

void Device::readCapabilities() {
    capabilities.maxImageSize = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    capabilities.maxArrayLayers = D3D12_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
    capabilities.originTopLeft = true;
    capabilities.independentBlend = true;
    capabilities.multisampledRenderTargets = true;
    capabilities.gpuTiming = true;

    // The swapchain and the render targets of every color format share the sample counts of the formats that draws use most.
    for (const UINT count : {8U, 4U, 2U}) {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS color{.Format = DXGI_FORMAT_R8G8B8A8_UNORM, .SampleCount = count, .Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE, .NumQualityLevels = 0};
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS depth{.Format = DXGI_FORMAT_D24_UNORM_S8_UINT, .SampleCount = count, .Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE, .NumQualityLevels = 0};
        device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &color, sizeof(color));
        device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &depth, sizeof(depth));
        if (color.NumQualityLevels > 0 && depth.NumQualityLevels > 0) {
            capabilities.maxSampleCount = static_cast<int>(count);
            break;
        }
    }

    for (std::size_t index = 0; index < Formats::kPixelFormatCount; ++index) {
        const auto format = static_cast<PixelFormat>(index);
        D3D12_FEATURE_DATA_FORMAT_SUPPORT support{.Format = Formats::getViewFormat(format), .Support1 = D3D12_FORMAT_SUPPORT1_NONE, .Support2 = D3D12_FORMAT_SUPPORT2_NONE};
        const bool known = !Formats::isDepth(format) && SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)));
        capabilities.blendableFormats[index] = known && (support.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET) != 0 && (support.Support1 & D3D12_FORMAT_SUPPORT1_BLENDABLE) != 0;
    }
}

gpu::Device::Language Device::getLanguage() const noexcept {
    return Language::Hlsl5;
}

gpu::Device::Api Device::getApi() const noexcept {
    return Api::Direct3D12;
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
    core::Log::error("Direct3D 12 lost its device, because {}.", HResult::describe(FAILED(removed) ? removed : reason));
    describeRemoval();
}

// The breadcrumbs name the last operation that each command list finished on the GPU before it was removed, and the page fault names the address it read where no memory was.
void Device::describeRemoval() noexcept {
    ComPtr<ID3D12DeviceRemovedExtendedData> data;
    if (!dred || FAILED(device.As(&data))) {
        return;
    }
    D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumbs{};
    if (SUCCEEDED(data->GetAutoBreadcrumbsOutput(&breadcrumbs))) {
        for (const D3D12_AUTO_BREADCRUMB_NODE* node = breadcrumbs.pHeadAutoBreadcrumbNode; node != nullptr; node = node->pNext) {
            const UINT finished = node->pLastBreadcrumbValue != nullptr ? *node->pLastBreadcrumbValue : 0;
            if (finished < node->BreadcrumbCount) {
                const std::string list = node->pCommandListDebugNameW != nullptr ? direct3d::WideText::toUtf8(node->pCommandListDebugNameW) : std::string("unnamed");
                core::Log::error("The command list \"{}\" of Direct3D 12 finished {} of its {} operations, and stopped at the operation {}.", list, finished, node->BreadcrumbCount, static_cast<int>(node->pCommandHistory[finished]));
            }
        }
    }
    D3D12_DRED_PAGE_FAULT_OUTPUT fault{};
    if (SUCCEEDED(data->GetPageFaultAllocationOutput(&fault)) && fault.PageFaultVA != 0) {
        core::Log::error("The GPU read the address 0x{:X}, where Direct3D 12 held no memory.", fault.PageFaultVA);
    }
}

void Device::setName(ID3D12Object& object, std::string_view label) {
    if (!label.empty()) {
        object.SetName(direct3d::WideText::toWide(label).c_str());
    }
}

D3D12_RESOURCE_BARRIER Device::makeTransition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, UINT subresource) noexcept {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition = {.pResource = resource, .Subresource = subresource, .StateBefore = before, .StateAfter = after};
    return barrier;
}

// A removed device passes every fence value at once, so no wait outlives it.
void Device::waitFor(std::uint64_t value) {
    if (fence->GetCompletedValue() >= value) {
        return;
    }
    if (SUCCEEDED(fence->SetEventOnCompletion(value, fenceEvent))) {
        WaitForSingleObject(fenceEvent, INFINITE);
    }
}

// The fence of the frames advances only with commits, which readbacks and released objects count on, so waiting for the queue to drain signals a fence of its own.
void Device::waitIdle() {
    ++idleValue;
    if (SUCCEEDED(queue->Signal(idleFence.Get(), idleValue)) && idleFence->GetCompletedValue() < idleValue && SUCCEEDED(idleFence->SetEventOnCompletion(idleValue, fenceEvent))) {
        WaitForSingleObject(fenceEvent, INFINITE);
    }
    release(fence->GetCompletedValue());
}

void Device::retire(ComPtr<IUnknown> object, DescriptorHeap* heap, D3D12_CPU_DESCRIPTOR_HANDLE handle) {
    garbage.push_back({.fence = fenceValue + 1, .object = std::move(object), .heap = heap, .handle = handle});
}

void Device::release(std::uint64_t completedValue) {
    while (!garbage.empty() && garbage.front().fence <= completedValue) {
        if (garbage.front().heap != nullptr) {
            garbage.front().heap->free(garbage.front().handle);
        }
        garbage.pop_front();
    }
    uploads->release(completedValue);
    textureTables->release(completedValue);
    samplerTables->release(completedValue);
}

void Device::setHeaps() {
    const std::array<ID3D12DescriptorHeap*, 2> heaps{textureTables->get(), samplerTables->get()};
    commands->SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());
    bound.tablesSet = {};
    bound.tableCounts = {};
}

// The frame waits for the GPU to finish the frame that used the same slot, then reuses its command memory, and the list starts from a state that it sets in full.
void Device::openFrame() {
    if (frameOpen) {
        return;
    }
    Frame& frame = frames[frameIndex];
    waitFor(frame.fence);
    release(fence->GetCompletedValue());
    timer->collect(frameIndex);

    frame.commands->Reset();
    frame.transfers->Reset();
    commands->Reset(frame.commands.Get(), nullptr);
    frameOpen = true;
    transfersOpen = false;
    bound = {};
    setHeaps();
    commands->OMSetBlendFactor(blendColor.data());
    commands->OMSetStencilRef(stencilReference);
    timer->begin(*commands.Get(), frameIndex);
}

ID3D12GraphicsCommandList4& Device::getTransferList() {
    openFrame();
    if (!transfersOpen) {
        transfers->Reset(frames[frameIndex].transfers.Get(), nullptr);
        transfersOpen = true;
    }
    return *transfers.Get();
}

ComPtr<ID3D12Resource> Device::makeBufferResource(D3D12_HEAP_TYPE heap, UINT64 size, D3D12_RESOURCE_STATES state, std::string_view label) {
    const D3D12_HEAP_PROPERTIES properties{.Type = heap};
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ComPtr<ID3D12Resource> resource;
    HResult::check(device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(resource.GetAddressOf())), std::format("create a buffer of {} bytes", size));
    setName(*resource.Get(), label);
    return resource;
}

gpu::Device::Buffer Device::makeBuffer(const BufferDesc& desc) {
    const std::size_t size = desc.data.empty() ? desc.size : desc.data.size();
    if (size == 0) {
        throw std::logic_error(std::format("The buffer \"{}\" has no size.", desc.label));
    }
    if (desc.usage == Usage::Immutable && desc.data.empty()) {
        throw std::logic_error(std::format("The immutable buffer \"{}\" has no data.", desc.label));
    }

    BufferRecord record{.resource = makeBufferResource(D3D12_HEAP_TYPE_DEFAULT, size, D3D12_RESOURCE_STATE_COMMON, desc.label), .size = size, .usage = desc.usage};
    record.address = record.resource->GetGPUVirtualAddress();
    if (!desc.data.empty()) {
        const UploadAllocator::Allocation staged = uploads->allocate(size, 16);
        std::memcpy(staged.data, desc.data.data(), size);
        getTransferList().CopyBufferRegion(record.resource.Get(), 0, staged.resource, staged.offset, size);
    }
    return {buffers.insert(std::move(record))};
}

void Device::makeTextureView(ImageRecord& image) {
    D3D12_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = Formats::getViewFormat(image.format);
    view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    const auto mips = static_cast<UINT>(image.mipLevels);
    const auto layers = static_cast<UINT>(image.layers);
    const bool multisampled = image.sampleCount > 1;

    switch (image.type) {
    case TextureType::Flat:
        view.ViewDimension = multisampled ? D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D;
        view.Texture2D = {.MostDetailedMip = 0, .MipLevels = mips, .PlaneSlice = 0, .ResourceMinLODClamp = 0.0F};
        break;
    case TextureType::Array:
        view.ViewDimension = multisampled ? D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        if (multisampled) {
            view.Texture2DMSArray = {.FirstArraySlice = 0, .ArraySize = layers};
        } else {
            view.Texture2DArray = {.MostDetailedMip = 0, .MipLevels = mips, .FirstArraySlice = 0, .ArraySize = layers, .PlaneSlice = 0, .ResourceMinLODClamp = 0.0F};
        }
        break;
    case TextureType::Cube:
        view.ViewDimension = layers > 6 ? D3D12_SRV_DIMENSION_TEXTURECUBEARRAY : D3D12_SRV_DIMENSION_TEXTURECUBE;
        if (layers > 6) {
            view.TextureCubeArray = {.MostDetailedMip = 0, .MipLevels = mips, .First2DArrayFace = 0, .NumCubes = layers / 6, .ResourceMinLODClamp = 0.0F};
        } else {
            view.TextureCube = {.MostDetailedMip = 0, .MipLevels = mips, .ResourceMinLODClamp = 0.0F};
        }
        break;
    case TextureType::Volume:
        view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        view.Texture3D = {.MostDetailedMip = 0, .MipLevels = mips, .ResourceMinLODClamp = 0.0F};
        break;
    }
    image.view = textureViews->allocate();
    device->CreateShaderResourceView(image.resource.Get(), &view, image.view);
}

// The pixels go into upload memory at the footprints that Direct3D 12 lays out for each subresource, and the transfer list copies them into the image, which it moves out of its state and back. Each span holds every layer of its mip level, and subresources count layers apart by the mip levels of the image.
void Device::stagePixels(const ImageRecord& image, std::span<const std::span<const std::byte>> levels) {
    const D3D12_RESOURCE_DESC desc = image.resource->GetDesc();
    const bool volume = image.type == TextureType::Volume;
    const auto layers = static_cast<UINT>(volume ? 1 : image.layers);
    const auto count = static_cast<UINT>(levels.size());

    footprints.resize(static_cast<std::size_t>(count) * layers);
    UINT64 total = 0;
    for (UINT layer = 0; layer < layers; ++layer) {
        const UINT first = layer * static_cast<UINT>(image.mipLevels);
        UINT64 bytes = 0;
        device->GetCopyableFootprints(&desc, first, count, total, footprints.data() + static_cast<std::size_t>(layer) * count, nullptr, nullptr, &bytes);
        total += (bytes + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1) / D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT * D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
    }
    const UploadAllocator::Allocation staged = uploads->allocate(total, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

    ID3D12GraphicsCommandList4& list = getTransferList();
    const D3D12_RESOURCE_BARRIER toCopy = makeTransition(image.resource.Get(), image.home, D3D12_RESOURCE_STATE_COPY_DEST);
    list.ResourceBarrier(1, &toCopy);
    for (UINT layer = 0; layer < layers; ++layer) {
        for (UINT index = 0; index < count; ++index) {
            const direct3d::MipLevels::Level level = direct3d::MipLevels::describe(image.type, image.width, image.height, image.layers, image.format, static_cast<int>(index));
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = footprints[static_cast<std::size_t>(layer) * count + index];
            const std::byte* source = levels[index].data() + (volume ? 0 : layer * level.sliceBytes);
            const std::size_t slices = volume ? level.layers : 1;
            for (std::size_t slice = 0; slice < slices; ++slice) {
                for (std::size_t row = 0; row < level.height; ++row) {
                    std::byte* target = staged.data + footprint.Offset + (slice * level.height + row) * footprint.Footprint.RowPitch;
                    std::memcpy(target, source + slice * level.sliceBytes + row * level.rowBytes, level.rowBytes);
                }
            }

            footprint.Offset += staged.offset;
            const D3D12_TEXTURE_COPY_LOCATION destination{.pResource = image.resource.Get(), .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, .SubresourceIndex = layer * static_cast<UINT>(image.mipLevels) + index};
            D3D12_TEXTURE_COPY_LOCATION from{.pResource = staged.resource, .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
            from.PlacedFootprint = footprint;
            list.CopyTextureRegion(&destination, 0, 0, 0, &from, nullptr);
        }
    }
    const D3D12_RESOURCE_BARRIER toHome = makeTransition(image.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, image.home);
    list.ResourceBarrier(1, &toHome);
}

gpu::Device::Image Device::makeImage(const ImageDesc& desc) {
    if (desc.width <= 0 || desc.height <= 0 || desc.width > capabilities.maxImageSize || desc.height > capabilities.maxImageSize) {
        throw std::logic_error(std::format("The image \"{}\" of {}x{} pixels does not fit the device.", desc.label, desc.width, desc.height));
    }
    const bool depth = Formats::isDepth(desc.format);
    if (depth != (desc.kind == ImageKind::DepthStencil)) {
        throw std::logic_error(std::format("The image \"{}\" needs a depth format exactly when it is a depth stencil image.", desc.label));
    }
    if (desc.kind == ImageKind::Texture) {
        direct3d::MipLevels::check(desc);
    } else if (!desc.levels.empty()) {
        throw std::logic_error(std::format("The image \"{}\" starts without pixels, since only a texture is made with them.", desc.label));
    }

    D3D12_RESOURCE_DESC made{};
    made.Dimension = desc.type == TextureType::Volume ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    made.Width = static_cast<UINT64>(desc.width);
    made.Height = static_cast<UINT>(desc.height);
    made.DepthOrArraySize = static_cast<UINT16>(desc.layers);
    made.MipLevels = static_cast<UINT16>(desc.mipLevels);
    made.Format = Formats::getResourceFormat(desc.format);
    made.SampleDesc.Count = static_cast<UINT>(desc.sampleCount);
    made.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    made.Flags = desc.kind == ImageKind::RenderTarget ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : (depth ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_NONE);

    ImageRecord record{.type = desc.type, .width = desc.width, .height = desc.height, .layers = desc.layers, .mipLevels = desc.mipLevels, .sampleCount = desc.sampleCount, .format = desc.format, .kind = desc.kind};
    record.home = depth ? D3D12_RESOURCE_STATE_DEPTH_READ | kShaderResource : kShaderResource;
    const D3D12_HEAP_PROPERTIES heap{.Type = D3D12_HEAP_TYPE_DEFAULT};
    HResult::check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &made, record.home, nullptr, IID_PPV_ARGS(record.resource.GetAddressOf())), std::format("create the {}x{} image \"{}\"", desc.width, desc.height, desc.label));
    setName(*record.resource.Get(), desc.label);
    makeTextureView(record);
    if (desc.kind == ImageKind::Texture) {
        stagePixels(record, desc.levels);
    }
    return {images.insert(std::move(record))};
}

gpu::Device::Sampler Device::makeSampler(const SamplerDesc& desc) {
    const D3D12_SAMPLER_DESC made = StateDescriptions::makeSampler(desc);
    const D3D12_CPU_DESCRIPTOR_HANDLE handle = samplerViews->allocate();
    device->CreateSampler(&made, handle);
    return {samplers.insert(handle)};
}

gpu::Device::Program Device::makeProgram(const ProgramDesc& desc) {
    ProgramRecord record{.layout = rootSignatures->make(desc)};
    record.vertexCode = compiler->compile(desc.vertex, desc.label);
    record.fragmentCode = compiler->compile(desc.fragment, desc.label);
    return {programs.insert(std::move(record))};
}

void Device::prepareProgram(const ProgramDesc& desc, core::JobSystem& jobs) {
    compiler->prepare(desc, jobs);
}

int Device::getSwapchainSampleCount() const noexcept {
    return surface ? std::max(1, surface->sampleCount) : 1;
}

gpu::Device::Pipeline Device::makePipeline(const PipelineDesc& desc) {
    const ProgramRecord& program = programs.get(desc.program.id);
    if (desc.colors.empty() || desc.colors.size() > kMaxColorTargets || desc.buffers.size() > kMaxVertexBuffers) {
        throw std::logic_error(std::format("The pipeline \"{}\" needs one to {} color targets and at most {} vertex buffers.", desc.label, kMaxColorTargets, kMaxVertexBuffers));
    }

    PipelineRecord record{.layout = program.layout, .topology = StateDescriptions::toTopology(desc.primitive), .bufferCount = static_cast<UINT>(desc.buffers.size()), .indexFormat = Formats::getIndexFormat(desc.indexType), .swapchain = desc.swapchain, .vertexCode = program.vertexCode, .fragmentCode = program.fragmentCode};
    for (std::size_t index = 0; index < desc.buffers.size(); ++index) {
        record.strides[index] = static_cast<UINT>(desc.buffers[index].stride);
    }
    for (const VertexAttribute& attribute : desc.attributes) {
        if (attribute.buffer < 0 || attribute.buffer >= static_cast<int>(desc.buffers.size())) {
            throw std::logic_error(std::format("An attribute of the pipeline \"{}\" reads a vertex buffer it does not lay out.", desc.label));
        }
        const bool instanced = desc.buffers[static_cast<std::size_t>(attribute.buffer)].perInstance;
        record.elements.push_back({.SemanticName = "TEXCOORD", .SemanticIndex = static_cast<UINT>(attribute.location), .Format = Formats::getVertexFormat(attribute.format), .InputSlot = static_cast<UINT>(attribute.buffer), .AlignedByteOffset = static_cast<UINT>(attribute.offset), .InputSlotClass = instanced ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = instanced ? 1U : 0U});
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC& state = record.description;
    state.pRootSignature = record.layout.signature.Get();
    state.VS = {.pShaderBytecode = record.vertexCode->GetBufferPointer(), .BytecodeLength = record.vertexCode->GetBufferSize()};
    state.PS = {.pShaderBytecode = record.fragmentCode->GetBufferPointer(), .BytecodeLength = record.fragmentCode->GetBufferSize()};
    state.BlendState = StateDescriptions::makeBlend(desc);
    state.SampleMask = UINT_MAX;
    state.RasterizerState = StateDescriptions::makeRasterizer(desc);
    state.DepthStencilState = StateDescriptions::makeDepthStencil(desc);
    state.InputLayout = {.pInputElementDescs = record.elements.data(), .NumElements = static_cast<UINT>(record.elements.size())};
    state.PrimitiveTopologyType = StateDescriptions::toTopologyType(desc.primitive);

    // Strips restart at the largest index, as on every other device.
    const bool strip = desc.primitive == Primitive::TriangleStrip || desc.primitive == Primitive::LineStrip;
    if (strip && desc.indexType != IndexType::None) {
        state.IBStripCutValue = desc.indexType == IndexType::UInt16 ? D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF : D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF;
    }

    if (desc.swapchain) {
        state.NumRenderTargets = 1;
        state.RTVFormats[0] = Formats::kSwapchainColor;
        state.DSVFormat = Formats::getDepthFormat(Formats::kSwapchainDepthFormat);
        state.SampleDesc.Count = static_cast<UINT>(pipelineSampleCount);
    } else {
        state.NumRenderTargets = static_cast<UINT>(desc.colors.size());
        for (std::size_t index = 0; index < desc.colors.size(); ++index) {
            state.RTVFormats[index] = Formats::getViewFormat(desc.colors[index].format);
        }
        state.DSVFormat = desc.depthStencil.format ? Formats::getDepthFormat(*desc.depthStencil.format) : DXGI_FORMAT_UNKNOWN;
        state.SampleDesc.Count = static_cast<UINT>(std::max(1, desc.sampleCount));
    }

    HResult::check(device->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(record.state.GetAddressOf())), std::format("create the pipeline \"{}\"", desc.label));
    setName(*record.state.Get(), desc.label);

    // Only a pipeline of the swapchain is ever made again, so the others let go of what made them.
    if (!desc.swapchain) {
        record.vertexCode.Reset();
        record.fragmentCode.Reset();
        record.elements.clear();
        record.description = {};
    }
    return {pipelines.insert(std::move(record))};
}

void Device::rebuildSwapchainPipelines() {
    // clang-format off
    pipelines.forEach([this](PipelineRecord& record) {
        if (!record.swapchain) {
            return;
        }
        record.description.SampleDesc.Count = static_cast<UINT>(pipelineSampleCount);
        ComPtr<ID3D12PipelineState> made;
        HResult::check(device->CreateGraphicsPipelineState(&record.description, IID_PPV_ARGS(made.GetAddressOf())), "make a pipeline of the swapchain again for its new sample count");
        retire(std::move(record.state));
        record.state = std::move(made);
    });
    // clang-format on
    bound.state = nullptr;
}

void Device::destroy(Buffer buffer) noexcept {
    if (std::optional<BufferRecord> record = buffers.erase(buffer.id)) {
        retire(std::move(record->resource));
    }
}

void Device::destroy(Image image) noexcept {
    std::optional<ImageRecord> record = images.erase(image.id);
    if (!record) {
        return;
    }
    retire(nullptr, textureViews.get(), record->view);
    for (const Attachment& attachment : record->attachments) {
        retire(nullptr, record->kind == ImageKind::DepthStencil ? depthViews.get() : targetViews.get(), attachment.handle);
    }
    retire(std::move(record->resource));
}

void Device::destroy(Sampler sampler) noexcept {
    if (std::optional<D3D12_CPU_DESCRIPTOR_HANDLE> handle = samplers.erase(sampler.id)) {
        retire(nullptr, samplerViews.get(), *handle);
    }
}

void Device::destroy(Program program) noexcept {
    programs.erase(program.id);
}

void Device::destroy(Pipeline pipeline) noexcept {
    if (std::optional<PipelineRecord> record = pipelines.erase(pipeline.id)) {
        retire(std::move(record->state));
    }
    if (bound.pipeline == pipeline.id) {
        bound.pipeline = 0;
    }
}

void Device::writeBuffer(Buffer buffer, std::size_t offset, std::span<const std::byte> data) {
    const BufferRecord& record = buffers.get(buffer.id);
    if (record.usage != Usage::Dynamic) {
        throw std::logic_error("Only a dynamic buffer takes new contents.");
    }
    if (offset + data.size() > record.size) {
        throw std::logic_error(std::format("The buffer of {} bytes cannot take {} bytes at {}.", record.size, data.size(), offset));
    }
    if (data.empty()) {
        return;
    }
    const UploadAllocator::Allocation staged = uploads->allocate(data.size(), 16);
    std::memcpy(staged.data, data.data(), data.size());
    getTransferList().CopyBufferRegion(record.resource.Get(), offset, staged.resource, staged.offset, data.size());
}

void Device::writeImages(std::span<const ImageWrite> writes) {
    for (const ImageWrite& write : writes) {
        const ImageRecord& image = images.get(write.image.id);
        if (image.kind != ImageKind::Dynamic) {
            throw std::logic_error("Only a dynamic image takes new pixels.");
        }
        const direct3d::MipLevels::Level level = direct3d::MipLevels::describe(image.type, image.width, image.height, image.layers, image.format, 0);
        if (write.pixels.size() != level.sliceBytes * level.layers) {
            throw std::logic_error(std::format("The pixels do not fill the image of {}x{} with {} layers.", image.width, image.height, image.layers));
        }
        const std::array<std::span<const std::byte>, 1> levels{write.pixels};
        stagePixels(image, levels);
    }
}

bool Device::prepareSwapchain() {
    if (!surface || surface->width <= 0 || surface->height <= 0 || lost) {
        return false;
    }
    if (swapchain == nullptr) {
        swapchain = std::make_unique<direct3d::Swapchain>(*factory.Get(), *queue.Get(), *surface);
        makeSwapchainTargets();
        return true;
    }
    // A new size applies from the first pass of a frame into the swapchain, so the passes of one frame draw into the same buffers.
    if (presentPending || (swapchain->getWidth() == surface->width && swapchain->getHeight() == surface->height)) {
        return true;
    }

    // The buffers resize only once the GPU let go of them and the device holds no reference to them.
    waitIdle();
    releaseSwapchainTargets();
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
    for (UINT index = 0; index < direct3d::Swapchain::kBufferCount; ++index) {
        HResult::check(swapchain->get().GetBuffer(index, IID_PPV_ARGS(backBuffers[index].GetAddressOf())), "reach a buffer of the swapchain");
        device->CreateRenderTargetView(backBuffers[index].Get(), nullptr, backBufferViews[index]);
        backBuffers[index]->SetName(L"haylen-swapchain");
    }

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = static_cast<UINT64>(swapchain->getWidth());
    desc.Height = static_cast<UINT>(swapchain->getHeight());
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = static_cast<UINT>(getSwapchainSampleCount());
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    const D3D12_HEAP_PROPERTIES heap{.Type = D3D12_HEAP_TYPE_DEFAULT};

    // Flip model swapchains hold one sample, so a multisampled window draws into a texture of its own that resolves into the swapchain when the frame is presented.
    if (desc.SampleDesc.Count > 1) {
        desc.Format = Formats::kSwapchainColor;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        HResult::check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(multisampleColor.GetAddressOf())), "create the multisampled color of the swapchain");
        device->CreateRenderTargetView(multisampleColor.Get(), nullptr, multisampleView);
        multisampleColor->SetName(L"haylen-swapchain-color");
    }

    desc.Format = Formats::getDepthFormat(Formats::kSwapchainDepthFormat);
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    HResult::check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, nullptr, IID_PPV_ARGS(swapchainDepth.GetAddressOf())), "create the depth and stencil of the swapchain");
    device->CreateDepthStencilView(swapchainDepth.Get(), nullptr, swapchainDepthView);
    swapchainDepth->SetName(L"haylen-swapchain-depth");
}

void Device::releaseSwapchainTargets() {
    for (ComPtr<ID3D12Resource>& buffer : backBuffers) {
        buffer.Reset();
    }
    multisampleColor.Reset();
    swapchainDepth.Reset();
}

void Device::setViewportAndScissor(int width, int height) {
    const D3D12_VIEWPORT viewport{.TopLeftX = 0.0F, .TopLeftY = 0.0F, .Width = static_cast<float>(width), .Height = static_cast<float>(height), .MinDepth = 0.0F, .MaxDepth = 1.0F};
    const D3D12_RECT scissor{.left = 0, .top = 0, .right = width, .bottom = height};
    commands->RSSetViewports(1, &viewport);
    commands->RSSetScissorRects(1, &scissor);
}

void Device::beginPass(const PassDesc& desc) {
    if (passOpen) {
        throw std::logic_error("A pass began while another pass was open.");
    }
    openFrame();
    passOpen = true;
    pass.endBarrierCount = 0;
    pass.labeled = !desc.label.empty();
    if (pass.labeled) {
        pushDebugGroup(desc.label);
    }

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

    // The first pass of the frame into the swapchain takes the buffer that DXGI hands out next, and the frame presents it.
    if (!presentPending) {
        presentPending = true;
        backBufferIndex = swapchain->get().GetCurrentBackBufferIndex();
        if (multisampleColor == nullptr) {
            const D3D12_RESOURCE_BARRIER barrier = makeTransition(backBuffers[backBufferIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            commands->ResourceBarrier(1, &barrier);
        }
    }

    const D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE begin = desc.load == Load::Clear ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR : (desc.load == Load::Keep ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD);
    D3D12_RENDER_PASS_RENDER_TARGET_DESC color{};
    color.cpuDescriptor = multisampleColor != nullptr ? multisampleView : backBufferViews[backBufferIndex];
    color.BeginningAccess.Type = begin;
    color.BeginningAccess.Clear.ClearValue = {.Format = Formats::kSwapchainColor, .Color = {desc.clear.r, desc.clear.g, desc.clear.b, desc.clear.a}};
    color.EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC depth{};
    depth.cpuDescriptor = swapchainDepthView;
    depth.DepthBeginningAccess.Type = begin;
    depth.StencilBeginningAccess.Type = begin;
    const DXGI_FORMAT depthFormat = Formats::getDepthFormat(Formats::kSwapchainDepthFormat);
    depth.DepthBeginningAccess.Clear.ClearValue = {.Format = depthFormat, .DepthStencil = {.Depth = desc.clearDepth, .Stencil = desc.clearStencil}};
    depth.StencilBeginningAccess.Clear.ClearValue = depth.DepthBeginningAccess.Clear.ClearValue;
    depth.DepthEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
    depth.StencilEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

    commands->BeginRenderPass(1, &color, &depth, D3D12_RENDER_PASS_FLAG_NONE);
    setViewportAndScissor(swapchain->getWidth(), swapchain->getHeight());
}

// The attachments leave the state that shaders read for the pass, and the barriers that bring them back wait for its end. A multisampled attachment resolves as its render pass ends, which tile GPUs do without writing the samples back to memory.
void Device::beginOffscreenPass(const PassDesc& desc) {
    if (desc.colors.size() > kMaxColorTargets) {
        throw std::logic_error(std::format("A pass draws into at most {} color attachments.", kMaxColorTargets));
    }
    std::array<D3D12_RESOURCE_BARRIER, kMaxColorTargets * 2 + 1> barriers{};
    UINT barrierCount = 0;
    std::array<D3D12_RENDER_PASS_RENDER_TARGET_DESC, kMaxColorTargets> colors{};
    int width = 0;
    int height = 0;

    for (std::size_t index = 0; index < desc.colors.size(); ++index) {
        const ColorAttachment& attachment = desc.colors[index];
        ImageRecord& image = images.get(attachment.image.id);
        if (image.kind != ImageKind::RenderTarget) {
            throw std::logic_error("A pass draws only into render targets.");
        }
        const UINT subresource = image.type == TextureType::Volume ? static_cast<UINT>(attachment.mipLevel) : static_cast<UINT>(attachment.mipLevel + attachment.layer * image.mipLevels);
        barriers[barrierCount++] = makeTransition(image.resource.Get(), image.home, D3D12_RESOURCE_STATE_RENDER_TARGET, subresource);
        pass.endBarriers[pass.endBarrierCount++] = makeTransition(image.resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, image.home, subresource);
        width = std::max(1, image.width >> attachment.mipLevel);
        height = std::max(1, image.height >> attachment.mipLevel);

        D3D12_RENDER_PASS_RENDER_TARGET_DESC& color = colors[index];
        color.cpuDescriptor = getAttachment(image, attachment.mipLevel, attachment.layer);
        color.BeginningAccess.Type = attachment.load == Load::Clear ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR : (attachment.load == Load::Keep ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD);
        color.BeginningAccess.Clear.ClearValue = {.Format = Formats::getViewFormat(image.format), .Color = {attachment.clear.r, attachment.clear.g, attachment.clear.b, attachment.clear.a}};
        color.EndingAccess.Type = attachment.store == Store::Store ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
        if (!attachment.resolve.isValid()) {
            continue;
        }

        const ImageRecord& resolve = images.get(attachment.resolve.id);
        barriers[barrierCount++] = makeTransition(resolve.resource.Get(), resolve.home, D3D12_RESOURCE_STATE_RESOLVE_DEST, 0);
        pass.endBarriers[pass.endBarrierCount++] = makeTransition(resolve.resource.Get(), D3D12_RESOURCE_STATE_RESOLVE_DEST, resolve.home, 0);
        // An empty source rectangle resolves the whole subresource, the only resolve that GPUs without programmable sample positions take.
        pass.resolves[index] = {.SrcSubresource = subresource, .DstSubresource = 0, .DstX = 0, .DstY = 0, .SrcRect = {.left = 0, .top = 0, .right = 0, .bottom = 0}};
        color.EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE;
        color.EndingAccess.Resolve = {.pSrcResource = image.resource.Get(), .pDstResource = resolve.resource.Get(), .SubresourceCount = 1, .pSubresourceParameters = &pass.resolves[index], .Format = Formats::getViewFormat(image.format), .ResolveMode = D3D12_RESOLVE_MODE_AVERAGE, .PreserveResolveSource = attachment.store == Store::Store ? TRUE : FALSE};
    }

    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC depth{};
    if (desc.depthStencil) {
        const DepthStencilAttachment& attachment = *desc.depthStencil;
        ImageRecord& image = images.get(attachment.image.id);
        if (image.kind != ImageKind::DepthStencil) {
            throw std::logic_error("A pass takes only a depth stencil image as its depth and stencil attachment.");
        }
        barriers[barrierCount++] = makeTransition(image.resource.Get(), image.home, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        pass.endBarriers[pass.endBarrierCount++] = makeTransition(image.resource.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, image.home);
        width = width > 0 ? width : image.width;
        height = height > 0 ? height : image.height;

        const auto access = [](Load load) { return load == Load::Clear ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR : (load == Load::Keep ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD); };
        const auto ending = [](Store store) { return store == Store::Store ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD; };
        const bool stencil = Formats::hasStencil(image.format);
        const D3D12_CLEAR_VALUE clear{.Format = Formats::getDepthFormat(image.format), .DepthStencil = {.Depth = attachment.clearDepth, .Stencil = attachment.clearStencil}};
        depth.cpuDescriptor = getAttachment(image, 0, 0);
        depth.DepthBeginningAccess.Type = access(attachment.depthLoad);
        depth.DepthBeginningAccess.Clear.ClearValue = clear;
        depth.DepthEndingAccess.Type = ending(attachment.depthStore);
        depth.StencilBeginningAccess.Type = stencil ? access(attachment.stencilLoad) : D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
        depth.StencilBeginningAccess.Clear.ClearValue = clear;
        depth.StencilEndingAccess.Type = stencil ? ending(attachment.stencilStore) : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
    }

    commands->ResourceBarrier(barrierCount, barriers.data());
    commands->BeginRenderPass(static_cast<UINT>(desc.colors.size()), colors.data(), desc.depthStencil ? &depth : nullptr, D3D12_RENDER_PASS_FLAG_NONE);
    setViewportAndScissor(width, height);
}

D3D12_CPU_DESCRIPTOR_HANDLE Device::getAttachment(ImageRecord& image, int mipLevel, int layer) {
    for (const Attachment& attachment : image.attachments) {
        if (attachment.mipLevel == mipLevel && attachment.layer == layer) {
            return attachment.handle;
        }
    }

    const auto mip = static_cast<UINT>(mipLevel);
    const auto slice = static_cast<UINT>(layer);
    const bool multisampled = image.sampleCount > 1;
    if (image.kind == ImageKind::DepthStencil) {
        D3D12_DEPTH_STENCIL_VIEW_DESC view{};
        view.Format = Formats::getDepthFormat(image.format);
        if (image.type == TextureType::Flat) {
            view.ViewDimension = multisampled ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
            view.Texture2D.MipSlice = mip;
        } else {
            view.ViewDimension = multisampled ? D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY : D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            if (multisampled) {
                view.Texture2DMSArray = {.FirstArraySlice = slice, .ArraySize = 1};
            } else {
                view.Texture2DArray = {.MipSlice = mip, .FirstArraySlice = slice, .ArraySize = 1};
            }
        }
        const D3D12_CPU_DESCRIPTOR_HANDLE handle = depthViews->allocate();
        device->CreateDepthStencilView(image.resource.Get(), &view, handle);
        return image.attachments.emplace_back(Attachment{.mipLevel = mipLevel, .layer = layer, .handle = handle}).handle;
    }

    D3D12_RENDER_TARGET_VIEW_DESC view{};
    view.Format = Formats::getViewFormat(image.format);
    switch (image.type) {
    case TextureType::Flat:
        view.ViewDimension = multisampled ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;
        view.Texture2D.MipSlice = mip;
        break;
    case TextureType::Array:
    case TextureType::Cube:
        view.ViewDimension = multisampled ? D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        if (multisampled) {
            view.Texture2DMSArray = {.FirstArraySlice = slice, .ArraySize = 1};
        } else {
            view.Texture2DArray = {.MipSlice = mip, .FirstArraySlice = slice, .ArraySize = 1, .PlaneSlice = 0};
        }
        break;
    case TextureType::Volume:
        view.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
        view.Texture3D = {.MipSlice = mip, .FirstWSlice = slice, .WSize = 1};
        break;
    }
    const D3D12_CPU_DESCRIPTOR_HANDLE handle = targetViews->allocate();
    device->CreateRenderTargetView(image.resource.Get(), &view, handle);
    return image.attachments.emplace_back(Attachment{.mipLevel = mipLevel, .layer = layer, .handle = handle}).handle;
}

void Device::applyViewport(const math::Rect& pixels) {
    if (skipping) {
        return;
    }
    const D3D12_VIEWPORT viewport{.TopLeftX = pixels.x, .TopLeftY = pixels.y, .Width = pixels.width, .Height = pixels.height, .MinDepth = 0.0F, .MaxDepth = 1.0F};
    commands->RSSetViewports(1, &viewport);
}

void Device::applyScissor(const math::Rect& pixels) {
    if (skipping) {
        return;
    }
    const D3D12_RECT scissor{.left = std::lround(pixels.x), .top = std::lround(pixels.y), .right = std::lround(pixels.x + pixels.width), .bottom = std::lround(pixels.y + pixels.height)};
    commands->RSSetScissorRects(1, &scissor);
}

// A new root signature drops every root argument, so the tables are set again for the next draw.
void Device::applyPipeline(Pipeline pipeline) {
    if (skipping || bound.pipeline == pipeline.id) {
        return;
    }
    const PipelineRecord& record = pipelines.get(pipeline.id);
    bound.pipeline = pipeline.id;
    bound.indexed = record.indexFormat != DXGI_FORMAT_UNKNOWN;

    if (bound.state != record.state.Get()) {
        bound.state = record.state.Get();
        commands->SetPipelineState(bound.state);
    }
    if (bound.signature != record.layout.signature.Get()) {
        bound.signature = record.layout.signature.Get();
        commands->SetGraphicsRootSignature(bound.signature);
        bound.tablesSet = {};
        bound.tableCounts = {};
    }
    if (bound.topology != record.topology) {
        bound.topology = record.topology;
        commands->IASetPrimitiveTopology(bound.topology);
    }
}

void Device::applyBindings(const Bindings& bindings) {
    if (skipping) {
        return;
    }
    const PipelineRecord& pipeline = pipelines.get(bound.pipeline);

    std::array<D3D12_VERTEX_BUFFER_VIEW, kMaxVertexBuffers> views{};
    for (std::size_t index = 0; index < pipeline.bufferCount; ++index) {
        const BufferRecord& buffer = buffers.get(bindings.vertexBuffers[index].id);
        const auto offset = static_cast<std::size_t>(bindings.vertexOffsets[index]);
        views[index] = {.BufferLocation = buffer.address + offset, .SizeInBytes = static_cast<UINT>(buffer.size - offset), .StrideInBytes = pipeline.strides[index]};
    }
    if (pipeline.bufferCount != bound.vertexBufferCount || std::memcmp(views.data(), bound.vertexBuffers.data(), sizeof(D3D12_VERTEX_BUFFER_VIEW) * pipeline.bufferCount) != 0) {
        commands->IASetVertexBuffers(0, pipeline.bufferCount, views.data());
        bound.vertexBuffers = views;
        bound.vertexBufferCount = pipeline.bufferCount;
    }

    if (pipeline.indexFormat != DXGI_FORMAT_UNKNOWN) {
        const BufferRecord& buffer = buffers.get(bindings.indexBuffer.id);
        const auto offset = static_cast<std::size_t>(bindings.indexOffset);
        const D3D12_INDEX_BUFFER_VIEW view{.BufferLocation = buffer.address + offset, .SizeInBytes = static_cast<UINT>(buffer.size - offset), .Format = pipeline.indexFormat};
        if (std::memcmp(&view, &bound.indexBuffer, sizeof(view)) != 0) {
            commands->IASetIndexBuffer(&view);
            bound.indexBuffer = view;
        }
    }

    // A heap that grows replaces the heaps of the list, which drops every table set so far, so the tables are set again in the new heap.
    if (!applyTables(pipeline, bindings)) {
        retire(textureTables->grow());
        setHeaps();
        if (!applyTables(pipeline, bindings)) {
            throw std::runtime_error("The textures of one draw take more descriptors than a heap of Direct3D 12 holds.");
        }
    }
}

bool Device::applyTables(const PipelineRecord& pipeline, const Bindings& bindings) {
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, DescriptorRing::kMaxTable> handles{};
    for (std::size_t stage = 0; stage < 2; ++stage) {
        const RootSignatures::Table& textures = pipeline.layout.textures[stage];
        for (std::size_t index = 0; index < textures.slots.size(); ++index) {
            handles[index] = images.get(bindings.textures[static_cast<std::size_t>(textures.slots[index])].id).view;
        }
        if (textures.parameter >= 0 && !applyTable(stage, textures.parameter, std::span(handles).first(textures.slots.size()))) {
            return false;
        }

        const RootSignatures::Table& samplersOfStage = pipeline.layout.samplers[stage];
        for (std::size_t index = 0; index < samplersOfStage.slots.size(); ++index) {
            handles[index] = samplers.get(bindings.samplers[static_cast<std::size_t>(samplersOfStage.slots[index])].id);
        }
        if (samplersOfStage.parameter >= 0 && !applyTable(2 + stage, samplersOfStage.parameter, std::span(handles).first(samplersOfStage.slots.size()))) {
            return false;
        }
    }
    return true;
}

bool Device::applyTable(std::size_t table, int parameter, std::span<const D3D12_CPU_DESCRIPTOR_HANDLE> handles) {
    const bool same = bound.tablesSet[table] && bound.tableCounts[table] == handles.size() && std::ranges::equal(handles, std::span(bound.tables[table]).first(handles.size()), {}, &D3D12_CPU_DESCRIPTOR_HANDLE::ptr, &D3D12_CPU_DESCRIPTOR_HANDLE::ptr);
    if (same) {
        return true;
    }
    DescriptorRing& ring = table < 2 ? *textureTables : *samplerTables;
    const std::optional<D3D12_GPU_DESCRIPTOR_HANDLE> written = ring.write(handles);
    if (!written) {
        return false;
    }
    commands->SetGraphicsRootDescriptorTable(static_cast<UINT>(parameter), *written);
    std::ranges::copy(handles, bound.tables[table].begin());
    bound.tableCounts[table] = static_cast<UINT>(handles.size());
    bound.tablesSet[table] = true;
    return true;
}

void Device::applyUniforms(int slot, std::span<const std::byte> data) {
    if (skipping) {
        return;
    }
    const PipelineRecord& pipeline = pipelines.get(bound.pipeline);
    const bool known = slot >= 0 && slot < static_cast<int>(kMaxBlocks) && pipeline.layout.blockParameters[static_cast<std::size_t>(slot)] >= 0;
    if (!known || data.size() != pipeline.layout.blockSizes[static_cast<std::size_t>(slot)]) {
        throw std::logic_error(std::format("The program has no uniform block of {} bytes at slot {}.", data.size(), slot));
    }
    const UploadAllocator::Allocation staged = uploads->allocate(data.size(), kUniformAlignment);
    std::memcpy(staged.data, data.data(), data.size());
    commands->SetGraphicsRootConstantBufferView(static_cast<UINT>(pipeline.layout.blockParameters[static_cast<std::size_t>(slot)]), staged.address);
}

void Device::applyStencilReference(std::uint8_t value) {
    stencilReference = value;
    if (frameOpen) {
        commands->OMSetStencilRef(stencilReference);
    }
}

void Device::applyBlendColor(math::Color value) {
    blendColor = {value.r, value.g, value.b, value.a};
    if (frameOpen) {
        commands->OMSetBlendFactor(blendColor.data());
    }
}

void Device::draw(int first, int count, int instances) {
    if (skipping || count <= 0 || instances <= 0) {
        return;
    }
    if (bound.indexed) {
        commands->DrawIndexedInstanced(static_cast<UINT>(count), static_cast<UINT>(instances), static_cast<UINT>(first), 0, 0);
        return;
    }
    commands->DrawInstanced(static_cast<UINT>(count), static_cast<UINT>(instances), static_cast<UINT>(first), 0);
}

void Device::endPass() {
    if (!passOpen) {
        throw std::logic_error("A pass ended while no pass was open.");
    }
    if (!skipping) {
        commands->EndRenderPass();
        if (pass.endBarrierCount > 0) {
            commands->ResourceBarrier(pass.endBarrierCount, pass.endBarriers.data());
        }
    }
    if (pass.labeled) {
        popDebugGroup();
    }
    passOpen = false;
    skipping = false;
}

// The markers carry their names as plain text, the encoding that PIX and the other GPU debuggers read.
void Device::pushDebugGroup(std::string_view name) {
    static constexpr UINT kAnsiText = 1;
    openFrame();
    markerText.assign(name);
    commands->BeginEvent(kAnsiText, markerText.c_str(), static_cast<UINT>(markerText.size() + 1));
}

void Device::popDebugGroup() {
    if (frameOpen) {
        commands->EndEvent();
    }
}

void Device::commit() {
    if (passOpen) {
        throw std::logic_error("The frame was committed while a pass was open.");
    }
    openFrame();

    if (presentPending && multisampleColor != nullptr) {
        ID3D12Resource* backBuffer = backBuffers[backBufferIndex].Get();
        const std::array<D3D12_RESOURCE_BARRIER, 2> toResolve{makeTransition(multisampleColor.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE), makeTransition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RESOLVE_DEST)};
        commands->ResourceBarrier(static_cast<UINT>(toResolve.size()), toResolve.data());
        commands->ResolveSubresource(backBuffer, 0, multisampleColor.Get(), 0, Formats::kSwapchainColor);
        const std::array<D3D12_RESOURCE_BARRIER, 2> toPresent{makeTransition(multisampleColor.Get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET), makeTransition(backBuffer, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PRESENT)};
        commands->ResourceBarrier(static_cast<UINT>(toPresent.size()), toPresent.data());
    } else if (presentPending) {
        const D3D12_RESOURCE_BARRIER barrier = makeTransition(backBuffers[backBufferIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        commands->ResourceBarrier(1, &barrier);
    }
    timer->end(*commands.Get(), frameIndex);

    // The transfer list runs before the passes, which read what it copied.
    if (transfersOpen) {
        ID3D12CommandList* list = transfers.Get();
        if (SUCCEEDED(transfers->Close())) {
            queue->ExecuteCommandLists(1, &list);
        }
    }
    ID3D12CommandList* list = commands.Get();
    if (SUCCEEDED(commands->Close())) {
        queue->ExecuteCommandLists(1, &list);
    }
    if (presentPending && swapchain != nullptr) {
        const HRESULT result = swapchain->present();
        if (HResult::isDeviceLoss(result)) {
            loseDevice(result);
        }
    }

    ++fenceValue;
    queue->Signal(fence.Get(), fenceValue);
    frames[frameIndex].fence = fenceValue;
    uploads->endFrame(fenceValue);
    textureTables->endFrame(fenceValue);
    samplerTables->endFrame(fenceValue);
    frameIndex = (frameIndex + 1) % kFramesInFlight;
    frameOpen = false;
    transfersOpen = false;
    presentPending = false;

    if (!lost && FAILED(device->GetDeviceRemovedReason())) {
        loseDevice(device->GetDeviceRemovedReason());
    }
    drainMessages();
}

void Device::drainMessages() {
    if (messages != nullptr) {
        messages->drain<ID3D12InfoQueue, D3D12_MESSAGE>(*infoQueue.Get(), "Direct3D 12");
    }
}

void Device::resizeSurface(int width, int height) {
    if (surface) {
        surface->width = width;
        surface->height = height;
    }
}

void Device::detachSurface() {
    waitIdle();
    releaseSwapchainTargets();
    swapchain.reset();
    surface.reset();
}

void Device::attachSurface(const Surface& target) {
    detachSurface();
    surface = target;
    if (getSwapchainSampleCount() != pipelineSampleCount) {
        pipelineSampleCount = getSwapchainSampleCount();
        rebuildSwapchainPipelines();
    }
}

std::unique_ptr<gpu::Device::Readback> Device::readImage(Image image, int width, int height) {
    const ImageRecord& record = images.get(image.id);
    if (record.kind != ImageKind::RenderTarget || record.sampleCount > 1 || record.type == TextureType::Volume) {
        throw std::logic_error("Only a render target of one sample can be read.");
    }

    const UINT rowBytes = static_cast<UINT>(width) * static_cast<UINT>(Formats::getBytesPerPixel(record.format));
    const UINT pitch = (rowBytes + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) / D3D12_TEXTURE_DATA_PITCH_ALIGNMENT * D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
    ComPtr<ID3D12Resource> buffer = makeBufferResource(D3D12_HEAP_TYPE_READBACK, static_cast<UINT64>(pitch) * static_cast<UINT64>(height), D3D12_RESOURCE_STATE_COPY_DEST, "haylen-readback");

    ID3D12GraphicsCommandList4& list = getTransferList();
    const D3D12_RESOURCE_BARRIER toCopy = makeTransition(record.resource.Get(), record.home, D3D12_RESOURCE_STATE_COPY_SOURCE, 0);
    list.ResourceBarrier(1, &toCopy);
    D3D12_TEXTURE_COPY_LOCATION destination{.pResource = buffer.Get(), .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
    destination.PlacedFootprint = {.Offset = 0, .Footprint = {.Format = Formats::getViewFormat(record.format), .Width = static_cast<UINT>(width), .Height = static_cast<UINT>(height), .Depth = 1, .RowPitch = pitch}};
    const D3D12_TEXTURE_COPY_LOCATION source{.pResource = record.resource.Get(), .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, .SubresourceIndex = 0};
    const D3D12_BOX box{.left = 0, .top = 0, .front = 0, .right = static_cast<UINT>(width), .bottom = static_cast<UINT>(height), .back = 1};
    list.CopyTextureRegion(&destination, 0, 0, 0, &source, &box);
    const D3D12_RESOURCE_BARRIER toHome = makeTransition(record.resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, record.home, 0);
    list.ResourceBarrier(1, &toHome);

    // The copy runs with the frame that the next commit submits, which signals the next fence value.
    return std::make_unique<ImageReadback>(std::move(buffer), fence, fenceValue + 1, pitch, record.format, width, height);
}

} // namespace haylen::gpu::d3d12
