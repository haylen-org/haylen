#include "platform/windows/WindowsSystem.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <shellapi.h>

#include <array>
#include <cstddef>
#include <string_view>

#include "haylen/platform/Theme.hpp"
#include "platform/SystemState.hpp"
#include "platform/sokol/SokolHost.hpp"
#include "platform/windows/WindowsPowerStatus.hpp"
#include "platform/windows/WindowsText.hpp"

namespace haylen::platform {

SystemInfo WindowsSystem::getInfo() {
    return {
        .os = SystemInfo::Os::Windows,
        .osVersion = getSystemVersion(),
        .deviceModel = readMachineText(kBiosKey, L"SystemProductName"),
        .manufacturer = readMachineText(kBiosKey, L"SystemManufacturer"),
        .deviceKind = SystemInfo::DeviceKind::Desktop,
        .cpuName = readMachineText(kProcessorKey, L"ProcessorNameString"),
        .cpuCores = static_cast<int>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)),
        .memoryBytes = getMemory(),
        .locale = getLocale(),
        .languages = getLanguages(),
        .timeZone = getTimeZone(),
    };
}

void WindowsSystem::openUrl(const std::string& url, std::function<void(bool opened)> callback) {
    callback(reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", WindowsText::toWide(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32);
}

// Windows versions without the setting show light apps.
void WindowsSystem::reportTheme() {
    DWORD light = 1;
    DWORD size = sizeof(light);
    const LSTATUS status = RegGetValueW(HKEY_CURRENT_USER, kPersonalizeKey, L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
    SokolHost::getSystemState().setTheme(status == ERROR_SUCCESS && light == 0 ? Theme::Dark : Theme::Light);
}

void WindowsSystem::reportBattery() {
    SYSTEM_POWER_STATUS status{};
    if (GetSystemPowerStatus(&status)) {
        SokolHost::getSystemState().setBattery(WindowsPowerStatus::toBattery(status.ACLineStatus, status.BatteryFlag, status.BatteryLifePercent));
    }
}

// Reads the real version from `ntdll`, because `GetVersionEx` reports an older one to applications without a compatibility manifest.
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
    return WindowsText::toUtf8(name.data());
}

// The languages arrive as one buffer of texts that each end with a null, followed by one more null.
std::vector<std::string> WindowsSystem::getLanguages() {
    ULONG count = 0;
    ULONG size = 0;
    if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, nullptr, &size)) {
        return {};
    }
    std::wstring buffer(size, L'\0');
    if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, buffer.data(), &size)) {
        return {};
    }

    std::vector<std::string> languages;
    for (std::size_t start = 0; start < buffer.size() && buffer[start] != L'\0';) {
        const std::size_t end = buffer.find(L'\0', start);
        languages.push_back(WindowsText::toUtf8(std::wstring_view(buffer).substr(start, end - start)));
        start = end + 1;
    }
    return languages;
}

// Windows names its time zones in its own way, and the ICU that ships with Windows 10 1903 and later turns them into IANA names.
std::string WindowsSystem::getTimeZone() {
    using TimeZoneForWindowsId = std::int32_t(__cdecl*)(const wchar_t* windowsId, std::int32_t length, const char* region, wchar_t* id, std::int32_t capacity, int* status);
    DYNAMIC_TIME_ZONE_INFORMATION zone{};
    const HMODULE icu = LoadLibraryExW(L"icu.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (icu == nullptr || GetDynamicTimeZoneInformation(&zone) == TIME_ZONE_ID_INVALID) {
        return {};
    }
    const auto convert = reinterpret_cast<TimeZoneForWindowsId>(GetProcAddress(icu, "ucal_getTimeZoneIDForWindowsID"));
    if (convert == nullptr) {
        return {};
    }

    std::array<wchar_t, 128> id{};
    int status = 0;
    const std::int32_t length = convert(zone.TimeZoneKeyName, -1, nullptr, id.data(), static_cast<std::int32_t>(id.size()), &status);
    if (status > 0 || length <= 0) {
        return {};
    }
    return WindowsText::toUtf8(std::wstring_view(id.data(), static_cast<std::size_t>(length)));
}

std::uint64_t WindowsSystem::getMemory() {
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    return GlobalMemoryStatusEx(&memory) ? memory.ullTotalPhys : 0;
}

// Values of the registry may carry spaces around them, as the names of processors often do.
std::string WindowsSystem::readMachineText(const wchar_t* key, const wchar_t* name) {
    DWORD size = 0;
    if (RegGetValueW(HKEY_LOCAL_MACHINE, key, name, RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS) {
        return {};
    }
    std::wstring text(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_LOCAL_MACHINE, key, name, RRF_RT_REG_SZ, nullptr, text.data(), &size) != ERROR_SUCCESS) {
        return {};
    }

    const std::wstring_view value(text.c_str());
    const std::size_t first = value.find_first_not_of(L' ');
    if (first == std::wstring_view::npos) {
        return {};
    }
    return WindowsText::toUtf8(value.substr(first, value.find_last_not_of(L' ') - first + 1));
}

} // namespace haylen::platform
