#include "gpu/direct3d/WideText.hpp"

#include <windows.h>

namespace haylen::gpu::direct3d {

std::wstring WideText::toWide(std::string_view text) {
    WideText converter;
    return {converter.convert(text)};
}

std::string WideText::toUtf8(std::wstring_view text) {
    if (text.empty()) {
        return {};
    }
    const auto length = static_cast<int>(text.size());
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), length, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), length, result.data(), size, nullptr, nullptr);
    return result;
}

const wchar_t* WideText::convert(std::string_view text) {
    if (text.empty()) {
        buffer.clear();
        return buffer.c_str();
    }
    const auto length = static_cast<int>(text.size());
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), length, nullptr, 0);
    buffer.resize(static_cast<std::size_t>(size));
    MultiByteToWideChar(CP_UTF8, 0, text.data(), length, buffer.data(), size);
    return buffer.c_str();
}

} // namespace haylen::gpu::direct3d
