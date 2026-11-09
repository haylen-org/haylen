#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>

namespace haylen::gpu::renderdoc {

// Captures the frames of the Vulkan, Direct3D and OpenGL devices through the in-application API of RenderDoc, which writes `.rdc` files, when the library of RenderDoc is in the process, as when RenderDoc launched the app. The engine never loads the library itself, since RenderDoc hooks the graphics API before the device exists.
class Capturer final {
  public:
    static constexpr std::string_view kMissing = "RenderDoc is not loaded into the app. Launch the app from RenderDoc, through its \"Launch Application\" panel or \"renderdoccmd capture\", which loads it before the graphics API starts, and capture again.";

    // The API of the RenderDoc library that the process holds, or null when it holds none.
    [[nodiscard]] static std::unique_ptr<Capturer> find();

    // Starts capturing the device, such as the `ID3D11Device` of Direct3D 11 or the dispatch table of a `VkInstance`, or every device for null, into a file that starts with the path. Throws `std::logic_error` while a capture records, and `std::runtime_error` when RenderDoc does not start.
    void begin(void* device, const std::filesystem::path& path);

    // Ends the capture of the device and returns the path of the file that RenderDoc wrote. Throws `std::logic_error` when no capture records, and `std::runtime_error` when RenderDoc wrote no file.
    [[nodiscard]] std::filesystem::path end(void* device);

  private:
    // The functions of version 1.1.2 of the API, in the order RenderDoc lays them out, with the ones the engine never calls as plain pointers.
    struct Functions {
        void* getApiVersion;
        void* setCaptureOptionU32;
        void* setCaptureOptionF32;
        void* getCaptureOptionU32;
        void* getCaptureOptionF32;
        void* setFocusToggleKeys;
        void* setCaptureKeys;
        void* getOverlayBits;
        void* maskOverlayBits;
        void* removeHooks;
        void* unloadCrashHandler;
        void (*setCaptureFilePathTemplate)(const char* pathTemplate);
        void* getCaptureFilePathTemplate;
        std::uint32_t (*getNumCaptures)();
        std::uint32_t (*getCapture)(std::uint32_t index, char* fileName, std::uint32_t* pathLength, std::uint64_t* timestamp);
        void* triggerCapture;
        void* isTargetControlConnected;
        void* launchReplayUi;
        void* setActiveWindow;
        void (*startFrameCapture)(void* device, void* window);
        std::uint32_t (*isFrameCapturing)();
        std::uint32_t (*endFrameCapture)(void* device, void* window);
    };

    static constexpr int kVersion = 10102;

    explicit Capturer(const Functions& api) noexcept : functions(api) {}

    // Returns the address of `RENDERDOC_GetAPI` in the library of RenderDoc that the process holds, or null.
    [[nodiscard]] static void* findEntryPoint();

    const Functions& functions;
    std::uint32_t capturesBefore = 0;
    bool capturing = false;
};

} // namespace haylen::gpu::renderdoc
