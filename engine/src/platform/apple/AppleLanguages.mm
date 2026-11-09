#import "platform/apple/AppleLanguages.hpp"

#import <Foundation/Foundation.h>

#include <string>
#include <vector>

#include "platform/NativeHost.hpp"
#include "platform/SystemState.hpp"

namespace haylen::platform {

void AppleLanguages::observe() {
    [NSNotificationCenter.defaultCenter addObserverForName:NSCurrentLocaleDidChangeNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*) { report(); }];
    report();
}

void AppleLanguages::report() {
    std::vector<std::string> languages;
    for (NSString* language in NSLocale.preferredLanguages) {
        languages.emplace_back(language.UTF8String);
    }
    NativeHost::getSystemState().setLanguages(languages);
}

} // namespace haylen::platform
