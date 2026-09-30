#import "platform/apple/HaylenRequirements+Runtime.h"

#include <TargetConditionals.h>

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
#include <Security/SecTask.h>
#include <dlfcn.h>
#endif

#include "haylen/core/Log.hpp"
#import "platform/apple/HaylenRequirement+Runtime.h"

@implementation HaylenRequirements {
    NSString* owner;
}

- (instancetype)initWithOwner:(NSString*)value {
    self = [super init];
    owner = [value copy];
    return self;
}

- (BOOL)hasInfoPlistKey:(NSString*)key {
    return NSBundle.mainBundle.infoDictionary[key] != nil;
}

- (BOOL)hasUsageDescription:(NSString*)key {
    id text = NSBundle.mainBundle.infoDictionary[key];
    return [text isKindOfClass:NSString.class] && [text length] > 0;
}

- (BOOL)hasBackgroundMode:(NSString*)mode {
    id modes = NSBundle.mainBundle.infoDictionary[@"UIBackgroundModes"];
    return [modes isKindOfClass:NSArray.class] && [modes containsObject:mode];
}

- (BOOL)hasURLScheme:(NSString*)scheme {
    id types = NSBundle.mainBundle.infoDictionary[@"CFBundleURLTypes"];
    if (![types isKindOfClass:NSArray.class]) {
        return NO;
    }
    for (id type in types) {
        id schemes = [type isKindOfClass:NSDictionary.class] ? type[@"CFBundleURLSchemes"] : nil;
        if (![schemes isKindOfClass:NSArray.class]) {
            continue;
        }
        for (id declared in schemes) {
            if ([declared isKindOfClass:NSString.class] && [declared caseInsensitiveCompare:scheme] == NSOrderedSame) {
                return YES;
            }
        }
    }
    return NO;
}

- (BOOL)hasClass:(NSString*)name {
    return NSClassFromString(name) != nil;
}

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
// The Security framework reads the entitlements of the process. The runtime loads it the first time a plugin asks, so apps need not link it.
- (BOOL)hasEntitlement:(NSString*)key {
    static void* const security = dlopen("/System/Library/Frameworks/Security.framework/Security", RTLD_LAZY | RTLD_LOCAL);
    static const auto createTask = reinterpret_cast<decltype(&SecTaskCreateFromSelf)>(dlsym(security, "SecTaskCreateFromSelf"));
    static const auto copyValue = reinterpret_cast<decltype(&SecTaskCopyValueForEntitlement)>(dlsym(security, "SecTaskCopyValueForEntitlement"));
    SecTaskRef task = createTask(kCFAllocatorDefault);
    if (task == nullptr) {
        return NO;
    }
    id value = CFBridgingRelease(copyValue(task, (__bridge CFStringRef)key, nullptr));
    CFRelease(task);
    return value != nil && ![value isEqual:@NO];
}
#endif

- (NSArray<HaylenRequirement*>*)missing:(NSArray<HaylenRequirement*>*)requirements {
    NSMutableArray<HaylenRequirement*>* missing = [NSMutableArray array];
    for (HaylenRequirement* requirement in requirements) {
        if (![requirement isMetBy:self]) {
            [missing addObject:requirement];
        }
    }
    return missing;
}

- (BOOL)require:(NSArray<HaylenRequirement*>*)requirements error:(NSError**)error {
    NSArray<HaylenRequirement*>* missing = [self missing:requirements];
    if (missing.count == 0) {
        return YES;
    }

    NSMutableArray<NSString*>* summaries = [NSMutableArray array];
    NSMutableArray<NSDictionary<NSString*, NSString*>*>* entries = [NSMutableArray array];
    for (HaylenRequirement* requirement in missing) {
        [self report:requirement];
        [summaries addObject:requirement.summary];
        [entries addObject:requirement.entry];
    }
    NSString* last = summaries.lastObject;
    [summaries removeLastObject];
    NSString* list = summaries.count == 0 ? last : [NSString stringWithFormat:@"%@ and %@", [summaries componentsJoinedByString:@", "], last];
    NSString* message = [NSString stringWithFormat:@"%@ needs %@, which the app lacks.", owner, list];
    if (error != nullptr) {
        *error = [NSError errorWithDomain:@"HaylenRequirements" code:0 userInfo:@{NSLocalizedDescriptionKey : message, @"message" : message, @"code" : @"unsupported", @"data" : @{@"missing" : entries}}];
    }
    return NO;
}

// Logs once per process what the owner needs, what happens without it and how to add it.
- (void)report:(HaylenRequirement*)requirement {
    static NSMutableSet<NSString*>* const logged = [NSMutableSet set];
    NSString* key = [NSString stringWithFormat:@"%@\n%@\n%@", owner, requirement.kind, requirement.name];
    @synchronized(logged) {
        if ([logged containsObject:key]) {
            return;
        }
        [logged addObject:key];
    }
    haylen::core::Log::warning("{} needs {}, which the app lacks, so the calls that need it fail with the code \"unsupported\". Add \"{}\" to \"{}\".", owner.UTF8String, requirement.summary.UTF8String, requirement.snippet.UTF8String, requirement.file.UTF8String);
}

@end
