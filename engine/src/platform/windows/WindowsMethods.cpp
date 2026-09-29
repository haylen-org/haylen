#include "platform/windows/WindowsMethods.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <shellapi.h>

#include <algorithm>
#include <array>
#include <stdexcept>

namespace haylen::platform {

core::Json WindowsMethods::getDeviceInfo() const {
    return {{"model", "PC"}, {"system", "Windows"}, {"systemVersion", getSystemVersion()}, {"locale", getLocale()}};
}

std::string WindowsMethods::getLocale() const {
    std::array<wchar_t, LOCALE_NAME_MAX_LENGTH> name{};
    GetUserDefaultLocaleName(name.data(), static_cast<int>(name.size()));
    return narrow(name.data());
}

void WindowsMethods::openUrl(const std::string& url, std::function<void(bool opened)> done) {
    done(reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32);
}

// Reads the real version from ntdll, because GetVersionEx reports an older one to applications without a compatibility manifest.
std::string WindowsMethods::getSystemVersion() {
    using RtlGetVersion = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    const auto readVersion = reinterpret_cast<RtlGetVersion>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
    RTL_OSVERSIONINFOW info{};
    info.dwOSVersionInfoSize = sizeof(info);
    if (readVersion == nullptr || readVersion(&info) != 0) {
        throw std::runtime_error("The Windows version could not be read.");
    }
    return std::to_string(info.dwMajorVersion) + "." + std::to_string(info.dwMinorVersion) + "." + std::to_string(info.dwBuildNumber);
}

std::string WindowsMethods::narrow(const wchar_t* text) {
    const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(std::max(0, size - 1)), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring WindowsMethods::widen(const std::string& text) {
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring result(static_cast<std::size_t>(std::max(0, size - 1)), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, result.data(), size);
    return result;
}

} // namespace haylen::platform
