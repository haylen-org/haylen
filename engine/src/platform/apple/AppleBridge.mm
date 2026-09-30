#import "platform/apple/AppleBridge.hpp"

#include "haylen/core/Log.hpp"
#include "platform/BridgeRelay.hpp"

namespace haylen::platform {

std::vector<AppleBridge::WaitingEvent>& AppleBridge::waiting = *new std::vector<WaitingEvent>();
bool AppleBridge::running = false;

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
        fail(call, @{@"message" : [NSString stringWithFormat:@"No native handler is registered for %@.", name], @"code" : @"noHandler"});
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

void AppleBridge::emit(NSString* event, id payload, bool retain) {
    const std::optional<std::string> json = toJson(payload);
    if (!json) {
        core::Log::error("The native event '{}' carried a payload that is not JSON and was dropped.", event.UTF8String);
        return;
    }
    @synchronized([HaylenBridge class]) {
        if (!running) {
            waiting.push_back({.event = event.UTF8String, .payload = *json, .retain = retain});
            return;
        }
    }
    BridgeRelay::emit(event.UTF8String, *json, retain);
}

// The events reach the bridge under the lock, so an event that another thread sends meanwhile never overtakes them.
void AppleBridge::setAppRunning(bool value) {
    @synchronized([HaylenBridge class]) {
        running = value;
        if (!running) {
            return;
        }
        for (const WaitingEvent& entry : waiting) {
            BridgeRelay::emit(entry.event, entry.payload, entry.retain);
        }
        waiting.clear();
    }
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
