#import "platform/apple/HaylenDialog.h"

#include <utility>

#import "platform/apple/AppleDialogs.hpp"

using haylen::platform::AppleDialogs;

@implementation HaylenDialog {
    std::uint64_t dialogId;
    std::filesystem::path root;
}

- (instancetype)initWithIdentifier:(std::uint64_t)identifier folder:(std::filesystem::path)folder {
    self = [super init];
    dialogId = identifier;
    root = std::move(folder);
    return self;
}

- (std::uint64_t)identifier {
    return dialogId;
}

- (std::filesystem::path)folder {
    return root;
}

#if TARGET_OS_IOS
- (void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
    self.picked(urls);
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller {
    AppleDialogs::answer(dialogId, {});
}

// The person swiped the picker away, which UIKit reports here alone.
- (void)presentationControllerDidDismiss:(UIPresentationController*)presentationController {
    AppleDialogs::answer(dialogId, {});
}
#endif

@end
