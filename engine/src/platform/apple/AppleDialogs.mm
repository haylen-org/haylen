#import "platform/apple/AppleDialogs.hpp"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <system_error>
#include <utility>
#include <variant>

#include "platform/DialogRelay.hpp"
#import "platform/apple/HaylenDialog.h"
#include "sokol_app.h"

#if !TARGET_OS_OSX
#import "platform/apple/ApplePresenter.hpp"
#endif

namespace haylen::platform {

NSMutableDictionary<NSNumber*, HaylenDialog*>* AppleDialogs::dialogs = [NSMutableDictionary dictionary];
NSMutableSet<NSURL*>* AppleDialogs::folders = [NSMutableSet set];

// The dialog is known before it shows, so an answer that comes at once, such as a failure, still answers it.
void AppleDialogs::show(std::uint64_t id, const DialogRequest& request, const std::filesystem::path& folder) {
    HaylenDialog* dialog = [[HaylenDialog alloc] initWithIdentifier:id folder:folder];
    dialogs[@(id)] = dialog;
#if TARGET_OS_OSX
    if (!sapp_isvalid()) {
        answer(id, makeFailure(DialogResult::Code::Failed, "The app has no window to show the dialog on."));
        return;
    }
#endif

    if (const auto* message = std::get_if<DialogRequest::Message>(&request.dialog)) {
        showMessage(dialog, *message);
    } else if (const auto* files = std::get_if<DialogRequest::OpenFiles>(&request.dialog)) {
        showOpenFiles(dialog, *files);
    } else if (const auto* save = std::get_if<DialogRequest::SaveFile>(&request.dialog)) {
        showSaveFile(dialog, *save);
    } else {
        showOpenFolder(dialog, std::get<DialogRequest::OpenFolder>(request.dialog));
    }
}

// A closed sheet and a closed panel answer through their completion, and a dismissed controller through nothing, so the dialog answers here either way.
void AppleDialogs::cancel(std::uint64_t id) {
    HaylenDialog* dialog = dialogs[@(id)];
    if (dialog == nil) {
        return;
    }
#if TARGET_OS_OSX
    if ([dialog.view isKindOfClass:NSAlert.class]) {
        NSWindow* sheet = static_cast<NSAlert*>(dialog.view).window;
        [(__bridge NSWindow*)sapp_macos_get_window() endSheet:sheet returnCode:NSModalResponseCancel];
    } else {
        [static_cast<NSSavePanel*>(dialog.view) cancel:nil];
    }
#else
    [static_cast<UIViewController*>(dialog.view) dismissViewControllerAnimated:YES completion:nil];
#endif
    answer(id, {});
}

void AppleDialogs::answer(std::uint64_t id, DialogResult result) {
    if (dialogs[@(id)] == nil) {
        return;
    }
    [dialogs removeObjectForKey:@(id)];
    DialogRelay::resolve(id, std::move(result));
}

NSArray<UTType*>* AppleDialogs::getContentTypes(const std::vector<DialogRequest::Filter>& filters) {
    NSMutableArray<UTType*>* types = [NSMutableArray array];
    for (const DialogRequest::Filter& filter : filters) {
        for (const std::string& extension : filter.extensions) {
            UTType* type = [UTType typeWithFilenameExtension:@(extension.c_str())];
            if (type != nil && ![types containsObject:type]) {
                [types addObject:type];
            }
        }
    }
    return types;
}

DialogResult AppleDialogs::makeFailure(DialogResult::Code code, const std::string& message) {
    return {.failure = DialogResult::Failure{.code = code, .message = message}};
}

#if TARGET_OS_OSX
// The first button answers Return, and a button named `Cancel` answers Escape, as in every alert of the Mac.
void AppleDialogs::showMessage(HaylenDialog* dialog, const DialogRequest::Message& message) {
    NSAlert* alert = [[NSAlert alloc] init];
    switch (message.kind) {
    case DialogRequest::MessageKind::Info:
        alert.alertStyle = NSAlertStyleInformational;
        break;
    case DialogRequest::MessageKind::Warning:
        alert.alertStyle = NSAlertStyleWarning;
        break;
    case DialogRequest::MessageKind::Error:
        alert.alertStyle = NSAlertStyleCritical;
        break;
    }
    alert.messageText = @((message.title.empty() ? message.text : message.title).c_str());
    alert.informativeText = message.title.empty() ? @"" : @(message.text.c_str());
    for (const std::string& label : message.buttons) {
        [alert addButtonWithTitle:@(label.c_str())];
    }

    dialog.view = alert;
    const std::uint64_t id = dialog.identifier;
    const std::size_t count = message.buttons.size();
    // clang-format off
    [alert beginSheetModalForWindow:(__bridge NSWindow*)sapp_macos_get_window() completionHandler:^(NSModalResponse response) {
        const NSInteger index = response - NSAlertFirstButtonReturn;
        answer(id, index >= 0 && static_cast<std::size_t>(index) < count ? DialogResult{.button = static_cast<std::size_t>(index)} : DialogResult{});
    }];
    // clang-format on
}

void AppleDialogs::showOpenFiles(HaylenDialog* dialog, const DialogRequest::OpenFiles& files) {
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = files.multiple;
    panel.message = @(files.title.c_str());
    if (!files.filters.empty()) {
        panel.allowedContentTypes = getContentTypes(files.filters);
    }

    dialog.view = panel;
    const std::uint64_t id = dialog.identifier;
    // clang-format off
    [panel beginSheetModalForWindow:(__bridge NSWindow*)sapp_macos_get_window() completionHandler:^(NSModalResponse response) {
        if (response != NSModalResponseOK) {
            answer(id, {});
            return;
        }
        DialogResult result;
        for (NSURL* url in panel.URLs) {
            result.files.push_back({.name = url.lastPathComponent.UTF8String, .path = url.path.UTF8String});
        }
        answer(id, std::move(result));
    }];
    // clang-format on
}

void AppleDialogs::showSaveFile(HaylenDialog* dialog, const DialogRequest::SaveFile& save) {
    NSSavePanel* panel = [NSSavePanel savePanel];
    panel.message = @(save.title.c_str());
    panel.nameFieldStringValue = @(save.name.c_str());
    panel.canCreateDirectories = YES;
    if (!save.filters.empty()) {
        panel.allowedContentTypes = getContentTypes(save.filters);
    }

    dialog.view = panel;
    const std::uint64_t id = dialog.identifier;
    NSData* data = [NSData dataWithBytes:save.data.data() length:save.data.size()];
    // clang-format off
    [panel beginSheetModalForWindow:(__bridge NSWindow*)sapp_macos_get_window() completionHandler:^(NSModalResponse response) {
        if (response != NSModalResponseOK) {
            answer(id, {});
            return;
        }
        NSError* error = nil;
        if (![data writeToURL:panel.URL options:NSDataWritingAtomic error:&error]) {
            answer(id, makeFailure(DialogResult::Code::Failed, std::string("The file could not be written. ") + error.localizedDescription.UTF8String));
            return;
        }
        answer(id, {.saved = DialogResult::File{.name = panel.URL.lastPathComponent.UTF8String, .path = panel.URL.path.UTF8String}});
    }];
    // clang-format on
}

void AppleDialogs::showOpenFolder(HaylenDialog* dialog, const DialogRequest::OpenFolder& open) {
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.canCreateDirectories = YES;
    panel.allowsMultipleSelection = NO;
    panel.message = @(open.title.c_str());

    dialog.view = panel;
    const std::uint64_t id = dialog.identifier;
    [panel beginSheetModalForWindow:(__bridge NSWindow*)sapp_macos_get_window() completionHandler:^(NSModalResponse response) { answer(id, response == NSModalResponseOK ? DialogResult{.folder = std::string(panel.URL.path.UTF8String)} : DialogResult{}); }];
}
#else
// An alert answers only through its buttons, which dismiss it, in the order of the request.
void AppleDialogs::showMessage(HaylenDialog* dialog, const DialogRequest::Message& message) {
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:message.title.empty() ? nil : @(message.title.c_str()) message:@(message.text.c_str()) preferredStyle:UIAlertControllerStyleAlert];
    const std::uint64_t id = dialog.identifier;
    for (std::size_t index = 0; index < message.buttons.size(); ++index) {
        [alert addAction:[UIAlertAction actionWithTitle:@(message.buttons[index].c_str()) style:UIAlertActionStyleDefault handler:^(UIAlertAction*) { answer(id, {.button = index}); }]];
    }

    dialog.view = alert;
    ApplePresenter::present(alert, ^{ answer(id, makeFailure(DialogResult::Code::Failed, "The app has no window to show the dialog over.")); });
}

#if TARGET_OS_TV
void AppleDialogs::showOpenFiles(HaylenDialog* dialog, const DialogRequest::OpenFiles&) {
    answer(dialog.identifier, makeFailure(DialogResult::Code::Unsupported, "tvOS has no document picker, so apps on a TV open no files."));
}

void AppleDialogs::showSaveFile(HaylenDialog* dialog, const DialogRequest::SaveFile&) {
    answer(dialog.identifier, makeFailure(DialogResult::Code::Unsupported, "tvOS has no document picker, so apps on a TV save no files where the person picks."));
}

void AppleDialogs::showOpenFolder(HaylenDialog* dialog, const DialogRequest::OpenFolder&) {
    answer(dialog.identifier, makeFailure(DialogResult::Code::Unsupported, "tvOS has no document picker, so apps on a TV open no folders."));
}
#else
// The picker makes copies that belong to the app, which move into the folder of the dialog.
void AppleDialogs::showOpenFiles(HaylenDialog* dialog, const DialogRequest::OpenFiles& files) {
    NSArray<UTType*>* types = getContentTypes(files.filters);
    UIDocumentPickerViewController* picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:types.count > 0 ? types : @[ UTTypeItem ] asCopy:YES];
    picker.allowsMultipleSelection = files.multiple;
    const std::uint64_t id = dialog.identifier;
    const std::filesystem::path folder = dialog.folder;
    dialog.picked = ^(NSArray<NSURL*>* urls) { moveFiles(id, urls, folder); };
    presentPicker(dialog, picker);
}

// The picker exports a file with the data from the folder of the dialog, which the engine empties when the next app starts.
void AppleDialogs::showSaveFile(HaylenDialog* dialog, const DialogRequest::SaveFile& save) {
    const std::uint64_t id = dialog.identifier;
    std::error_code error;
    std::filesystem::create_directories(dialog.folder, error);
    NSURL* file = [NSURL fileURLWithPath:@((dialog.folder / save.name).c_str())];
    NSError* failure = nil;
    if (error || ![[NSData dataWithBytes:save.data.data() length:save.data.size()] writeToURL:file options:NSDataWritingAtomic error:&failure]) {
        answer(id, makeFailure(DialogResult::Code::Failed, "The data to save could not be written to a file to export. " + (error ? error.message() : std::string(failure.localizedDescription.UTF8String))));
        return;
    }

    UIDocumentPickerViewController* picker = [[UIDocumentPickerViewController alloc] initForExportingURLs:@[ file ] asCopy:YES];
    // clang-format off
    dialog.picked = ^(NSArray<NSURL*>* urls) {
        [NSFileManager.defaultManager removeItemAtURL:file error:nil];
        NSURL* saved = urls.firstObject;
        answer(id, saved != nil ? DialogResult{.saved = DialogResult::File{.name = saved.lastPathComponent.UTF8String, .path = saved.path.UTF8String}} : DialogResult{});
    };
    // clang-format on
    presentPicker(dialog, picker);
}

// A folder outside the app is reachable only while its security-scoped access lasts, which the session keeps.
void AppleDialogs::showOpenFolder(HaylenDialog* dialog, const DialogRequest::OpenFolder&) {
    UIDocumentPickerViewController* picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[ UTTypeFolder ]];
    const std::uint64_t id = dialog.identifier;
    // clang-format off
    dialog.picked = ^(NSArray<NSURL*>* urls) {
        NSURL* folder = urls.firstObject;
        if (folder == nil) {
            answer(id, {});
            return;
        }
        if ([folder startAccessingSecurityScopedResource]) {
            [folders addObject:folder];
        }
        answer(id, {.folder = std::string(folder.path.UTF8String)});
    };
    // clang-format on
    presentPicker(dialog, picker);
}

void AppleDialogs::presentPicker(HaylenDialog* dialog, UIDocumentPickerViewController* picker) {
    picker.delegate = dialog;
    picker.presentationController.delegate = dialog;
    dialog.view = picker;
    const std::uint64_t id = dialog.identifier;
    ApplePresenter::present(picker, ^{ answer(id, makeFailure(DialogResult::Code::Failed, "The app has no window to show the picker over.")); });
}

void AppleDialogs::moveFiles(std::uint64_t id, NSArray<NSURL*>* urls, const std::filesystem::path& folder) {
    const std::filesystem::path destination = folder;
    // clang-format off
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        DialogResult result;
        std::error_code error;
        for (NSURL* url in urls) {
            const std::string name = url.lastPathComponent.UTF8String;
            std::filesystem::path target = destination / name;
            for (int copy = 1; std::filesystem::exists(target, error); ++copy) {
                target = destination / std::to_string(copy) / name;
            }
            std::filesystem::create_directories(target.parent_path(), error);
            NSError* failure = nil;
            if (![NSFileManager.defaultManager moveItemAtURL:url toURL:[NSURL fileURLWithPath:@(target.c_str())] error:&failure]) {
                result = makeFailure(DialogResult::Code::Failed, "The picked file \"" + name + "\" could not be copied for the app. " + failure.localizedDescription.UTF8String);
                break;
            }
            result.files.push_back({.name = name, .path = target.string()});
        }
        dispatch_async(dispatch_get_main_queue(), ^{ answer(id, result); });
    });
    // clang-format on
}
#endif
#endif

} // namespace haylen::platform
