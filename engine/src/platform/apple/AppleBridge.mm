#import "platform/apple/AppleBridge.hpp"

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

#include "haylen/core/Log.hpp"
#include "platform/sokol/BridgeRelay.hpp"

namespace haylen::platform {

void AppleBridge::setHandler(NSString* method, HaylenHandler handler) {
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

void AppleBridge::dispatch(std::uint64_t call, std::string_view method, std::string_view paramsJson) {
    NSString* name = toString(method);
    HaylenHandler handler = nil;
    @synchronized([HaylenBridge class]) {
        handler = getHandlers()[name];
    }
    if (handler == nil) {
        fail(call, [NSString stringWithFormat:@"No native handler is registered for %@.", name]);
        return;
    }

    id parsed = fromJson(paramsJson);
    id params = parsed != nil ? parsed : @{};
    dispatch_async(dispatch_get_main_queue(), ^{ handler(params, ^(BOOL ok, id _Nullable result) { answer(call, name, ok, result); }); });
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
NSMutableDictionary<NSString*, HaylenHandler>* AppleBridge::getHandlers() {
    static NSMutableDictionary<NSString*, HaylenHandler>* table = [NSMutableDictionary dictionary];
    return table;
}

void AppleBridge::registerBuiltIn(NSString* method, HaylenHandler handler) {
    @synchronized([HaylenBridge class]) {
        if (getHandlers()[method] == nil) {
            getHandlers()[method] = [handler copy];
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

void AppleBridge::fail(std::uint64_t call, NSString* message) {
    BridgeRelay::resolve(call, false, *toJson(@{@"message" : message}));
}

// Failures reach the bridge as an object with a message, and a success value that JSON cannot hold fails the call instead of answering with garbage.
void AppleBridge::answer(std::uint64_t call, NSString* method, BOOL ok, id result) {
    if (!ok) {
        const bool described = [result isKindOfClass:NSDictionary.class] && [result[@"message"] isKindOfClass:NSString.class];
        fail(call, [result isKindOfClass:NSString.class] ? result : (described ? result[@"message"] : [NSString stringWithFormat:@"The native handler for %@ failed.", method]));
        return;
    }
    const std::optional<std::string> json = toJson(result);
    if (!json) {
        fail(call, [NSString stringWithFormat:@"The native handler for %@ returned a value that is not JSON.", method]);
        return;
    }
    BridgeRelay::resolve(call, true, *json);
}

} // namespace haylen::platform
