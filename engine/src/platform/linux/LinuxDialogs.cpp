#include "platform/linux/LinuxDialogs.hpp"

#include <dlfcn.h>

#include <format>
#include <fstream>
#include <thread>
#include <variant>

#include "platform/DialogRelay.hpp"
#include "platform/linux/LinuxFilePattern.hpp"
#include "platform/linux/LinuxGlib.hpp"
#include "sokol_app.h"

namespace haylen::platform {

LinuxDialogs::Gtk LinuxDialogs::gtk{};
bool LinuxDialogs::loaded = false;
std::optional<DialogResult::Failure>& LinuxDialogs::failure = *new std::optional<DialogResult::Failure>();
void* LinuxDialogs::parent = nullptr;
std::unordered_map<std::uint64_t, LinuxDialogs::Shown>& LinuxDialogs::shown = *new std::unordered_map<std::uint64_t, Shown>();

void LinuxDialogs::show(std::uint64_t id, const DialogRequest& request) {
    if (!load()) {
        DialogRelay::resolve(id, {.failure = failure});
        return;
    }
    if (const auto* message = std::get_if<DialogRequest::Message>(&request.dialog)) {
        showMessage(id, *message);
        return;
    }
    showChooser(id, request);
}

// A message is a widget, which GTK destroys, while a file chooser is an object that the dialogs own.
void LinuxDialogs::cancel(std::uint64_t id) {
    const auto found = shown.find(id);
    if (found == shown.end()) {
        return;
    }
    const Shown dialog = std::move(found->second);
    shown.erase(found);
    if (dialog.chooser) {
        gtk.destroyChooser(dialog.dialog);
        gtk.unref(dialog.dialog);
    } else {
        gtk.destroyWidget(dialog.dialog);
    }
}

// GTK draws with X11 like the window of the app, even in a Wayland session, so its dialogs can stay over that window, and it leaves the locale of the process alone, which Lua formats numbers with.
bool LinuxDialogs::load() {
    if (loaded || failure) {
        return loaded;
    }
    void* library = LinuxGlib::open() != nullptr ? dlopen("libgtk-3.so.0", RTLD_NOW | RTLD_LOCAL) : nullptr;
    if (library == nullptr) {
        failure = DialogResult::Failure{.code = DialogResult::Code::Unsupported, .message = "Native dialogs on Linux need GTK 3, and the library \"libgtk-3.so.0\" could not be loaded."};
        return false;
    }
    if (!findFunctions(library)) {
        failure = DialogResult::Failure{.code = DialogResult::Code::Unsupported, .message = "Native dialogs on Linux need GTK 3.20 or later."};
        return false;
    }

    gtk.setAllowedBackends("x11");
    gtk.disableSetlocale();
    if (gtk.initCheck(nullptr, nullptr) == 0) {
        failure = DialogResult::Failure{.code = DialogResult::Code::Failed, .message = "GTK 3 could not connect to the display of the app."};
        return false;
    }
    loaded = true;
    return true;
}

bool LinuxDialogs::findFunctions(void* library) {
    return LinuxGlib::find(library, gtk.setAllowedBackends, "gdk_set_allowed_backends") && LinuxGlib::find(library, gtk.disableSetlocale, "gtk_disable_setlocale") && LinuxGlib::find(library, gtk.initCheck, "gtk_init_check") && LinuxGlib::find(library, gtk.newMessageDialog, "gtk_message_dialog_new") && LinuxGlib::find(library, gtk.setSecondaryText, "gtk_message_dialog_format_secondary_text") && LinuxGlib::find(library, gtk.addButton, "gtk_dialog_add_button") && LinuxGlib::find(library, gtk.setDefaultResponse, "gtk_dialog_set_default_response") && LinuxGlib::find(library, gtk.realize, "gtk_widget_realize") && LinuxGlib::find(library, gtk.getWindow, "gtk_widget_get_window") && LinuxGlib::find(library, gtk.showWidget, "gtk_widget_show") && LinuxGlib::find(library, gtk.destroyWidget, "gtk_widget_destroy") && LinuxGlib::find(library, gtk.getDefaultDisplay, "gdk_display_get_default") && LinuxGlib::find(library, gtk.newForeignWindow, "gdk_x11_window_foreign_new_for_display") && LinuxGlib::find(library, gtk.setTransientFor, "gdk_window_set_transient_for") && LinuxGlib::find(library, gtk.newChooser, "gtk_file_chooser_native_new") && LinuxGlib::find(library, gtk.setChooserModal, "gtk_native_dialog_set_modal") && LinuxGlib::find(library, gtk.showChooser, "gtk_native_dialog_show") && LinuxGlib::find(library, gtk.destroyChooser, "gtk_native_dialog_destroy") && LinuxGlib::find(library, gtk.setSelectMultiple, "gtk_file_chooser_set_select_multiple") && LinuxGlib::find(library, gtk.setCurrentName, "gtk_file_chooser_set_current_name") && LinuxGlib::find(library, gtk.setOverwriteConfirmation, "gtk_file_chooser_set_do_overwrite_confirmation") && LinuxGlib::find(library, gtk.getFilenames, "gtk_file_chooser_get_filenames") && LinuxGlib::find(library, gtk.addFilter, "gtk_file_chooser_add_filter") && LinuxGlib::find(library, gtk.newFilter, "gtk_file_filter_new") && LinuxGlib::find(library, gtk.setFilterName, "gtk_file_filter_set_name") && LinuxGlib::find(library, gtk.addPattern, "gtk_file_filter_add_pattern") && LinuxGlib::find(library, gtk.connect, "g_signal_connect_data") && LinuxGlib::find(library, gtk.unref, "g_object_unref") && LinuxGlib::find(library, gtk.freeMemory, "g_free") && LinuxGlib::find(library, gtk.freeList, "g_slist_free");
}

// The title leads the message in bold where there is one, and the buttons answer with their index. The dialog stays over the window of the app, whose GDK wrapper lives as long as the process.
void LinuxDialogs::showMessage(std::uint64_t id, const DialogRequest::Message& message) {
    int type = kInfoMessage;
    switch (message.kind) {
    case DialogRequest::MessageKind::Info:
        break;
    case DialogRequest::MessageKind::Warning:
        type = kWarningMessage;
        break;
    case DialogRequest::MessageKind::Error:
        type = kErrorMessage;
        break;
    }
    const bool titled = !message.title.empty();
    void* dialog = gtk.newMessageDialog(nullptr, kModal, type, kNoButtons, "%s", titled ? message.title.c_str() : message.text.c_str());
    if (titled) {
        gtk.setSecondaryText(dialog, "%s", message.text.c_str());
    }
    for (std::size_t index = 0; index < message.buttons.size(); ++index) {
        gtk.addButton(dialog, message.buttons[index].c_str(), static_cast<int>(index));
    }
    gtk.setDefaultResponse(dialog, 0);
    gtk.connect(dialog, "response", reinterpret_cast<void (*)()>(&messageResponded), nullptr, nullptr, 0);
    shown.emplace(id, Shown{.dialog = dialog});

    if (parent == nullptr) {
        parent = gtk.newForeignWindow(gtk.getDefaultDisplay(), static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(sapp_x11_get_window())));
    }
    gtk.realize(dialog);
    if (parent != nullptr) {
        gtk.setTransientFor(gtk.getWindow(dialog), parent);
    }
    gtk.showWidget(dialog);
}

void LinuxDialogs::showChooser(std::uint64_t id, const DialogRequest& request) {
    Shown entry{.chooser = true};
    if (const auto* files = std::get_if<DialogRequest::OpenFiles>(&request.dialog)) {
        entry.dialog = gtk.newChooser(files->title.empty() ? nullptr : files->title.c_str(), nullptr, kOpenAction, nullptr, nullptr);
        gtk.setSelectMultiple(entry.dialog, files->multiple ? 1 : 0);
        addFilters(entry.dialog, files->filters);
    } else if (const auto* save = std::get_if<DialogRequest::SaveFile>(&request.dialog)) {
        entry.action = kSaveAction;
        entry.data = save->data;
        entry.dialog = gtk.newChooser(save->title.empty() ? nullptr : save->title.c_str(), nullptr, kSaveAction, nullptr, nullptr);
        gtk.setCurrentName(entry.dialog, save->name.c_str());
        gtk.setOverwriteConfirmation(entry.dialog, 1);
        addFilters(entry.dialog, save->filters);
    } else {
        const auto& folder = std::get<DialogRequest::OpenFolder>(request.dialog);
        entry.action = kFolderAction;
        entry.dialog = gtk.newChooser(folder.title.empty() ? nullptr : folder.title.c_str(), nullptr, kFolderAction, nullptr, nullptr);
    }

    void* chooser = entry.dialog;
    gtk.setChooserModal(chooser, 1);
    gtk.connect(chooser, "response", reinterpret_cast<void (*)()>(&chooserResponded), nullptr, nullptr, 0);
    shown.emplace(id, std::move(entry));
    gtk.showChooser(chooser);
}

// The patterns ignore case, as the file dialogs of the other desktops do.
void LinuxDialogs::addFilters(void* chooser, const std::vector<DialogRequest::Filter>& filters) {
    for (const DialogRequest::Filter& filter : filters) {
        void* chooserFilter = gtk.newFilter();
        gtk.setFilterName(chooserFilter, filter.name.c_str());
        for (const std::string& extension : filter.extensions) {
            gtk.addPattern(chooserFilter, LinuxFilePattern::fromExtension(extension).c_str());
        }
        gtk.addFilter(chooser, chooserFilter);
    }
}

// Escape, the close button of the window and the window manager answer with negative responses, which dismiss the message.
void LinuxDialogs::messageResponded(void* dialog, int response, void*) {
    const auto taken = take(dialog);
    gtk.destroyWidget(dialog);
    if (taken) {
        DialogRelay::resolve(taken->first, response >= 0 ? DialogResult{.button = static_cast<std::size_t>(response)} : DialogResult{});
    }
}

void LinuxDialogs::chooserResponded(void* chooser, int response, void*) {
    auto taken = take(chooser);
    if (!taken) {
        return;
    }
    const std::vector<std::string> files = response == kAccept ? readFiles(chooser) : std::vector<std::string>();
    gtk.destroyChooser(chooser);
    gtk.unref(chooser);
    auto& [id, dialog] = *taken;
    if (files.empty()) {
        DialogRelay::resolve(id, {});
        return;
    }

    if (dialog.action == kSaveAction) {
        writeFile(id, files.front(), std::move(dialog.data));
    } else if (dialog.action == kFolderAction) {
        DialogRelay::resolve(id, {.folder = files.front()});
    } else {
        DialogResult result;
        for (const std::string& file : files) {
            result.files.push_back({.name = std::filesystem::path(file).filename().string(), .path = file});
        }
        DialogRelay::resolve(id, std::move(result));
    }
}

std::optional<std::pair<std::uint64_t, LinuxDialogs::Shown>> LinuxDialogs::take(void* dialog) {
    for (auto entry = shown.begin(); entry != shown.end(); ++entry) {
        if (entry->second.dialog == dialog) {
            std::pair<std::uint64_t, Shown> taken{entry->first, std::move(entry->second)};
            shown.erase(entry);
            return taken;
        }
    }
    return std::nullopt;
}

// The chooser returns a list of paths that belong to the caller, which frees every path and the list.
std::vector<std::string> LinuxDialogs::readFiles(void* chooser) {
    std::vector<std::string> files;
    List* list = gtk.getFilenames(chooser);
    for (List* node = list; node != nullptr; node = node->next) {
        files.emplace_back(static_cast<const char*>(node->data));
        gtk.freeMemory(node->data);
    }
    gtk.freeList(list);
    return files;
}

// Data may be large, so it is written on a thread of its own, which answers the dialog.
void LinuxDialogs::writeFile(std::uint64_t id, const std::filesystem::path& path, std::vector<std::uint8_t> data) {
    // clang-format off
    std::thread([id, path, data = std::move(data)] {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        stream.close();
        const std::string name = path.filename().string();
        if (!stream) {
            DialogRelay::resolve(id, {.failure = DialogResult::Failure{.code = DialogResult::Code::Failed, .message = std::format("The file \"{}\" could not be written.", name)}});
            return;
        }
        DialogRelay::resolve(id, {.saved = DialogResult::File{.name = name, .path = path.string()}});
    }).detach();
    // clang-format on
}

} // namespace haylen::platform
