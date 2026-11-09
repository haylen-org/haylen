#include "platform/LaunchOptions.hpp"

#include <string_view>

namespace haylen::platform {

// Options of the player start with two dashes, and other options with two dashes are left to the system without a value.
LaunchOptions LaunchOptions::parse(int argc, char* argv[]) {
    LaunchOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        const bool hasValue = index + 1 < argc;
        if (argument == "--dev") {
            options.development = true;
        } else if (argument == "--dev-server" && hasValue) {
            options.development = true;
            options.developmentServer = argv[++index];
        } else if (argument == "--native" && hasValue) {
            options.nativeFolders.emplace_back(argv[++index]);
        } else if (argument == "--graphics-api" && hasValue) {
            options.graphicsApi = argv[++index];
        } else if (argument == "--link" && hasValue) {
            options.link = argv[++index];
        } else if (argument.starts_with('-') && !argument.starts_with("--") && hasValue) {
            ++index;
        } else if (!argument.starts_with('-') && options.package.empty()) {
            options.package = argument;
        }
    }
    return options;
}

} // namespace haylen::platform
