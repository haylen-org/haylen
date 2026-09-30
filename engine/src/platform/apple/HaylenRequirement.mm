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
    NSString* snippet = [NSString stringWithFormat:@"<key>%@</key><string>%@</string>", key, value];
    return [[self alloc] initWithType:HaylenRequirementKindInfoPlistKey name:key summary:[NSString stringWithFormat:@"the \"Info.plist\" key \"%@\"", key] file:self.infoPlistFile snippet:snippet];
}

+ (instancetype)usageDescription:(NSString*)key {
    NSString* snippet = [NSString stringWithFormat:@"<key>%@</key><string>Tell the person why the app asks for this.</string>", key];
    return [[self alloc] initWithType:HaylenRequirementKindUsageDescription name:key summary:[NSString stringWithFormat:@"the usage description \"%@\"", key] file:self.infoPlistFile snippet:snippet];
}

+ (instancetype)backgroundMode:(NSString*)mode {
    NSString* snippet = [NSString stringWithFormat:@"<key>UIBackgroundModes</key><array><string>%@</string></array>", mode];
    return [[self alloc] initWithType:HaylenRequirementKindBackgroundMode name:mode summary:[NSString stringWithFormat:@"the background mode \"%@\"", mode] file:self.infoPlistFile snippet:snippet];
}

+ (instancetype)urlScheme:(NSString*)scheme {
    NSString* snippet = [NSString stringWithFormat:@"<key>CFBundleURLTypes</key><array><dict><key>CFBundleURLSchemes</key><array><string>%@</string></array></dict></array>", scheme];
    return [[self alloc] initWithType:HaylenRequirementKindUrlScheme name:scheme summary:[NSString stringWithFormat:@"the URL scheme \"%@\"", scheme] file:self.infoPlistFile snippet:snippet];
}

// The target of the app lists the frameworks it links among the dependencies of `project.yml`.
+ (instancetype)className:(NSString*)name framework:(NSString*)framework {
    NSString* summary = [NSString stringWithFormat:@"the class \"%@\" of \"%@\"", name, framework];
    return [[self alloc] initWithType:HaylenRequirementKindClass name:name summary:summary file:@"project.yml" snippet:[NSString stringWithFormat:@"- sdk: %@", framework]];
}

+ (instancetype)entitlement:(NSString*)key {
    NSString* snippet = [NSString stringWithFormat:@"<key>%@</key><true/>", key];
    return [[self alloc] initWithType:HaylenRequirementKindEntitlement name:key summary:[NSString stringWithFormat:@"the entitlement \"%@\"", key] file:self.entitlementsFile snippet:snippet];
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

// The iOS target builds iOS and Mac Catalyst from one `Info.plist`, while each platform signs with entitlements of its own.
+ (NSString*)infoPlistFile {
#if TARGET_OS_OSX
    return @"macos/Info.plist";
#elif TARGET_OS_TV
    return @"tvos/Info.plist";
#else
    return @"ios/Info.plist";
#endif
}

+ (NSString*)entitlementsFile {
#if TARGET_OS_OSX
    return @"macos/App.entitlements";
#elif TARGET_OS_MACCATALYST
    return @"catalyst/App.entitlements";
#elif TARGET_OS_TV
    return @"tvos/App.entitlements";
#else
    return @"ios/App.entitlements";
#endif
}

@end
