#import "platform/apple/HaylenTextView.h"

#if !TARGET_OS_OSX && !TARGET_OS_TV
#import "platform/apple/HaylenTextEditor.h"

@implementation HaylenTextView

- (NSArray<UIKeyCommand*>*)keyCommands {
    UIKeyCommand* tab = [UIKeyCommand keyCommandWithInput:@"\t" modifierFlags:0 action:@selector(next:)];
    UIKeyCommand* escape = [UIKeyCommand keyCommandWithInput:UIKeyInputEscape modifierFlags:0 action:@selector(cancel:)];
    tab.wantsPriorityOverSystemBehavior = YES;
    escape.wantsPriorityOverSystemBehavior = YES;
    return @[ tab, escape ];
}

- (void)next:(UIKeyCommand*)command {
    [self.editor act:haylen::platform::TextInput::Action::Next];
}

- (void)cancel:(UIKeyCommand*)command {
    [self.editor act:haylen::platform::TextInput::Action::Cancel];
}

@end
#endif
