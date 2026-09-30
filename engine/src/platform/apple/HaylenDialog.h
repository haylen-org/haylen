#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

#include <cstdint>
#include <filesystem>

NS_ASSUME_NONNULL_BEGIN

// A native dialog that shows: its id, the folder that the engine gave it and the UI that shows it, which a cancel closes. On iOS, iPadOS and Mac Catalyst it is the delegate of its document picker and of the presentation of the picker, so the picked documents, the `Cancel` button and the swipe that dismisses the picker each answer the dialog.
#if TARGET_OS_IOS
@interface HaylenDialog : NSObject <UIDocumentPickerDelegate, UIAdaptivePresentationControllerDelegate>
#else
@interface HaylenDialog : NSObject
#endif

@property(nonatomic, readonly) std::uint64_t identifier;
@property(nonatomic, readonly) std::filesystem::path folder;

// The alert, the panel or the picker that shows the dialog.
@property(nonatomic, nullable) id view;

#if TARGET_OS_IOS
// Receives the documents that the picker returns.
@property(nonatomic, copy, nullable) void (^picked)(NSArray<NSURL*>* urls);
#endif

- (instancetype)initWithIdentifier:(std::uint64_t)identifier folder:(std::filesystem::path)folder;

@end

NS_ASSUME_NONNULL_END
