#include "platform/windows/WindowsSystem.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <shellapi.h>

#include <algorithm>
#include <array>
#include <thread>

namespace haylen::platform {

SystemInfo WindowsSystem::getInfo() {
    SystemInfo info;
    info.os = SystemInfo::Os::Windows;
    info.deviceKind = SystemInfo::DeviceKind::Desktop;
    info.osVersion = getSystemVersion();
    info.cpuCores = static_cast<int>(std::thread::hardware_concurrency());
    info.locale = getLocale();
    return info;
}

void WindowsSystem::openUrl(const std::string& url, std::function<void(bool opened)> callback) {
    callback(reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32);
}

// Reads the real version from ntdll, because GetVersionEx reports an older one to applications without a compatibility manifest.
std::string WindowsSystem::getSystemVersion() {
    using RtlGetVersion = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    const auto readVersion = reinterpret_cast<RtlGetVersion>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
    RTL_OSVERSIONINFOW info{};
    info.dwOSVersionInfoSize = sizeof(info);
    if (readVersion == nullptr || readVersion(&info) != 0) {
        return {};
    }
    return std::to_string(info.dwMajorVersion) + "." + std::to_string(info.dwMinorVersion) + "." + std::to_string(info.dwBuildNumber);
}

std::string WindowsSystem::getLocale() {
    std::array<wchar_t, LOCALE_NAME_MAX_LENGTH> name{};
    GetUserDefaultLocaleName(name.data(), static_cast<int>(name.size()));
    return narrow(name.data());
}

std::string WindowsSystem::narrow(const wchar_t* text) {
    const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(std::max(0, size - 1)), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring WindowsSystem::widen(const std::string& text) {
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring result(static_cast<std::size_t>(std::max(0, size - 1)), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, result.data(), size);
    return result;
}

} // namespace haylen::platform
