#import "haylen/platform/apple/HaylenBridge.h"

#include "platform/apple/AppleBridge.hpp"

@implementation HaylenBridge

+ (void)registerHandler:(NSString*)method handler:(HaylenHandler)handler {
    haylen::platform::AppleBridge::setHandler(method, ^HaylenCancel(id params, HaylenReply reply) {
      handler(params, reply);
      return nil;
    });
}

+ (void)registerCancellableHandler:(NSString*)method handler:(HaylenCancellableHandler)handler {
    haylen::platform::AppleBridge::setHandler(method, handler);
}

+ (void)removeHandler:(NSString*)method {
    haylen::platform::AppleBridge::removeHandler(method);
}

+ (void)emit:(NSString*)event payload:(nullable id)payload {
    haylen::platform::AppleBridge::emit(event, payload);
}

@end
