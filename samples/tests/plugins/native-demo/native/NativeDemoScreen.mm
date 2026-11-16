// The screens of macOS: a sheet of the window of the app, which AppKit shows over it and which keeps the events of the mouse and the keyboard from the app while it shows. The frame thread of the app is the main thread of AppKit, so the sheet opens and ends there.

#import <AppKit/AppKit.h>

#include "NativeDemoScreen.hpp"

#include <utility>

// The sheet of the screen that shows, which ends the screen once through the engine.
@interface NativeDemoSheet : NSObject

@property(nonatomic) const HaylenNativeApi* api;
@property(nonatomic) uint64_t screen;
@property(nonatomic, strong) NSWindow* parent;
@property(nonatomic, strong) NSWindow* sheet;

- (instancetype)initWithContent:(NativeDemoScreen::Content)content;
- (void)endWithResult:(const std::string&)json ok:(int)ok;

@end

struct NativeDemoScreen::Showing {
    NativeDemoSheet* sheet = nil;
};

@implementation NativeDemoSheet {
    NativeDemoScreen::Content content;
}

- (instancetype)initWithContent:(NativeDemoScreen::Content)given {
    self = [super init];
    if (self != nil) {
        content = std::move(given);
    }
    return self;
}

- (void)confirm:(id)sender {
    if (content.onConfirm) {
        content.onConfirm();
    }
    [self endWithResult:content.confirmed ok:1];
}

- (void)decline:(id)sender {
    if (content.declined) {
        [self endWithResult:*content.declined ok:1];
        return;
    }
    [self endWithResult:content.closed ok:0];
}

- (void)close:(id)sender {
    [self endWithResult:content.closed ok:0];
}

// The record of the screen that shows holds the only reference to the sheet, so it lets go last, once nothing reads the sheet anymore.
- (void)endWithResult:(const std::string&)json ok:(int)ok {
    NativeDemoScreen::Showing& showing = NativeDemoScreen::getShowing();
    if (showing.sheet != self) {
        return;
    }
    [self.parent endSheet:self.sheet];
    self.api->finishScreen(self.screen, ok, json.c_str(), nullptr, 0);
    showing.sheet = nil;
}

@end

NativeDemoScreen::Showing& NativeDemoScreen::getShowing() {
    static Showing* showing = new Showing();
    return *showing;
}

void NativeDemoScreen::open(const HaylenNativeApi& api, std::uint64_t screen, Content content) {
    HaylenNativeWindow window{};
    if (!api.getWindow(&window) || window.handle == nullptr) {
        api.finishScreen(screen, 0, R"({"message":"The app has no window yet.","code":"noWindow"})", nullptr, 0);
        return;
    }

    NSString* title = @(content.title.c_str());
    NSString* question = @(content.question.c_str());
    NSString* confirmLabel = @(content.confirmLabel.c_str());
    NSString* declineLabel = content.declineLabel ? @(content.declineLabel->c_str()) : nil;
    NativeDemoSheet* sheet = [[NativeDemoSheet alloc] initWithContent:std::move(content)];
    sheet.api = &api;
    sheet.screen = screen;
    sheet.parent = (__bridge NSWindow*)window.handle;
    sheet.sheet = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 440, 200) styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];

    NSTextField* heading = [NSTextField labelWithString:title];
    heading.font = [NSFont boldSystemFontOfSize:20];
    NSTextField* detail = [NSTextField wrappingLabelWithString:question];
    NSButton* close = [NSButton buttonWithTitle:@"Close" target:sheet action:@selector(close:)];
    close.keyEquivalent = @"\e";
    NSMutableArray<NSView*>* buttons = [NSMutableArray arrayWithObject:close];
    if (declineLabel != nil) {
        [buttons addObject:[NSButton buttonWithTitle:declineLabel target:sheet action:@selector(decline:)]];
    }
    NSButton* confirm = [NSButton buttonWithTitle:confirmLabel target:sheet action:@selector(confirm:)];
    confirm.keyEquivalent = @"\r";
    [buttons addObject:confirm];
    NSStackView* column = [NSStackView stackViewWithViews:@[ heading, detail, [NSStackView stackViewWithViews:buttons] ]];
    column.orientation = NSUserInterfaceLayoutOrientationVertical;
    column.spacing = 16;
    column.edgeInsets = NSEdgeInsetsMake(24, 24, 24, 24);
    sheet.sheet.contentView = column;

    getShowing().sheet = sheet;
    [sheet.parent beginSheet:sheet.sheet completionHandler:nil];
}

void NativeDemoScreen::cancel(std::uint64_t screen) {
    NativeDemoSheet* sheet = getShowing().sheet;
    if (sheet != nil && sheet.screen == screen) {
        [sheet endWithResult:R"({"message":"The app gave the screen up.","code":"cancelled"})" ok:0];
    }
}
