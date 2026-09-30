#pragma once

#import "haylen/platform/apple/HaylenPlugin.h"

#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::platform {

// The native parts of the plugins of the app, which the runtime creates while the app launches from the classes that the `plugin.json` files of the bundled package name, in load order, and hands the events of the app on the main thread.
class ApplePlugins final {
  public:
    // A plugin answers an event that takes a completion handler with bits that join the answers of the other plugins.
    using Answer = void (^)(NSUInteger answer);
    using Ask = void (^)(id<HaylenPlugin> plugin, Answer answer);
    using Done = void (^)(NSUInteger answers);

    // Creates the class of every plugin that the bundled package lists for this destination, with the parameters of the plugin, and loads it with its context. A destination leaves out the classes of the plugins that do not list it, such as Mac Catalyst the ones for iOS alone, and those plugins run without their native part there, while a class that a listed destination lacks is logged with what the project lacks.
    static void load();

    // The ids of the loaded plugins, which run their native part.
    [[nodiscard]] static std::vector<std::string> getIds();

    // The loaded plugins that implement a method, in load order.
    [[nodiscard]] static NSArray<id<HaylenPlugin>>* getPlugins(SEL selector);

    // Asks every plugin that implements the method, passing each an answer of its own, and calls done once with the union of the answers after the last plugin answered, or at once with none when no plugin implements it. A second answer of a plugin is ignored.
    static void join(SEL selector, Done done, Ask ask);

    // Hands the report of an error that stopped the app to every plugin that takes app errors, on the main queue.
    static void reportError(const core::Json& report);

    // Ends the covers that the plugins left open, when the window of the app goes away.
    static void closeCovers();

  private:
    // What `plugin.json` and `app.json` say about a plugin with a class.
    struct Declaration {
        std::string identifier;
        std::string className;
        core::Json config;
        bool supported = false;
    };

    // The plugins of `app.json` whose `plugin.json` names a class in its `apple` section, in load order, or none when the bundle holds no package, as the desktop player, which runs the package that its command line names. Throws when the bundled package, its `app.json` or a `plugin.json` cannot be read.
    [[nodiscard]] static std::vector<Declaration> readDeclarations();

    // The destination this build runs on, as plugin.json names platforms.
    [[nodiscard]] static std::string_view getDestination() noexcept;

    static NSMutableArray<id<HaylenPlugin>>* plugins;
    static NSMutableArray<HaylenPluginContext*>* contexts;
};

} // namespace haylen::platform
