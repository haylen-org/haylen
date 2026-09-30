#import "platform/apple/HaylenScreenController.h"

#if !TARGET_OS_OSX
#import "platform/apple/HaylenScreen+Runtime.h"

@implementation HaylenScreenController {
    __weak HaylenScreen* owner;
    UIViewController* content;
}

- (instancetype)initWithScreen:(HaylenScreen*)screen content:(UIViewController*)child style:(UIModalPresentationStyle)style {
    self = [super initWithNibName:nil bundle:nil];
    owner = screen;
    content = child;
    self.modalPresentationStyle = style;
    self.presentationController.delegate = self;
    self.preferredContentSize = child.preferredContentSize;
    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    [self addChildViewController:content];
    content.view.frame = self.view.bounds;
    content.view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self.view addSubview:content.view];
    [content didMoveToParentViewController:self];
}

// The container goes away when it is dismissed, and also when a controller below it dismisses what it presents.
- (void)viewDidDisappear:(BOOL)animated {
    [super viewDidDisappear:animated];
    if (self.isBeingDismissed || self.presentingViewController == nil) {
        [owner dismissed];
    }
}

- (void)presentationControllerDidDismiss:(UIPresentationController*)presentationController {
    [owner dismissed];
}

- (void)preferredContentSizeDidChangeForChildContentContainer:(id<UIContentContainer>)container {
    [super preferredContentSizeDidChangeForChildContentContainer:container];
    self.preferredContentSize = container.preferredContentSize;
}

- (NSArray<id<UIFocusEnvironment>>*)preferredFocusEnvironments {
    return @[ content ];
}

#if !TARGET_OS_TV
- (BOOL)isModalInPresentation {
    return content.isModalInPresentation;
}

- (UIViewController*)childViewControllerForStatusBarStyle {
    return content;
}

- (UIViewController*)childViewControllerForStatusBarHidden {
    return content;
}

- (UIViewController*)childViewControllerForHomeIndicatorAutoHidden {
    return content;
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return content.supportedInterfaceOrientations;
}
#endif

@end
#endif
