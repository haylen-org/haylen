#import "platform/apple/HaylenTextField.h"

#if !TARGET_OS_OSX
#import "platform/apple/HaylenTextEditor.h"

@implementation HaylenTextField

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
