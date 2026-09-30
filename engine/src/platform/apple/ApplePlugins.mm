#import "platform/apple/ApplePlugins.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <memory>

#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/platform/AppPlugin.hpp"
#include "platform/PluginLoadOrder.hpp"
#include "platform/Services.hpp"
#import "platform/apple/AppleBridge.hpp"
#import "platform/apple/HaylenPluginContext+Runtime.h"

namespace haylen::platform {

NSMutableArray<id<HaylenPlugin>>* ApplePlugins::plugins = [NSMutableArray array];
NSMutableArray<HaylenPluginContext*>* ApplePlugins::contexts = [NSMutableArray array];

void ApplePlugins::load() {
    std::vector<Declaration> declarations;
    try {
        declarations = readDeclarations();
    } catch (const std::exception& error) {
        core::Log::error("The native parts of the plugins do not run, because the plugins of the app could not be read. {}", error.what());
        return;
    }

    for (const Declaration& declaration : declarations) {
        // A destination leaves out the classes of the plugins that do not list it on purpose, so only a plugin that lists it misses its class by mistake.
        if (!declaration.supported) {
            continue;
        }
        const char* name = declaration.className.c_str();
        Class type = NSClassFromString(@(name));
        if (type == nil) {
            core::Log::error("The class \"{}\" of the plugin \"{}\" is missing from the app, so the plugin runs without its native part. The project lacks the Apple sources of the plugin, which \"project.yml\" compiles into the target through \"include: [plugins.json]\", or a Swift class of them names itself without \"@objc({})\".", name, declaration.identifier, name);
            continue;
        }
        if (![type conformsToProtocol:@protocol(HaylenPlugin)]) {
            core::Log::error("The class \"{}\" of the plugin \"{}\" does not conform to the \"HaylenPlugin\" protocol, so the plugin runs without its native part.", name, declaration.identifier);
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

std::vector<ApplePlugins::Declaration> ApplePlugins::readDeclarations() {
    const std::filesystem::path resources = NSBundle.mainBundle.resourcePath.UTF8String;
    if (!std::filesystem::exists(resources / "app") && !std::filesystem::exists(resources / "app.zip")) {
        return {};
    }
    const std::shared_ptr<io::Package> package = Services::openBundledPackage();
    const core::AppConfig config = core::AppConfig::fromPackage(*package);
    std::vector<Declaration> declarations;
    for (const std::string& identifier : PluginLoadOrder::read(*package)) {
        const core::Json manifest = core::Json::parse(package->readText(io::Path::plugin(identifier, io::Path::kPluginManifestFile)));
        const core::Json apple = manifest.value("apple", core::Json::object());
        if (!apple.contains("class")) {
            continue;
        }
        const core::Json platforms = manifest.value("platforms", core::Json::array());
        const bool supported = std::ranges::find(platforms, core::Json(getDestination())) != platforms.end();
        declarations.push_back({.identifier = identifier, .className = apple.at("class").get<std::string>(), .config = AppPlugin::read(*package, identifier, config.plugins.at(identifier)).config, .supported = supported});
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
