#pragma once

#include <TargetConditionals.h>

#import <Foundation/Foundation.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "haylen/platform/DialogRequest.hpp"
#include "haylen/platform/DialogResult.hpp"

@class HaylenDialog;
@class UTType;

#if TARGET_OS_IOS
@class UIDocumentPickerViewController;
#endif

namespace haylen::platform {

// The native dialogs of Apple platforms. macOS shows every dialog as a sheet of the window of the app. iOS, iPadOS and Mac Catalyst show an alert and the document picker over the topmost view controller: the picker copies opened files into the folder of the dialog, exports the data of a save from a file there and keeps the access to a picked folder for the session. tvOS shows alerts alone. Every method runs on the main thread, and every dialog answers exactly once through `DialogRelay`.
class AppleDialogs final {
  public:
    static void show(std::uint64_t id, const DialogRequest& request, const std::filesystem::path& folder);

    // Closes a dialog that the app gave up and answers it as dismissed, which the engine drops.
    static void cancel(std::uint64_t id);

    // Answers a dialog that still shows and forgets it, so later answers of the same dialog change nothing.
    static void answer(std::uint64_t id, DialogResult result);

  private:
    static void showMessage(HaylenDialog* dialog, const DialogRequest::Message& message);
    static void showOpenFiles(HaylenDialog* dialog, const DialogRequest::OpenFiles& files);
    static void showSaveFile(HaylenDialog* dialog, const DialogRequest::SaveFile& save);
    static void showOpenFolder(HaylenDialog* dialog, const DialogRequest::OpenFolder& open);

    // The content types of the extensions of the filters, none when there are no filters.
    [[nodiscard]] static NSArray<UTType*>* getContentTypes(const std::vector<DialogRequest::Filter>& filters);

    [[nodiscard]] static DialogResult makeFailure(DialogResult::Code code, const std::string& message);

#if TARGET_OS_IOS
    // Shows the document picker of the dialog, whose delegate and presentation delegate the dialog is.
    static void presentPicker(HaylenDialog* dialog, UIDocumentPickerViewController* picker);

    // Moves the copies that the picker made into the folder of the dialog, each file in a numbered folder of its own when a file of its name is there already, on a queue of its own, since files may be large.
    static void moveFiles(std::uint64_t id, NSArray<NSURL*>* urls, const std::filesystem::path& folder);
#endif

    static NSMutableDictionary<NSNumber*, HaylenDialog*>* dialogs;

    // The folders that pickers returned, whose security-scoped access lasts for the session.
    static NSMutableSet<NSURL*>* folders;
};

} // namespace haylen::platform
