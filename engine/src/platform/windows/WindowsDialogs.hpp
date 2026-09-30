#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "haylen/platform/DialogRequest.hpp"
#include "haylen/platform/DialogResult.hpp"

struct IFileDialog;
struct IShellItem;

namespace haylen::platform {

// The native dialogs of Windows, which a thread of their own shows one after the other, owned by the window of the app, so the frame thread never waits for them. The thread lives in a single-threaded COM apartment with a message-only window, which receives the requests and closes a dialog that the app gave up, even while a dialog runs its modal loop.
class WindowsDialogs final {
  public:
    static void show(std::uint64_t id, const DialogRequest& request);

    // Closes a dialog that the app gave up, or forgets it while it still waits for its turn.
    static void cancel(std::uint64_t id);

  private:
    static constexpr UINT kShowMessage = WM_APP + 1;
    static constexpr UINT kCancelMessage = WM_APP + 2;
    static constexpr int kFirstButton = 100;
    static constexpr const wchar_t* kWindowClass = L"HaylenDialogs";

    struct Queued {
        std::uint64_t id = 0;
        DialogRequest request;
        HWND owner = nullptr;
        std::wstring ownerTitle;
    };

    // The dialog that shows, with the window of a task dialog once it exists or the file dialog that shows.
    struct Active {
        std::uint64_t id = 0;
        bool cancelled = false;
        HWND taskDialog = nullptr;
        IFileDialog* fileDialog = nullptr;
    };

    static std::mutex& mutex;
    static std::deque<Queued>& queue;
    static Active active;
    static HWND window;
    static bool started;
    static bool showing;

    static void run();
    static void showQueued();
    static void closeActive();
    [[nodiscard]] static DialogResult showDialog(const Queued& queued);
    [[nodiscard]] static DialogResult showMessage(const Queued& queued, const DialogRequest::Message& message);
    [[nodiscard]] static DialogResult showOpenFiles(HWND owner, const DialogRequest::OpenFiles& files);
    [[nodiscard]] static DialogResult showSaveFile(HWND owner, const DialogRequest::SaveFile& save);
    [[nodiscard]] static DialogResult showOpenFolder(HWND owner, const DialogRequest::OpenFolder& folder);
    static void prepare(IFileDialog& dialog, const std::string& title, const std::vector<DialogRequest::Filter>& filters);
    [[nodiscard]] static HRESULT showFileDialog(IFileDialog& dialog, HWND owner);
    [[nodiscard]] static std::wstring readPath(IShellItem& item);
    [[nodiscard]] static DialogResult makeFailure(const std::string& message, HRESULT result);
    static HRESULT CALLBACK taskDialogCallback(HWND dialog, UINT notification, WPARAM wParam, LPARAM lParam, LONG_PTR data);
    static LRESULT CALLBACK windowProcedure(HWND handle, UINT message, WPARAM wParam, LPARAM lParam);
};

} // namespace haylen::platform
