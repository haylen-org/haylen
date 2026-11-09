#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace haylen::platform {

// Hands the links that Windows opens to the instance of the app that runs. Windows starts a new process for every link of a URL scheme that the registry names, with the link after `--link`, so the new process writes the link into the named pipe of the app in the session of the person, and the instance that runs reads it there on a thread of its own and reports it to `LinkRelay`.
class WindowsLinks final {
  public:
    [[nodiscard]] static bool forward(std::string_view identifier, std::string_view url);
    static void listen(std::string_view identifier);

  private:
    static constexpr DWORD kMaxLinkBytes = 64 * 1024;
    static constexpr DWORD kBusyMilliseconds = 2000;

    // The pipe of the app in the session of the person, so the apps of other sessions never take its links.
    [[nodiscard]] static std::wstring getPipeName(std::string_view identifier);
    static void serve(const std::wstring& name);
};

} // namespace haylen::platform
