#import "platform/apple/AppleBridge.hpp"

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

#include "haylen/core/Log.hpp"
#include "platform/BridgeRelay.hpp"

namespace haylen::platform {

void AppleBridge::setHandler(NSString* method, HaylenCancellableHandler handler) {
    @synchronized([HaylenBridge class]) {
        getHandlers()[method] = [handler copy];
    }
}

void AppleBridge::removeHandler(NSString* method) {
    @synchronized([HaylenBridge class]) {
        [getHandlers() removeObjectForKey:method];
    }
}

void AppleBridge::clearHandlers() {
    @synchronized([HaylenBridge class]) {
        [getHandlers() removeAllObjects];
    }
}

void AppleBridge::registerBuiltIns() {
    registerBuiltIn(@"device.info", ^(id, HaylenReply reply) {
#if TARGET_OS_OSX
      NSOperatingSystemVersion version = NSProcessInfo.processInfo.operatingSystemVersion;
      NSString* systemVersion = [NSString stringWithFormat:@"%ld.%ld.%ld", static_cast<long>(version.majorVersion), static_cast<long>(version.minorVersion), static_cast<long>(version.patchVersion)];
      reply(YES, @{@"model" : @"Mac", @"system" : @"macOS", @"systemVersion" : systemVersion, @"locale" : getLanguageTag()});
#else
      UIDevice* device = UIDevice.currentDevice;
      reply(YES, @{@"model" : device.model, @"system" : device.systemName, @"systemVersion" : device.systemVersion, @"locale" : getLanguageTag()});
#endif
    });
    registerBuiltIn(@"system.locale", ^(id, HaylenReply reply) { reply(YES, getLanguageTag()); });
    registerBuiltIn(@"system.open_url", ^(id params, HaylenReply reply) {
      id address = [params isKindOfClass:NSDictionary.class] ? params[@"url"] : nil;
      if (![address isKindOfClass:NSString.class] || [address length] == 0) {
          reply(NO, @"The url is missing.");
          return;
      }
      NSURL* url = [NSURL URLWithString:address];
      if (url == nil) {
          reply(NO, @"The url could not be opened.");
          return;
      }
#if TARGET_OS_OSX
      if ([NSWorkspace.sharedWorkspace openURL:url]) {
          reply(YES, @YES);
      } else {
          reply(NO, @"The url could not be opened.");
      }
#else
      [UIApplication.sharedApplication openURL:url
                                       options:@{}
                             completionHandler:^(BOOL opened) {
                               if (opened) {
                                   reply(YES, @YES);
                               } else {
                                   reply(NO, @"The url could not be opened.");
                               }
                             }];
#endif
    });
    registerBuiltIn(@"haptics.vibrate", ^(id, HaylenReply reply) {
#if TARGET_OS_IOS
      UIImpactFeedbackGenerator* generator = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleMedium];
      [generator impactOccurred];
#endif
      reply(YES, nil);
    });
}

// The call is pending from here on, so a cancel that arrives before the handler runs keeps it from running, and the cancel block the handler returns is kept only while the call still waits.
void AppleBridge::dispatch(std::uint64_t call, std::string_view method, std::string_view paramsJson) {
    NSString* name = toString(method);
    NSNumber* key = @(call);
    HaylenCancellableHandler handler = nil;
    @synchronized([HaylenBridge class]) {
        handler = getHandlers()[name];
        if (handler != nil) {
            getCalls()[key] = NSNull.null;
        }
    }
    if (handler == nil) {
        fail(call, @{@"message" : [NSString stringWithFormat:@"No native handler is registered for %@.", name], @"code" : @"no_handler"});
        return;
    }

    id parsed = fromJson(paramsJson);
    id params = parsed != nil ? parsed : @{};
    dispatch_async(dispatch_get_main_queue(), ^{
      @synchronized([HaylenBridge class]) {
          if (getCalls()[key] == nil) {
              return;
          }
      }
      HaylenCancel cancel = handler(params, ^(BOOL ok, id _Nullable result) { answer(call, name, ok, result); });
      if (cancel == nil) {
          return;
      }
      @synchronized([HaylenBridge class]) {
          if (getCalls()[key] != nil) {
              getCalls()[key] = [cancel copy];
          }
      }
    });
}

void AppleBridge::cancel(std::uint64_t call) {
    id entry = nil;
    @synchronized([HaylenBridge class]) {
        entry = getCalls()[@(call)];
        [getCalls() removeObjectForKey:@(call)];
    }
    if (entry != nil && entry != NSNull.null) {
        HaylenCancel cancel = entry;
        dispatch_async(dispatch_get_main_queue(), cancel);
    }
}

void AppleBridge::emit(NSString* event, id payload) {
    const std::optional<std::string> json = toJson(payload);
    if (!json) {
        core::Log::error("The native event '{}' carried a payload that is not JSON and was dropped.", event.UTF8String);
        return;
    }
    BridgeRelay::emit(event.UTF8String, *json);
}

// Native code may register handlers before the engine starts, even before haylen_main, so the table exists from the first registration.
NSMutableDictionary<NSString*, HaylenCancellableHandler>* AppleBridge::getHandlers() {
    static NSMutableDictionary<NSString*, HaylenCancellableHandler>* table = [NSMutableDictionary dictionary];
    return table;
}

NSMutableDictionary<NSNumber*, id>* AppleBridge::getCalls() {
    static NSMutableDictionary<NSNumber*, id>* calls = [NSMutableDictionary dictionary];
    return calls;
}

void AppleBridge::registerBuiltIn(NSString* method, HaylenHandler handler) {
    @synchronized([HaylenBridge class]) {
        if (getHandlers()[method] == nil) {
            getHandlers()[method] = ^HaylenCancel(id params, HaylenReply reply) {
              handler(params, reply);
              return nil;
            };
        }
    }
}

// Returns the JSON text of a value, or nothing when JSON cannot hold it, which NSJSONSerialization reports with an exception.
std::optional<std::string> AppleBridge::toJson(id value) {
    if (value == nil) {
        return "null";
    }
    @try {
        NSData* data = [NSJSONSerialization dataWithJSONObject:value options:NSJSONWritingFragmentsAllowed error:nil];
        if (data == nil) {
            return std::nullopt;
        }
        return std::string(static_cast<const char*>(data.bytes), data.length);
    } @catch (NSException*) {
        return std::nullopt;
    }
}

id AppleBridge::fromJson(std::string_view text) {
    NSData* data = [NSData dataWithBytes:text.data() length:text.size()];
    return [NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingFragmentsAllowed error:nil];
}

NSString* AppleBridge::toString(std::string_view text) {
    return [[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding];
}

// Answers the first preferred language as a BCP 47 tag, the format every platform reports.
NSString* AppleBridge::getLanguageTag() {
    NSString* preferred = NSLocale.preferredLanguages.firstObject;
    return preferred != nil ? preferred : [NSLocale.currentLocale.localeIdentifier stringByReplacingOccurrencesOfString:@"_" withString:@"-"];
}

// A failure whose code or data JSON cannot hold fails with its message alone.
void AppleBridge::fail(std::uint64_t call, NSDictionary* failure) {
    const std::optional<std::string> json = toJson(failure);
    BridgeRelay::resolve(call, false, json ? *json : *toJson(@{@"message" : failure[@"message"]}));
}

// A call answers once, and not after it was cancelled. Failures reach the bridge as an object with a message and the code and data of the handler, and a success value that JSON cannot hold fails the call instead of answering with garbage.
void AppleBridge::answer(std::uint64_t call, NSString* method, BOOL ok, id result) {
    @synchronized([HaylenBridge class]) {
        if (getCalls()[@(call)] == nil) {
            return;
        }
        [getCalls() removeObjectForKey:@(call)];
    }

    if (!ok) {
        if ([result isKindOfClass:NSString.class]) {
            fail(call, @{@"message" : result});
            return;
        }
        NSMutableDictionary* failure = [result isKindOfClass:NSDictionary.class] ? [result mutableCopy] : [NSMutableDictionary dictionary];
        if (![failure[@"message"] isKindOfClass:NSString.class]) {
            failure[@"message"] = [NSString stringWithFormat:@"The native handler for %@ failed.", method];
        }
        fail(call, failure);
        return;
    }
    const std::optional<std::string> json = toJson(result);
    if (!json) {
        fail(call, @{@"message" : [NSString stringWithFormat:@"The native handler for %@ returned a value that is not JSON.", method]});
        return;
    }
    BridgeRelay::resolve(call, true, *json);
}

} // namespace haylen::platform
