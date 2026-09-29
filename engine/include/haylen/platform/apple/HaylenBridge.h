#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// Answers a platform call once. The result is any JSON value, and a failed call passes a message or a dictionary with message, code and data.
typedef void (^HaylenReply)(BOOL ok, id _Nullable result);
typedef void (^HaylenHandler)(id params, HaylenReply reply);

// Runs on the main queue when the app cancels a call or its timeout passes, so the handler can stop working on it.
typedef void (^HaylenCancel)(void);
typedef HaylenCancel _Nullable (^HaylenCancellableHandler)(id params, HaylenReply reply);

// Native side of the platform bridge on Apple platforms. Handlers may be registered at any time, even before the app starts, and one registered under the name of a built-in method replaces it. They run on the main queue and may reply later from any thread.
@interface HaylenBridge : NSObject

+ (void)registerHandler:(NSString*)method handler:(HaylenHandler)handler;
+ (void)registerCancellableHandler:(NSString*)method handler:(HaylenCancellableHandler)handler;
+ (void)removeHandler:(NSString*)method;
+ (void)emit:(NSString*)event payload:(nullable id)payload;

@end

NS_ASSUME_NONNULL_END
