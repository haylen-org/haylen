#import "platform/apple/ApplePlugins.hpp"

#include <algorithm>
#include <exception>
#include <memory>

#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/platform/AppPlugin.hpp"
#include "platform/Services.hpp"
#import "platform/apple/AppleBridge.hpp"
#import "platform/apple/HaylenPluginContext+Runtime.h"

namespace haylen::platform {

NSMutableArray<id<HaylenPlugin>>* ApplePlugins::plugins = [NSMutableArray array];
NSMutableArray<HaylenPluginContext*>* ApplePlugins::contexts = [NSMutableArray array];

void ApplePlugins::load() {
    NSArray<NSString*>* classes = NSBundle.mainBundle.infoDictionary[@"HaylenPlugins"];
    if (classes.count == 0) {
        return;
    }
    std::map<std::string, Declaration, std::less<>> declarations;
    try {
        declarations = readDeclarations();
    } catch (const std::exception& error) {
        core::Log::error("The native parts of the plugins do not run, because the plugins of the app could not be read. {}", error.what());
        return;
    }

    for (NSString* name in classes) {
        const auto found = declarations.find(name.UTF8String);
        if (found == declarations.end()) {
            core::Log::error("The Info.plist names the plugin class {}, which no plugin that app.json lists declares in the apple section of its plugin.json.", name.UTF8String);
            continue;
        }
        const Declaration& declaration = found->second;
        // A destination leaves out the classes of the plugins that do not list it on purpose, so only a plugin that lists it misses its class by mistake.
        Class type = NSClassFromString(name);
        if (type == nil) {
            if (declaration.supported) {
                core::Log::error("The class {} of the plugin {} is missing from the app, so its native part does not run. A Swift class names its Objective-C class with @objc({}).", name.UTF8String, declaration.identifier, name.UTF8String);
            }
            continue;
        }
        if (![type conformsToProtocol:@protocol(HaylenPlugin)]) {
            core::Log::error("The class {} of the plugin {} does not conform to the HaylenPlugin protocol, so its native part does not run.", name.UTF8String, declaration.identifier);
            continue;
        }

        id<HaylenPlugin> plugin = [[type alloc] init];
        HaylenPluginContext* context = [[HaylenPluginContext alloc] initWithIdentifier:@(declaration.identifier.c_str()) config:AppleBridge::fromJson(declaration.config.dump())];
        [plugins addObject:plugin];
        [contexts addObject:context];
        [plugin loadWithContext:context];
    }
}

std::vector<std::string> ApplePlugins::getIds() {
    std::vector<std::string> ids;
    for (HaylenPluginContext* context in contexts) {
        ids.emplace_back(context.identifier.UTF8String);
    }
    return ids;
}

NSArray<id<HaylenPlugin>>* ApplePlugins::getPlugins(SEL selector) {
    NSMutableArray<id<HaylenPlugin>>* found = [NSMutableArray array];
    for (id<HaylenPlugin> plugin in plugins) {
        if ([plugin respondsToSelector:selector]) {
            [found addObject:plugin];
        }
    }
    return found;
}

// The answers may come from any thread, so the lock guards the count and the union.
void ApplePlugins::join(SEL selector, Done done, Ask ask) {
    NSArray<id<HaylenPlugin>>* asked = getPlugins(selector);
    if (asked.count == 0) {
        done(0);
        return;
    }

    NSObject* lock = [[NSObject alloc] init];
    __block NSUInteger pending = asked.count;
    __block NSUInteger answers = 0;
    for (id<HaylenPlugin> plugin in asked) {
        __block BOOL answered = NO;
        ask(plugin, ^(NSUInteger answer) {
          BOOL last = NO;
          @synchronized(lock) {
              if (answered) {
                  return;
              }
              answered = YES;
              answers |= answer;
              last = --pending == 0;
          }
          if (last) {
              done(answers);
          }
        });
    }
}

void ApplePlugins::presentNotification(UNUserNotificationCenter* center, UNNotification* notification, void (^completionHandler)(UNNotificationPresentationOptions)) {
    // clang-format off
    join(@selector(userNotificationCenter:willPresentNotification:withCompletionHandler:), ^(NSUInteger options) { completionHandler(options); }, ^(id<HaylenPlugin> plugin, Answer answer) {
        [plugin userNotificationCenter:center willPresentNotification:notification withCompletionHandler:^(UNNotificationPresentationOptions options) { answer(options); }];
    });
    // clang-format on
}

#if !TARGET_OS_TV
void ApplePlugins::receiveNotificationResponse(UNUserNotificationCenter* center, UNNotificationResponse* response, void (^completionHandler)(void)) {
    // clang-format off
    join(@selector(userNotificationCenter:didReceiveNotificationResponse:withCompletionHandler:), ^(NSUInteger) { completionHandler(); }, ^(id<HaylenPlugin> plugin, Answer answer) {
        [plugin userNotificationCenter:center didReceiveNotificationResponse:response withCompletionHandler:^{ answer(0); }];
    });
    // clang-format on
}
#endif

// Script errors may quote bytes that are not UTF-8, which reach the plugins replaced instead of failing the report.
void ApplePlugins::reportError(const core::Json& report) {
    NSDictionary<NSString*, id>* error = AppleBridge::fromJson(report.dump(-1, ' ', false, core::Json::error_handler_t::replace));
    dispatch_async(dispatch_get_main_queue(), ^{
      for (id<HaylenPlugin> plugin in getPlugins(@selector(appDidFailWithError:))) {
          [plugin appDidFailWithError:error];
      }
    });
}

void ApplePlugins::closeCovers() {
    for (HaylenPluginContext* context in contexts) {
        [context closeCovers];
    }
}

std::map<std::string, ApplePlugins::Declaration, std::less<>> ApplePlugins::readDeclarations() {
    const std::shared_ptr<io::Package> package = Services::openBundledPackage();
    const core::AppConfig config = core::AppConfig::fromPackage(*package);
    std::map<std::string, Declaration, std::less<>> declarations;
    for (const auto& [identifier, values] : config.plugins.items()) {
        const core::Json manifest = core::Json::parse(package->readText(io::Path::plugin(identifier, io::Path::kPluginManifestFile)));
        const core::Json apple = manifest.value("apple", core::Json::object());
        if (!apple.contains("class")) {
            continue;
        }
        const core::Json platforms = manifest.value("platforms", core::Json::array());
        const bool supported = std::ranges::find(platforms, core::Json(getDestination())) != platforms.end();
        declarations[apple.at("class").get<std::string>()] = {.identifier = identifier, .config = AppPlugin::read(*package, identifier, values).config, .supported = supported};
    }
    return declarations;
}

std::string_view ApplePlugins::getDestination() noexcept {
#if TARGET_OS_MACCATALYST
    return "catalyst";
#elif TARGET_OS_TV
    return "tvos";
#elif TARGET_OS_OSX
    return "macos";
#else
    return "ios";
#endif
}

} // namespace haylen::platform
