#include "platform/windows/WindowsLinks.hpp"

#include <thread>
#include <utility>

#include "haylen/core/Log.hpp"
#include "platform/LinkRelay.hpp"
#include "platform/windows/WindowsText.hpp"

namespace haylen::platform {

// A pipe that is busy with another link takes this one once it is free. The instance that runs may bring its window to the front for the link.
bool WindowsLinks::forward(std::string_view identifier, std::string_view url) {
    const std::wstring name = getPipeName(identifier);
    HANDLE pipe = CreateFileW(name.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PIPE_BUSY && WaitNamedPipeW(name.c_str(), kBusyMilliseconds)) {
        pipe = CreateFileW(name.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    }
    if (pipe == INVALID_HANDLE_VALUE) {
        return false;
    }

    AllowSetForegroundWindow(ASFW_ANY);
    DWORD written = 0;
    const BOOL sent = WriteFile(pipe, url.data(), static_cast<DWORD>(url.size()), &written, nullptr);
    CloseHandle(pipe);
    return sent && written == url.size();
}

void WindowsLinks::listen(std::string_view identifier) {
    std::thread(&WindowsLinks::serve, getPipeName(identifier)).detach();
}

std::wstring WindowsLinks::getPipeName(std::string_view identifier) {
    DWORD session = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &session);
    return L"\\\\.\\pipe\\haylen-links-" + WindowsText::toWide(identifier) + L"-" + std::to_wstring(session);
}

// The first instance of the pipe belongs to the first app that runs, and the next instance waits before the one that read a link closes, so a link always finds the pipe.
void WindowsLinks::serve(const std::wstring& name) {
    constexpr DWORD kMode = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS;
    HANDLE pipe = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE, kMode, PIPE_UNLIMITED_INSTANCES, 0, kMaxLinkBytes, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        core::Log::warning("The app takes no links of later processes, because another instance of it takes them already.");
        return;
    }
    for (;;) {
        const bool connected = ConnectNamedPipe(pipe, nullptr) != FALSE || GetLastError() == ERROR_PIPE_CONNECTED;
        std::string url(kMaxLinkBytes, '\0');
        DWORD read = 0;
        if (connected && ReadFile(pipe, url.data(), kMaxLinkBytes, &read, nullptr) && read > 0) {
            url.resize(read);
            LinkRelay::receive(std::move(url));
        }

        HANDLE next = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_INBOUND, kMode, PIPE_UNLIMITED_INSTANCES, 0, kMaxLinkBytes, 0, nullptr);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        if (next == INVALID_HANDLE_VALUE) {
            core::Log::warning("The app takes no more links of later processes, because Windows refused the pipe for them.");
            return;
        }
        pipe = next;
    }
}

} // namespace haylen::platform
