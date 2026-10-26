#include "gpu/webgpu/Connection.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

#include "gpu/webgpu/Conversion.hpp"
#include "haylen/core/Log.hpp"

namespace haylen::gpu::webgpu {

std::unique_ptr<Connection> Connection::pending;
Connection::Status Connection::status = Connection::Status::Idle;
std::string Connection::failure;

Connection::~Connection() {
    if (queue != nullptr) {
        wgpuQueueRelease(queue);
    }
    if (device != nullptr) {
        wgpuDeviceDestroy(device);
        wgpuDeviceRelease(device);
    }
    if (adapter != nullptr) {
        wgpuAdapterRelease(adapter);
    }
    if (instance != nullptr) {
        wgpuInstanceRelease(instance);
    }
}

// The callbacks run spontaneously, when the browser settles its promises between frames, and the adapter of a browser without WebGPU fails before the request returns.
Connection::Status Connection::request(const DeviceFactory::Options& options) {
    if (status != Status::Idle) {
        return status;
    }

    pending.reset(new Connection());
    pending->instance = wgpuCreateInstance(nullptr);
    if (pending->instance == nullptr) {
        fail("The browser could not create a WebGPU instance.");
        return status;
    }
    status = Status::Pending;

    WGPURequestAdapterOptions adapterOptions = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
    adapterOptions.powerPreference = options.lowPower ? WGPUPowerPreference_LowPower : WGPUPowerPreference_HighPerformance;
    adapterOptions.forceFallbackAdapter = options.software ? 1U : 0U;
    WGPURequestAdapterCallbackInfo callback = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    callback.mode = WGPUCallbackMode_AllowSpontaneous;
    callback.callback = &Connection::receiveAdapter;
    wgpuInstanceRequestAdapter(pending->instance, &adapterOptions, callback);
    return status;
}

std::unique_ptr<Connection> Connection::take() {
    switch (status) {
    case Status::Ready:
        status = Status::Idle;
        return std::move(pending);
    case Status::Failed:
        status = Status::Idle;
        throw std::runtime_error(std::exchange(failure, {}));
    case Status::Pending:
        throw std::runtime_error("WebGPU is still starting. The platform keeps running its frames until the request of the device is ready.");
    case Status::Idle:
        break;
    }
    throw std::runtime_error("WebGPU was not requested. The platform requests the device before it creates it.");
}

void Connection::fail(std::string reason) {
    pending.reset();
    failure = std::move(reason);
    status = Status::Failed;
}

void Connection::receiveAdapter(WGPURequestAdapterStatus result, WGPUAdapter found, WGPUStringView message, void*, void*) {
    if (status != Status::Pending) {
        return;
    }
    if (result != WGPURequestAdapterStatus_Success || found == nullptr) {
        fail(std::format("The browser offers no WebGPU adapter: {}", Conversion::fromText(message)));
        return;
    }
    pending->adapter = found;
    pending->requestDevice();
}

// The device asks for every limit the adapter reaches, since the shaders of the engine bind more textures than the defaults allow.
void Connection::requestDevice() {
    adapterName = describeAdapter(adapter);
    wgpuAdapterGetLimits(adapter, &limits);
    for (const WGPUFeatureName feature : kOptionalFeatures) {
        if (wgpuAdapterHasFeature(adapter, feature)) {
            features.push_back(feature);
        }
    }

    WGPUDeviceDescriptor descriptor = WGPU_DEVICE_DESCRIPTOR_INIT;
    descriptor.label = Conversion::toText("haylen-device");
    descriptor.requiredFeatureCount = features.size();
    descriptor.requiredFeatures = features.data();
    descriptor.requiredLimits = &limits;
    descriptor.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    descriptor.deviceLostCallbackInfo.callback = &Connection::reportLoss;
    descriptor.deviceLostCallbackInfo.userdata1 = new std::shared_ptr<Events>(events);
    descriptor.uncapturedErrorCallbackInfo.callback = &Connection::reportError;
    descriptor.uncapturedErrorCallbackInfo.userdata1 = events.get();

    WGPURequestDeviceCallbackInfo callback = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    callback.mode = WGPUCallbackMode_AllowSpontaneous;
    callback.callback = &Connection::receiveDevice;
    wgpuAdapterRequestDevice(adapter, &descriptor, callback);
}

void Connection::receiveDevice(WGPURequestDeviceStatus result, WGPUDevice created, WGPUStringView message, void*, void*) {
    if (status != Status::Pending) {
        return;
    }
    if (result != WGPURequestDeviceStatus_Success || created == nullptr) {
        fail(std::format("The WebGPU adapter refused to create a device: {}", Conversion::fromText(message)));
        return;
    }
    pending->device = created;
    pending->queue = wgpuDeviceGetQueue(created);
    status = Status::Ready;
}

// A device that the engine destroys, or that never came to be, reports its loss too, which only releases what the callbacks share.
void Connection::reportLoss(WGPUDevice const*, WGPUDeviceLostReason reason, WGPUStringView message, void* first, void*) {
    const std::unique_ptr<std::shared_ptr<Events>> events(static_cast<std::shared_ptr<Events>*>(first));
    (*events)->lost = true;
    if (reason == WGPUDeviceLostReason_Unknown) {
        core::Log::error("The WebGPU device was lost: {}", Conversion::fromText(message));
    }
}

void Connection::reportError(WGPUDevice const*, WGPUErrorType, WGPUStringView message, void* first, void*) {
    ++static_cast<Events*>(first)->errors;
    core::Log::error("WebGPU reported an error: {}", Conversion::fromText(message));
}

// Browsers keep the name of the GPU to themselves unless the page asks for it, so the name falls back on the vendor and the architecture they report.
std::string Connection::describeAdapter(WGPUAdapter source) {
    WGPUAdapterInfo info = WGPU_ADAPTER_INFO_INIT;
    if (wgpuAdapterGetInfo(source, &info) != WGPUStatus_Success) {
        return {};
    }

    std::string name(Conversion::fromText(info.description));
    if (name.empty()) {
        name = Conversion::fromText(info.device);
    }
    if (name.empty()) {
        const std::string_view vendor = Conversion::fromText(info.vendor);
        const std::string_view architecture = Conversion::fromText(info.architecture);
        name = architecture.empty() ? std::string(vendor) : std::format("{} {}", vendor, architecture);
    }
    wgpuAdapterInfoFreeMembers(info);
    return name;
}

WGPUInstance Connection::getInstance() const noexcept {
    return instance;
}

WGPUAdapter Connection::getAdapter() const noexcept {
    return adapter;
}

WGPUDevice Connection::getDevice() const noexcept {
    return device;
}

WGPUQueue Connection::getQueue() const noexcept {
    return queue;
}

const WGPULimits& Connection::getLimits() const noexcept {
    return limits;
}

bool Connection::hasFeature(WGPUFeatureName feature) const noexcept {
    return std::ranges::find(features, feature) != features.end();
}

const std::string& Connection::getAdapterName() const noexcept {
    return adapterName;
}

bool Connection::isLost() const noexcept {
    return events->lost;
}

int Connection::getErrorCount() const noexcept {
    return events->errors;
}

} // namespace haylen::gpu::webgpu
