#include "gpu/d3d12/PixCapturer.hpp"

#include <format>
#include <stdexcept>

namespace haylen::gpu::d3d12 {

std::unique_ptr<PixCapturer> PixCapturer::find() {
    const HMODULE library = GetModuleHandleW(L"WinPixGpuCapturer.dll");
    if (library == nullptr) {
        return nullptr;
    }
    const auto beginFunction = reinterpret_cast<BeginCapture>(reinterpret_cast<void*>(GetProcAddress(library, "BeginProgrammaticGpuCapture")));
    const auto endFunction = reinterpret_cast<EndCapture>(reinterpret_cast<void*>(GetProcAddress(library, "EndProgrammaticGpuCapture")));
    if (beginFunction == nullptr || endFunction == nullptr) {
        return nullptr;
    }
    return std::unique_ptr<PixCapturer>(new PixCapturer(beginFunction, endFunction));
}

void PixCapturer::begin(const std::filesystem::path& path) {
    if (capturing) {
        throw std::logic_error("A capture began while another one records.");
    }
    document = path;
    document += ".wpix";
    fileName = document.wstring();
    const Parameters parameters{.fileName = fileName.c_str()};
    const HRESULT result = beginCapture(&parameters);
    if (FAILED(result)) {
        throw std::runtime_error(std::format("PIX did not start the capture, with the error 0x{:08X}.", static_cast<std::uint32_t>(result)));
    }
    capturing = true;
}

std::filesystem::path PixCapturer::end() {
    if (!capturing) {
        throw std::logic_error("A capture ended while none records.");
    }
    capturing = false;
    const HRESULT result = endCapture();
    if (FAILED(result)) {
        throw std::runtime_error(std::format("PIX failed to write the capture, with the error 0x{:08X}.", static_cast<std::uint32_t>(result)));
    }
    return document;
}

} // namespace haylen::gpu::d3d12
