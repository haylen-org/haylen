#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// Answers a platform call once. The result is any JSON value, with NSData values anywhere inside that cross as byte buffers, and a failed call passes a message or a dictionary with message, code and data.
typedef void (^HaylenReply)(BOOL ok, id _Nullable result);
typedef void (^HaylenHandler)(id params, HaylenReply reply);

// Runs on the main queue when the app cancels a call or its timeout passes, so the handler can stop working on it.
typedef void (^HaylenCancel)(void);
typedef HaylenCancel _Nullable (^HaylenCancellableHandler)(id params, HaylenReply reply);

// Native side of the platform bridge on Apple platforms. Handlers may be registered at any time, even before the app starts. They run on the main queue and may reply later from any thread. Bytes that the app sends arrive as NSData values in the parameters.
@interface HaylenBridge : NSObject

+ (void)registerHandler:(NSString*)method handler:(HaylenHandler)handler;
+ (void)registerCancellableHandler:(NSString*)method handler:(HaylenCancellableHandler)handler;
+ (void)removeHandler:(NSString*)method;

// Sends an event to the app from any thread, with NSData values that cross as byte buffers. A retained event waits for the first listener of its name, the batched events of a name that arrive in one frame reach the app as one list in order, and events sent while the app launches reach the first app once it starts.
+ (void)emit:(NSString*)event payload:(nullable id)payload;
+ (void)emit:(NSString*)event payload:(nullable id)payload retain:(BOOL)retain;
+ (void)emit:(NSString*)event payload:(nullable id)payload retain:(BOOL)retain batched:(BOOL)batched;

@end

NS_ASSUME_NONNULL_END
