#import "platform/apple/HaylenRequirement+Runtime.h"

// The kinds of requirements, which `kind` names.
typedef NS_ENUM(NSInteger, HaylenRequirementKind) {
    HaylenRequirementKindInfoPlistKey,
    HaylenRequirementKindUsageDescription,
    HaylenRequirementKindBackgroundMode,
    HaylenRequirementKindUrlScheme,
    HaylenRequirementKindClass,
    HaylenRequirementKindEntitlement,
};

@implementation HaylenRequirement {
    HaylenRequirementKind type;
    NSString* requirementName;
    NSString* requirementSummary;
    NSString* requirementFile;
    NSString* requirementSnippet;
}

- (instancetype)initWithType:(HaylenRequirementKind)kind name:(NSString*)name summary:(NSString*)summary file:(NSString*)file snippet:(NSString*)snippet {
    self = [super init];
    type = kind;
    requirementName = [name copy];
    requirementSummary = summary;
    requirementFile = file;
    requirementSnippet = snippet;
    return self;
}

+ (instancetype)infoPlistKey:(NSString*)key value:(NSString*)value {
    NSString* snippet = [self section:@"infoPlist" value:[NSString stringWithFormat:@"{%@: %@}", [self quoted:key], [self quoted:value]]];
    return [[self alloc] initWithType:HaylenRequirementKindInfoPlistKey name:key summary:[NSString stringWithFormat:@"the \"Info.plist\" key \"%@\"", key] file:self.appManifest snippet:snippet];
}

+ (instancetype)usageDescription:(NSString*)key {
    NSString* snippet = [self section:@"infoPlist" value:[NSString stringWithFormat:@"{%@: \"Tell the person why the app asks for this.\"}", [self quoted:key]]];
    return [[self alloc] initWithType:HaylenRequirementKindUsageDescription name:key summary:[NSString stringWithFormat:@"the usage description \"%@\"", key] file:self.appManifest snippet:snippet];
}

+ (instancetype)backgroundMode:(NSString*)mode {
    NSString* snippet = [self section:@"infoPlist" value:[NSString stringWithFormat:@"{\"UIBackgroundModes\": [%@]}", [self quoted:mode]]];
    return [[self alloc] initWithType:HaylenRequirementKindBackgroundMode name:mode summary:[NSString stringWithFormat:@"the background mode \"%@\"", mode] file:self.appManifest snippet:snippet];
}

+ (instancetype)urlScheme:(NSString*)scheme {
    NSString* snippet = [self section:@"infoPlist" value:[NSString stringWithFormat:@"{\"CFBundleURLTypes\": [{\"CFBundleURLSchemes\": [%@]}]}", [self quoted:scheme]]];
    return [[self alloc] initWithType:HaylenRequirementKindUrlScheme name:scheme summary:[NSString stringWithFormat:@"the URL scheme \"%@\"", scheme] file:self.appManifest snippet:snippet];
}

// Only a plugin links a framework, through a `framework` contribution of its `plugin.json`.
+ (instancetype)className:(NSString*)name framework:(NSString*)framework {
    NSString* summary = [NSString stringWithFormat:@"the class \"%@\" of \"%@\"", name, framework];
    NSString* snippet = [NSString stringWithFormat:@"{\"type\": \"framework\", \"platforms\": [\"%@\"], \"name\": %@}", self.platform, [self quoted:framework]];
    return [[self alloc] initWithType:HaylenRequirementKindClass name:name summary:summary file:@"plugin.json" snippet:snippet];
}

+ (instancetype)entitlement:(NSString*)key {
    NSString* snippet = [self section:@"entitlements" value:[NSString stringWithFormat:@"{%@: true}", [self quoted:key]]];
    return [[self alloc] initWithType:HaylenRequirementKindEntitlement name:key summary:[NSString stringWithFormat:@"the entitlement \"%@\"", key] file:self.appManifest snippet:snippet];
}

- (NSString*)kind {
    switch (type) {
    case HaylenRequirementKindInfoPlistKey:
        return @"infoPlistKey";
    case HaylenRequirementKindUsageDescription:
        return @"usageDescription";
    case HaylenRequirementKindBackgroundMode:
        return @"backgroundMode";
    case HaylenRequirementKindUrlScheme:
        return @"urlScheme";
    case HaylenRequirementKindClass:
        return @"class";
    case HaylenRequirementKindEntitlement:
        return @"entitlement";
    }
}

- (NSString*)name {
    return requirementName;
}

- (NSString*)summary {
    return requirementSummary;
}

- (NSString*)file {
    return requirementFile;
}

- (NSString*)snippet {
    return requirementSnippet;
}

- (NSString*)instructions {
    if (type == HaylenRequirementKindClass) {
        return [NSString stringWithFormat:@"Add the contribution \"%@\" to the \"contributions\" of \"plugin.json\" of the plugin.", requirementSnippet];
    }

    return [NSString stringWithFormat:@"Add \"%@\" to \"%@\".", requirementSnippet, requirementFile];
}

- (NSDictionary<NSString*, NSString*>*)entry {
    return @{@"kind" : self.kind, @"name" : self.name, @"file" : self.file, @"snippet" : self.snippet};
}

// Only macOS and Mac Catalyst tell an app its entitlements, so elsewhere the error of the API that needs one tells instead.
- (BOOL)isMetBy:(HaylenRequirements*)requirements {
    switch (type) {
    case HaylenRequirementKindInfoPlistKey:
        return [requirements hasInfoPlistKey:self.name];
    case HaylenRequirementKindUsageDescription:
        return [requirements hasUsageDescription:self.name];
    case HaylenRequirementKindBackgroundMode:
        return [requirements hasBackgroundMode:self.name];
    case HaylenRequirementKindUrlScheme:
        return [requirements hasURLScheme:self.name];
    case HaylenRequirementKindClass:
        return [requirements hasClass:self.name];
    case HaylenRequirementKindEntitlement:
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
        return [requirements hasEntitlement:self.name];
#else
        return YES;
#endif
    }
}

// The app gives the keys of its `Info.plist` and its entitlements in the section of its platform under `platforms` of `haylen.json`.
+ (NSString*)appManifest {
    return @"haylen.json";
}

+ (NSString*)section:(NSString*)key value:(NSString*)value {
    return [NSString stringWithFormat:@"{\"platforms\": {\"%@\": {\"%@\": %@}}}", self.platform, key, value];
}

// The name of the platform in `haylen.json` and `plugin.json`.
+ (NSString*)platform {
#if TARGET_OS_OSX
    return @"macos";
#elif TARGET_OS_MACCATALYST
    return @"catalyst";
#elif TARGET_OS_TV
    return @"tvos";
#elif TARGET_OS_VISION
    return @"visionos";
#else
    return @"ios";
#endif
}

+ (NSString*)quoted:(NSString*)text {
    NSData* json = [NSJSONSerialization dataWithJSONObject:text options:NSJSONWritingFragmentsAllowed | NSJSONWritingWithoutEscapingSlashes error:nil];
    return [[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding];
}

@end
