#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// The authorization of Core Location, which the runtime loads at run time, so an app links CoreLocation only when it uses it. One location manager lives on the main thread, and a request completes once the status changes or, where the person may leave it as it was, once the app becomes active again after the prompt of the system. Every method runs on the main thread.
@interface HaylenLocationAuthorization : NSObject

// The one instance, or `nil` where Core Location does not load.
+ (nullable instancetype)shared;

// The `CLAuthorizationStatus` and the `CLAccuracyAuthorization` of the app.
@property(nonatomic, readonly) int status;
@property(nonatomic, readonly) NSInteger accuracy;

- (void)requestWhenInUse:(void (^)(void))completion;
- (void)requestAlways:(void (^)(void))completion API_UNAVAILABLE(tvos, visionos);

// Asks for the exact location until the app quits, with the text of the key `purpose` of `NSLocationTemporaryUsageDescriptionDictionary`.
- (void)requestFullAccuracy:(NSString*)purpose completion:(void (^)(void))completion;

@end

NS_ASSUME_NONNULL_END
