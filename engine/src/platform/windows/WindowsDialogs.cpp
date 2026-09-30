#include "platform/windows/WindowsDialogs.hpp"

#include <commctrl.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>

#include "platform/DialogRelay.hpp"
#include "platform/windows/WindowsText.hpp"
#include "sokol_app.h"

namespace haylen::platform {

std::mutex& WindowsDialogs::mutex = *new std::mutex();
std::deque<WindowsDialogs::Queued>& WindowsDialogs::queue = *new std::deque<Queued>();
WindowsDialogs::Active WindowsDialogs::active{};
HWND WindowsDialogs::window = nullptr;
bool WindowsDialogs::started = false;
bool WindowsDialogs::showing = false;

// The thread starts with the first dialog and shows what waits once its window exists, so a request never needs the window before it does.
void WindowsDialogs::show(std::uint64_t id, const DialogRequest& request) {
    const auto owner = static_cast<HWND>(const_cast<void*>(sapp_win32_get_hwnd()));
    std::wstring ownerTitle(static_cast<std::size_t>(GetWindowTextLengthW(owner)) + 1, L'\0');
    ownerTitle.resize(static_cast<std::size_t>(GetWindowTextW(owner, ownerTitle.data(), static_cast<int>(ownerTitle.size()))));
    HWND target = nullptr;
    {
        const std::scoped_lock lock(mutex);
        queue.push_back({.id = id, .request = request, .owner = owner, .ownerTitle = std::move(ownerTitle)});
        target = window;
    }

    if (!started) {
        started = true;
        std::thread(run).detach();
    } else if (target != nullptr) {
        PostMessageW(target, kShowMessage, 0, 0);
    }
}

void WindowsDialogs::cancel(std::uint64_t id) {
    const std::scoped_lock lock(mutex);
    if (std::erase_if(queue, [id](const Queued& queued) { return queued.id == id; }) > 0 || active.id != id) {
        return;
    }
    active.cancelled = true;
    PostMessageW(window, kCancelMessage, 0, 0);
}

void WindowsDialogs::run() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = kWindowClass;
    RegisterClassExW(&windowClass);
    const HWND created = CreateWindowExW(0, kWindowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, windowClass.hInstance, nullptr);
    {
        const std::scoped_lock lock(mutex);
        window = created;
    }

    showQueued();
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

// The modal loop of a dialog dispatches the requests that arrive meanwhile, which wait for the dialog to close instead of showing over it.
void WindowsDialogs::showQueued() {
    if (showing) {
        return;
    }
    showing = true;
    for (;;) {
        std::unique_lock lock(mutex);
        if (queue.empty()) {
            break;
        }
        const Queued next = std::move(queue.front());
        queue.pop_front();
        active = {.id = next.id};
        lock.unlock();

        DialogResult result = showDialog(next);
        lock.lock();
        active = {};
        lock.unlock();
        DialogRelay::resolve(next.id, std::move(result));
    }
    showing = false;
}

// A task dialog closes as if the user pressed Escape and a file dialog as if the user cancelled it. A task dialog that the app gave up before its window existed closes once it does.
void WindowsDialogs::closeActive() {
    HWND taskDialog = nullptr;
    IFileDialog* fileDialog = nullptr;
    {
        const std::scoped_lock lock(mutex);
        if (!active.cancelled) {
            return;
        }
        taskDialog = active.taskDialog;
        fileDialog = active.fileDialog;
    }
    if (taskDialog != nullptr) {
        SendMessageW(taskDialog, TDM_CLICK_BUTTON, IDCANCEL, 0);
    } else if (fileDialog != nullptr) {
        fileDialog->Close(HRESULT_FROM_WIN32(ERROR_CANCELLED));
    }
}

DialogResult WindowsDialogs::showDialog(const Queued& queued) {
    if (const auto* message = std::get_if<DialogRequest::Message>(&queued.request.dialog)) {
        return showMessage(queued, *message);
    }
    if (const auto* files = std::get_if<DialogRequest::OpenFiles>(&queued.request.dialog)) {
        return showOpenFiles(queued.owner, *files);
    }
    if (const auto* save = std::get_if<DialogRequest::SaveFile>(&queued.request.dialog)) {
        return showSaveFile(queued.owner, *save);
    }
    return showOpenFolder(queued.owner, std::get<DialogRequest::OpenFolder>(queued.request.dialog));
}

// The task dialog lives in version 6 of the Common Controls, which the application manifest selects, so it is looked up when a message shows. Escape and the close button dismiss it without a button.
DialogResult WindowsDialogs::showMessage(const Queued& queued, const DialogRequest::Message& message) {
    const HMODULE controls = LoadLibraryW(L"comctl32.dll");
    const auto showTaskDialog = controls != nullptr ? reinterpret_cast<decltype(&TaskDialogIndirect)>(GetProcAddress(controls, "TaskDialogIndirect")) : nullptr;
    if (showTaskDialog == nullptr) {
        return {.failure = DialogResult::Failure{.code = DialogResult::Code::Failed, .message = "Message dialogs need version 6 of the Common Controls, which the application manifest of Haylen apps selects."}};
    }

    const std::wstring title = WindowsText::toWide(message.title);
    const std::wstring text = WindowsText::toWide(message.text);
    std::vector<std::wstring> labels;
    std::vector<TASKDIALOG_BUTTON> buttons;
    labels.reserve(message.buttons.size());
    for (const std::string& label : message.buttons) {
        labels.push_back(WindowsText::toWide(label));
        buttons.push_back({.nButtonID = kFirstButton + static_cast<int>(buttons.size()), .pszButtonText = labels.back().c_str()});
    }

    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof(config);
    config.hwndParent = queued.owner;
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    config.pszWindowTitle = queued.ownerTitle.empty() ? nullptr : queued.ownerTitle.c_str();
    switch (message.kind) {
    case DialogRequest::MessageKind::Info:
        config.pszMainIcon = TD_INFORMATION_ICON;
        break;
    case DialogRequest::MessageKind::Warning:
        config.pszMainIcon = TD_WARNING_ICON;
        break;
    case DialogRequest::MessageKind::Error:
        config.pszMainIcon = TD_ERROR_ICON;
        break;
    }
    config.pszMainInstruction = title.empty() ? nullptr : title.c_str();
    config.pszContent = text.c_str();
    config.cButtons = static_cast<UINT>(buttons.size());
    config.pButtons = buttons.data();
    config.nDefaultButton = kFirstButton;
    config.pfCallback = taskDialogCallback;

    int pressed = 0;
    const HRESULT result = showTaskDialog(&config, &pressed, nullptr, nullptr);
    if (FAILED(result)) {
        return makeFailure("The message could not be shown.", result);
    }
    const int index = pressed - kFirstButton;
    if (index < 0 || index >= static_cast<int>(buttons.size())) {
        return {};
    }
    return {.button = static_cast<std::size_t>(index)};
}

DialogResult WindowsDialogs::showOpenFiles(HWND owner, const DialogRequest::OpenFiles& files) {
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    HRESULT result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.GetAddressOf()));
    if (FAILED(result)) {
        return makeFailure("The file dialog could not be created.", result);
    }
    FILEOPENDIALOGOPTIONS options = 0;
    dialog->GetOptions(&options);
    options |= FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST;
    if (files.multiple) {
        options |= FOS_ALLOWMULTISELECT;
    }
    dialog->SetOptions(options);
    prepare(*dialog.Get(), files.title, files.filters);

    result = showFileDialog(*dialog.Get(), owner);
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        return {};
    }
    Microsoft::WRL::ComPtr<IShellItemArray> items;
    DWORD count = 0;
    if (SUCCEEDED(result)) {
        result = dialog->GetResults(items.GetAddressOf());
    }
    if (SUCCEEDED(result)) {
        result = items->GetCount(&count);
    }
    if (FAILED(result)) {
        return makeFailure("The file dialog failed.", result);
    }

    DialogResult answer;
    for (DWORD index = 0; index < count; ++index) {
        Microsoft::WRL::ComPtr<IShellItem> item;
        if (SUCCEEDED(items->GetItemAt(index, item.GetAddressOf()))) {
            const std::filesystem::path path = readPath(*item.Get());
            answer.files.push_back({.name = WindowsText::toUtf8(path.filename().native()), .path = WindowsText::toUtf8(path.native())});
        }
    }
    return answer;
}

// The dialog adds the extension of the chosen type to a name typed without one, and asks before it replaces a file.
DialogResult WindowsDialogs::showSaveFile(HWND owner, const DialogRequest::SaveFile& save) {
    Microsoft::WRL::ComPtr<IFileSaveDialog> dialog;
    HRESULT result = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.GetAddressOf()));
    if (FAILED(result)) {
        return makeFailure("The file dialog could not be created.", result);
    }
    FILEOPENDIALOGOPTIONS options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT);
    prepare(*dialog.Get(), save.title, save.filters);
    dialog->SetFileName(WindowsText::toWide(save.name).c_str());
    if (!save.filters.empty()) {
        dialog->SetDefaultExtension(WindowsText::toWide(save.filters.front().extensions.front()).c_str());
    }

    result = showFileDialog(*dialog.Get(), owner);
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        return {};
    }
    Microsoft::WRL::ComPtr<IShellItem> item;
    if (SUCCEEDED(result)) {
        result = dialog->GetResult(item.GetAddressOf());
    }
    if (FAILED(result)) {
        return makeFailure("The file dialog failed.", result);
    }

    const std::filesystem::path path = readPath(*item.Get());
    const std::string name = WindowsText::toUtf8(path.filename().native());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(save.data.data()), static_cast<std::streamsize>(save.data.size()));
    stream.close();
    if (!stream) {
        return {.failure = DialogResult::Failure{.code = DialogResult::Code::Failed, .message = std::format("The file \"{}\" could not be written.", name)}};
    }
    return {.saved = DialogResult::File{.name = name, .path = WindowsText::toUtf8(path.native())}};
}

DialogResult WindowsDialogs::showOpenFolder(HWND owner, const DialogRequest::OpenFolder& folder) {
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    HRESULT result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.GetAddressOf()));
    if (FAILED(result)) {
        return makeFailure("The folder dialog could not be created.", result);
    }
    FILEOPENDIALOGOPTIONS options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    prepare(*dialog.Get(), folder.title, {});

    result = showFileDialog(*dialog.Get(), owner);
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        return {};
    }
    Microsoft::WRL::ComPtr<IShellItem> item;
    if (SUCCEEDED(result)) {
        result = dialog->GetResult(item.GetAddressOf());
    }
    if (FAILED(result)) {
        return makeFailure("The folder dialog failed.", result);
    }
    return {.folder = WindowsText::toUtf8(readPath(*item.Get()))};
}

// The dialog copies the names and patterns of the file types, so they only live while it takes them.
void WindowsDialogs::prepare(IFileDialog& dialog, const std::string& title, const std::vector<DialogRequest::Filter>& filters) {
    if (!title.empty()) {
        dialog.SetTitle(WindowsText::toWide(title).c_str());
    }
    std::vector<std::wstring> names;
    std::vector<std::wstring> patterns;
    for (const DialogRequest::Filter& filter : filters) {
        names.push_back(WindowsText::toWide(filter.name));
        std::wstring pattern;
        for (const std::string& extension : filter.extensions) {
            pattern += (pattern.empty() ? L"*." : L";*.") + WindowsText::toWide(extension);
        }
        patterns.push_back(std::move(pattern));
    }

    std::vector<COMDLG_FILTERSPEC> types;
    for (std::size_t index = 0; index < names.size(); ++index) {
        types.push_back({.pszName = names[index].c_str(), .pszSpec = patterns[index].c_str()});
    }
    if (!types.empty()) {
        dialog.SetFileTypes(static_cast<UINT>(types.size()), types.data());
    }
}

// The dialog becomes the active one while it shows, so a cancel that arrives during its modal loop closes it.
HRESULT WindowsDialogs::showFileDialog(IFileDialog& dialog, HWND owner) {
    {
        const std::scoped_lock lock(mutex);
        if (active.cancelled) {
            return HRESULT_FROM_WIN32(ERROR_CANCELLED);
        }
        active.fileDialog = &dialog;
    }
    const HRESULT result = dialog.Show(owner);
    const std::scoped_lock lock(mutex);
    active.fileDialog = nullptr;
    return result;
}

std::wstring WindowsDialogs::readPath(IShellItem& item) {
    PWSTR path = nullptr;
    if (FAILED(item.GetDisplayName(SIGDN_FILESYSPATH, &path))) {
        return {};
    }
    std::wstring text(path);
    CoTaskMemFree(path);
    return text;
}

DialogResult WindowsDialogs::makeFailure(const std::string& message, HRESULT result) {
    return {.failure = DialogResult::Failure{.code = DialogResult::Code::Failed, .message = std::format("{} Windows reported \"{}\".", message, std::system_category().message(static_cast<int>(result)))}};
}

HRESULT CALLBACK WindowsDialogs::taskDialogCallback(HWND dialog, UINT notification, WPARAM, LPARAM, LONG_PTR) {
    const std::scoped_lock lock(mutex);
    switch (notification) {
    case TDN_CREATED:
        active.taskDialog = dialog;
        if (active.cancelled) {
            PostMessageW(dialog, TDM_CLICK_BUTTON, IDCANCEL, 0);
        }
        break;
    case TDN_DESTROYED:
        active.taskDialog = nullptr;
        break;
    default:
        break;
    }
    return S_OK;
}

LRESULT CALLBACK WindowsDialogs::windowProcedure(HWND handle, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case kShowMessage:
        showQueued();
        return 0;
    case kCancelMessage:
        closeActive();
        return 0;
    default:
        return DefWindowProcW(handle, message, wParam, lParam);
    }
}

} // namespace haylen::platform
