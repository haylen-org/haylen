#import "platform/apple/AppleBridge.hpp"

#include <utility>

#include "haylen/core/JsonBytes.hpp"
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
void AppleBridge::dispatch(std::uint64_t call, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers) {
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
        fail(call, @{@"message" : [NSString stringWithFormat:@"No native handler is registered for \"%@\".", name], @"code" : @"noHandler"});
        return;
    }

    id parsed = decode(paramsJson, buffers);
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

void AppleBridge::emit(NSString* event, id payload, const Bridge::EmitOptions& options) {
    std::optional<Encoded> encoded = encode(payload);
    if (!encoded) {
        core::Log::error("The native event '{}' carried a payload that is not JSON and was dropped.", event.UTF8String);
        return;
    }
    @synchronized([HaylenBridge class]) {
        if (!running) {
            waiting.push_back({.event = event.UTF8String, .payload = std::move(*encoded), .options = options});
            return;
        }
    }
    BridgeRelay::emit(event.UTF8String, encoded->json, std::move(encoded->buffers), options);
}

// The events reach the bridge under the lock, so an event that another thread sends meanwhile never overtakes them.
void AppleBridge::setAppRunning(bool value) {
    @synchronized([HaylenBridge class]) {
        running = value;
        if (!running) {
            return;
        }
        for (WaitingEvent& entry : waiting) {
            BridgeRelay::emit(entry.event, entry.payload.json, std::move(entry.payload.buffers), entry.options);
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

std::optional<AppleBridge::Encoded> AppleBridge::encode(id value) {
    if (value == nil) {
        return Encoded{.json = "null"};
    }
    Encoded encoded;
    id prepared = prepare(value, encoded.buffers);
    @try {
        NSData* data = [NSJSONSerialization dataWithJSONObject:prepared options:NSJSONWritingFragmentsAllowed error:nil];
        if (data == nil) {
            return std::nullopt;
        }
        encoded.json.assign(static_cast<const char*>(data.bytes), data.length);
        return encoded;
    } @catch (NSException*) {
        return std::nullopt;
    }
}

id AppleBridge::prepare(id value, std::vector<std::vector<std::byte>>& buffers) {
    if ([value isKindOfClass:NSData.class]) {
        NSData* data = value;
        const auto* bytes = static_cast<const std::byte*>(data.bytes);
        buffers.emplace_back(bytes, bytes + data.length);
        return @{@(core::JsonBytes::kKey) : @(buffers.size() - 1)};
    }
    if ([value isKindOfClass:NSDictionary.class]) {
        NSDictionary* source = value;
        NSMutableDictionary* object = [NSMutableDictionary dictionaryWithCapacity:source.count];
        for (id key in source) {
            object[key] = prepare(source[key], buffers);
        }
        return object;
    }
    if ([value isKindOfClass:NSArray.class]) {
        NSArray* source = value;
        NSMutableArray* array = [NSMutableArray arrayWithCapacity:source.count];
        for (id element in source) {
            [array addObject:prepare(element, buffers)];
        }
        return array;
    }
    return value;
}

// A reference is an object whose only key is $bytes with an integer, never a boolean or a fraction.
id AppleBridge::restore(id value, NSArray<NSData*>* buffers) {
    if ([value isKindOfClass:NSArray.class]) {
        NSMutableArray* array = [NSMutableArray arrayWithCapacity:[value count]];
        for (id element in value) {
            [array addObject:restore(element, buffers)];
        }
        return array;
    }
    if (![value isKindOfClass:NSDictionary.class]) {
        return value;
    }
    NSDictionary* source = value;
    id index = source.count == 1 ? source[@(core::JsonBytes::kKey)] : nil;
    if (index != nil && CFGetTypeID((__bridge CFTypeRef)index) == CFNumberGetTypeID() && !CFNumberIsFloatType((__bridge CFNumberRef)index) && [index longLongValue] >= 0 && [index unsignedLongLongValue] < buffers.count) {
        return buffers[[index unsignedIntegerValue]];
    }
    NSMutableDictionary* object = [NSMutableDictionary dictionaryWithCapacity:source.count];
    for (id key in source) {
        object[key] = restore(source[key], buffers);
    }
    return object;
}

id AppleBridge::fromJson(std::string_view text) {
    NSData* data = [NSData dataWithBytes:text.data() length:text.size()];
    return [NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingFragmentsAllowed error:nil];
}

id AppleBridge::decode(std::string_view json, std::span<const std::vector<std::byte>> buffers) {
    id parsed = fromJson(json);
    if (parsed == nil) {
        return nil;
    }
    NSMutableArray<NSData*>* datas = [NSMutableArray arrayWithCapacity:buffers.size()];
    for (const std::vector<std::byte>& buffer : buffers) {
        [datas addObject:[NSData dataWithBytes:buffer.data() length:buffer.size()]];
    }
    return restore(parsed, datas);
}

NSString* AppleBridge::toString(std::string_view text) {
    return [[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding];
}

// A failure whose code or data JSON cannot hold fails with its message alone.
void AppleBridge::fail(std::uint64_t call, NSDictionary* failure) {
    const std::optional<Encoded> encoded = encode(failure);
    BridgeRelay::resolve(call, false, encoded ? encoded->json : encode(@{@"message" : failure[@"message"]})->json);
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
            failure[@"message"] = [NSString stringWithFormat:@"The native handler for \"%@\" failed.", method];
        }
        fail(call, failure);
        return;
    }
    std::optional<Encoded> encoded = encode(result);
    if (!encoded) {
        fail(call, @{@"message" : [NSString stringWithFormat:@"The native handler for \"%@\" returned a value that is not JSON.", method]});
        return;
    }
    BridgeRelay::resolve(call, true, encoded->json, std::move(encoded->buffers));
}

} // namespace haylen::platform
