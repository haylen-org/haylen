#pragma once

#include <windows.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace haylen::gpu::d3d12 {

// Captures the frames of the Direct3D 12 device through the programmatic capture of PIX, which writes `.wpix` files, when the GPU capturer of PIX is in the process, as when PIX launched the app for a GPU capture. The engine never loads the capturer itself, since it hooks Direct3D before the device exists.
class PixCapturer final {
  public:
    // The capturer that the process holds, or null when it holds none.
    [[nodiscard]] static std::unique_ptr<PixCapturer> find();

    // Starts capturing into the file of the path with the extension `.wpix`. Throws `std::logic_error` while a capture records, and `std::runtime_error` when PIX does not start.
    void begin(const std::filesystem::path& path);

    // Ends the capture and returns the path of its file. Throws `std::logic_error` when no capture records, and `std::runtime_error` when PIX fails to end it.
    [[nodiscard]] std::filesystem::path end();

  private:
    // The parameters of a capture take the name of its file first, within the room of the largest kind of capture that PIX describes the same way.
    struct Parameters {
        const wchar_t* fileName = nullptr;
        std::array<std::uint32_t, 14> others{};
    };

    using BeginCapture = HRESULT(WINAPI*)(const Parameters* parameters);
    using EndCapture = HRESULT(WINAPI*)();

    PixCapturer(BeginCapture beginFunction, EndCapture endFunction) noexcept : beginCapture(beginFunction), endCapture(endFunction) {}

    BeginCapture beginCapture;
    EndCapture endCapture;
    std::filesystem::path document;
    std::wstring fileName;
    bool capturing = false;
};

} // namespace haylen::gpu::d3d12
