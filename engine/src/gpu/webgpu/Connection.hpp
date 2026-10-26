#pragma once

#include <webgpu/webgpu.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace haylen::gpu::webgpu {

// The instance, the adapter, the device and the queue of WebGPU. The browser hands the adapter and the device over in later turns of its event loop, so the platform starts the request before the app starts, keeps running its frames while it is pending and creates the device once it is ready, which takes the connection. The web build has one thread, which every method runs on.
class Connection final {
  public:
    enum class Status : std::uint8_t {
        Idle,
        Pending,
        Ready,
        Failed,
    };

    // What the callbacks of the device report. The callback of its loss holds it until it runs, which is the last callback of a device, so the callback of its errors can point at it.
    struct Events {
        bool lost = false;
        int errors = 0;
    };

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    ~Connection();

    // Starts requesting the adapter and the device unless a request runs or ended, and returns where it stands. A failed request stays failed until `take` reports it.
    static Status request();

    // Hands over the ready connection, leaving none. Throws `std::runtime_error` with the reason when the request failed, still runs or never started.
    [[nodiscard]] static std::unique_ptr<Connection> take();

    [[nodiscard]] WGPUInstance getInstance() const noexcept;
    [[nodiscard]] WGPUAdapter getAdapter() const noexcept;
    [[nodiscard]] WGPUDevice getDevice() const noexcept;
    [[nodiscard]] WGPUQueue getQueue() const noexcept;
    [[nodiscard]] const WGPULimits& getLimits() const noexcept;
    [[nodiscard]] bool hasFeature(WGPUFeatureName feature) const noexcept;
    [[nodiscard]] const std::string& getAdapterName() const noexcept;
    [[nodiscard]] bool isLost() const noexcept;
    [[nodiscard]] int getErrorCount() const noexcept;

  private:
    // The optional features that the device turns on where the adapter has them: timing the GPU, filtering and blending 32-bit float images, and the depth format with a 32-bit float and stencil.
    static constexpr std::array<WGPUFeatureName, 4> kOptionalFeatures{
        WGPUFeatureName_TimestampQuery,
        WGPUFeatureName_Float32Filterable,
        WGPUFeatureName_Float32Blendable,
        WGPUFeatureName_Depth32FloatStencil8,
    };

    static std::unique_ptr<Connection> pending;
    static Status status;
    static std::string failure;

    Connection() = default;

    static void fail(std::string reason);
    static void receiveAdapter(WGPURequestAdapterStatus result, WGPUAdapter found, WGPUStringView message, void* first, void* second);
    static void receiveDevice(WGPURequestDeviceStatus result, WGPUDevice created, WGPUStringView message, void* first, void* second);
    static void reportLoss(WGPUDevice const* device, WGPUDeviceLostReason reason, WGPUStringView message, void* first, void* second);
    static void reportError(WGPUDevice const* device, WGPUErrorType type, WGPUStringView message, void* first, void* second);
    static std::string describeAdapter(WGPUAdapter source);

    void requestDevice();

    WGPUInstance instance = nullptr;
    WGPUAdapter adapter = nullptr;
    WGPUDevice device = nullptr;
    WGPUQueue queue = nullptr;
    WGPULimits limits = WGPU_LIMITS_INIT;
    std::vector<WGPUFeatureName> features;
    std::string adapterName;
    std::shared_ptr<Events> events = std::make_shared<Events>();
};

} // namespace haylen::gpu::webgpu
