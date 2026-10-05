// The screens of macOS: a sheet of the window of the app, which AppKit shows over it and which keeps the events of the mouse and the keyboard from the app while it shows. The frame thread of the app is the main thread of AppKit, so the sheet opens and ends there.

#import <AppKit/AppKit.h>

#include "NativeDemoScreen.h"

// The sheet of the screen that shows, which ends the screen once through the engine.
@interface NativeDemoScreen : NSObject

@property(nonatomic) const HaylenNativeApi* api;
@property(nonatomic) uint64_t screen;
@property(nonatomic, strong) NSWindow* parent;
@property(nonatomic, strong) NSWindow* sheet;
@property(nonatomic, copy) NSString* confirmed;
@property(nonatomic, copy) NSString* declined;
@property(nonatomic, copy) NSString* closed;
@property(nonatomic) void (*onConfirm)(void);

- (void)endWithResult:(NSString*)json ok:(int)ok;

@end

static NativeDemoScreen* nativeDemoScreenShowing = nil;

@implementation NativeDemoScreen

- (void)confirm:(id)sender {
    if (self.onConfirm != NULL) {
        self.onConfirm();
    }
    [self endWithResult:self.confirmed ok:1];
}

- (void)decline:(id)sender {
    if (self.declined != nil) {
        [self endWithResult:self.declined ok:1];
    } else {
        [self endWithResult:self.closed ok:0];
    }
}

- (void)close:(id)sender {
    [self endWithResult:self.closed ok:0];
}

// The static holds the only reference to the screen, so it lets go last, once nothing reads the screen anymore.
- (void)endWithResult:(NSString*)json ok:(int)ok {
    if (nativeDemoScreenShowing != self) {
        return;
    }
    [self.parent endSheet:self.sheet];
    self.api->finishScreen(self.screen, ok, json.UTF8String, NULL, 0);
    nativeDemoScreenShowing = nil;
}

@end

static NSButton* native_demo_screen_button(NSString* title, SEL action, NativeDemoScreen* target, NSString* key) {
    NSButton* button = [NSButton buttonWithTitle:title target:target action:action];
    button.keyEquivalent = key;
    return button;
}

void native_demo_screen_open(const HaylenNativeApi* api, uint64_t screen, const NativeDemoScreenContent* content) {
    HaylenNativeWindow window;
    if (!api->getWindow(&window) || window.handle == NULL) {
        api->finishScreen(screen, 0, "{\"message\":\"The app has no window yet.\",\"code\":\"noWindow\"}", NULL, 0);
        return;
    }

    NativeDemoScreen* showing = [[NativeDemoScreen alloc] init];
    showing.api = api;
    showing.screen = screen;
    showing.parent = (__bridge NSWindow*)window.handle;
    showing.confirmed = @(content->confirmed);
    showing.declined = content->declined != NULL ? @(content->declined) : nil;
    showing.closed = @(content->closed);
    showing.onConfirm = content->onConfirm;
    showing.sheet = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 440, 200) styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];

    NSTextField* heading = [NSTextField labelWithString:@(content->title)];
    heading.font = [NSFont boldSystemFontOfSize:20];
    NSTextField* detail = [NSTextField wrappingLabelWithString:@(content->question)];
    NSMutableArray<NSView*>* buttons = [NSMutableArray arrayWithObject:native_demo_screen_button(@"Close", @selector(close:), showing, @"\e")];
    if (content->declineLabel != NULL) {
        [buttons addObject:native_demo_screen_button(@(content->declineLabel), @selector(decline:), showing, @"")];
    }
    [buttons addObject:native_demo_screen_button(@(content->confirmLabel), @selector(confirm:), showing, @"\r")];
    NSStackView* column = [NSStackView stackViewWithViews:@[ heading, detail, [NSStackView stackViewWithViews:buttons] ]];
    column.orientation = NSUserInterfaceLayoutOrientationVertical;
    column.spacing = 16;
    column.edgeInsets = NSEdgeInsetsMake(24, 24, 24, 24);
    showing.sheet.contentView = column;

    nativeDemoScreenShowing = showing;
    [showing.parent beginSheet:showing.sheet completionHandler:nil];
}

void native_demo_screen_cancel(uint64_t screen) {
    if (nativeDemoScreenShowing != nil && nativeDemoScreenShowing.screen == screen) {
        [nativeDemoScreenShowing endWithResult:@"{\"message\":\"The app gave the screen up.\",\"code\":\"cancelled\"}" ok:0];
    }
}
