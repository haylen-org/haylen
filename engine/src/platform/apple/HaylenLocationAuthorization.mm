#import "platform/apple/HaylenLocationAuthorization.h"

#include <TargetConditionals.h>
#include <dlfcn.h>

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>
#endif

// The methods of `CLLocationManager` that the runtime calls, with plain types in place of the types of CoreLocation.
@protocol HaylenLocationManager <NSObject>
- (void)setDelegate:(nullable id)delegate;
- (int)authorizationStatus;
- (NSInteger)accuracyAuthorization;
- (void)requestWhenInUseAuthorization;
- (void)requestAlwaysAuthorization;
- (void)requestTemporaryFullAccuracyAuthorizationWithPurposeKey:(NSString*)purposeKey completion:(void (^_Nullable)(NSError* _Nullable error))completion;
@end

@implementation HaylenLocationAuthorization {
    id<HaylenLocationManager> manager;
    // Each request waits with the status it started from.
    NSMutableArray<NSArray*>* waiting;
    BOOL prompted;
}

+ (instancetype)shared {
    static HaylenLocationAuthorization* const instance = [self create];
    return instance;
}

+ (nullable instancetype)create {
    if (dlopen("/System/Library/Frameworks/CoreLocation.framework/CoreLocation", RTLD_LAZY | RTLD_LOCAL) == nullptr) {
        return nil;
    }
    Class type = NSClassFromString(@"CLLocationManager");
    return type != nil ? [[self alloc] initWithManager:[[type alloc] init]] : nil;
}

- (instancetype)initWithManager:(id<HaylenLocationManager>)value {
    self = [super init];
    manager = value;
    waiting = [NSMutableArray array];
    [manager setDelegate:self];
#if !TARGET_OS_OSX
    // The prompt of the system makes the app resign active, so the app that becomes active again knows the person answered it, even with the status left as it was.
    NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
    __weak HaylenLocationAuthorization* weak = self;
    [center addObserverForName:UIApplicationWillResignActiveNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*) { [weak notePrompt]; }];
    [center addObserverForName:UIApplicationDidBecomeActiveNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*) { [weak answerAfterPrompt]; }];
#endif
    return self;
}

- (int)status {
    return manager.authorizationStatus;
}

- (NSInteger)accuracy {
    return manager.accuracyAuthorization;
}

- (void)requestWhenInUse:(void (^)(void))completion {
    [waiting addObject:@[ @(manager.authorizationStatus), completion ]];
    [manager requestWhenInUseAuthorization];
}

- (void)requestAlways:(void (^)(void))completion {
    [waiting addObject:@[ @(manager.authorizationStatus), completion ]];
    [manager requestAlwaysAuthorization];
}

- (void)requestFullAccuracy:(NSString*)purpose completion:(void (^)(void))completion {
    [manager requestTemporaryFullAccuracyAuthorizationWithPurposeKey:purpose completion:^(NSError*) { dispatch_async(dispatch_get_main_queue(), completion); }];
}

- (void)locationManagerDidChangeAuthorization:(id)changed {
    const int current = manager.authorizationStatus;
    NSMutableArray<NSArray*>* answered = [NSMutableArray array];
    for (NSArray* entry in waiting) {
        if ([entry[0] intValue] != current) {
            [answered addObject:entry];
        }
    }
    [waiting removeObjectsInArray:answered];
    for (NSArray* entry in answered) {
        ((void (^)(void))entry[1])();
    }
}

- (void)notePrompt {
    prompted = waiting.count > 0;
}

- (void)answerAfterPrompt {
    if (!prompted) {
        return;
    }
    prompted = NO;
    NSArray<NSArray*>* answered = [waiting copy];
    [waiting removeAllObjects];
    for (NSArray* entry in answered) {
        ((void (^)(void))entry[1])();
    }
}

@end
