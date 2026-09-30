#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "haylen/platform/DialogRequest.hpp"
#include "haylen/platform/DialogResult.hpp"

namespace haylen::platform {

// The native dialogs of Linux through GTK 3, which the engine loads with the first dialog, so apps run on systems without it, where every dialog fails with the code `unsupported`. A message is a GTK message dialog over the window of the app, and the pickers are GTK file choosers, which the desktop portal shows where it runs. GTK runs on the frame thread, where the dialogs answer while the main context of GLib runs, and a save writes its data on a thread of its own.
class LinuxDialogs final {
  public:
    static void show(std::uint64_t id, const DialogRequest& request);

    // Closes a dialog that the app gave up, which answers nothing.
    static void cancel(std::uint64_t id);

  private:
    static constexpr int kInfoMessage = 0;
    static constexpr int kWarningMessage = 1;
    static constexpr int kErrorMessage = 3;
    static constexpr int kModal = 1;
    static constexpr int kNoButtons = 0;
    static constexpr int kAccept = -3;
    static constexpr int kOpenAction = 0;
    static constexpr int kSaveAction = 1;
    static constexpr int kFolderAction = 2;

    // A dialog that shows: a message, which is a widget, or a file chooser of an action, which is an object of its own and keeps the data of a save.
    struct Shown {
        void* dialog = nullptr;
        bool chooser = false;
        int action = kOpenAction;
        std::vector<std::uint8_t> data;
    };

    // A list of GLib, which the file choosers return their files in.
    struct List {
        void* data;
        List* next;
    };

    // The functions of GTK 3, GDK and GObject that the dialogs use.
    struct Gtk {
        void (*setAllowedBackends)(const char* backends);
        void (*disableSetlocale)();
        int (*initCheck)(int* argc, char*** argv);
        void* (*newMessageDialog)(void* parent, int flags, int type, int buttons, const char* format, ...);
        void (*setSecondaryText)(void* dialog, const char* format, ...);
        void* (*addButton)(void* dialog, const char* text, int response);
        void (*setDefaultResponse)(void* dialog, int response);
        void (*realize)(void* widget);
        void* (*getWindow)(void* widget);
        void (*showWidget)(void* widget);
        void (*destroyWidget)(void* widget);
        void* (*getDefaultDisplay)();
        void* (*newForeignWindow)(void* display, unsigned long window);
        void (*setTransientFor)(void* window, void* parent);
        void* (*newChooser)(const char* title, void* parent, int action, const char* acceptLabel, const char* cancelLabel);
        void (*setChooserModal)(void* chooser, int modal);
        void (*showChooser)(void* chooser);
        void (*destroyChooser)(void* chooser);
        void (*setSelectMultiple)(void* chooser, int multiple);
        void (*setCurrentName)(void* chooser, const char* name);
        void (*setOverwriteConfirmation)(void* chooser, int confirm);
        List* (*getFilenames)(void* chooser);
        void (*addFilter)(void* chooser, void* filter);
        void* (*newFilter)();
        void (*setFilterName)(void* filter, const char* name);
        void (*addPattern)(void* filter, const char* pattern);
        unsigned long (*connect)(void* instance, const char* signal, void (*handler)(), void* data, void (*release)(void*, void*), int flags);
        void (*unref)(void* object);
        void (*freeMemory)(void* memory);
        void (*freeList)(List* list);
    };

    static Gtk gtk;
    static bool loaded;
    static std::optional<DialogResult::Failure>& failure;
    static void* parent;
    static std::unordered_map<std::uint64_t, Shown>& shown;

    [[nodiscard]] static bool load();
    [[nodiscard]] static bool findFunctions(void* library);
    static void showMessage(std::uint64_t id, const DialogRequest::Message& message);
    static void showChooser(std::uint64_t id, const DialogRequest& request);
    static void addFilters(void* chooser, const std::vector<DialogRequest::Filter>& filters);
    static void messageResponded(void* dialog, int response, void* data);
    static void chooserResponded(void* chooser, int response, void* data);
    [[nodiscard]] static std::optional<std::pair<std::uint64_t, Shown>> take(void* dialog);
    [[nodiscard]] static std::vector<std::string> readFiles(void* chooser);
    static void writeFile(std::uint64_t id, const std::filesystem::path& path, std::vector<std::uint8_t> data);
};

} // namespace haylen::platform
