#pragma once

#include <string>
#include <vector>

namespace haylen::platform {

// What the command line of a launch asks the player for: the package to play, or none for the bundled one, whether it plays in development, which only `--dev` turns on and shipped apps never pass, the address of the development server that `--dev-server` gives, the folders where `native.load` looks first, which `--native` names, the graphics API that `--graphics-api` names, which the web page passes the same way, and the link of `--link` that opened the app. Options of the system start with one dash and take a value, such as the `-AppleLanguages (pt-BR)` of a launch on Apple platforms or the `-NSDocumentRevisionsDebugMode YES` that Xcode passes, so the player leaves both to the system.
struct LaunchOptions {
    std::string package;
    bool development = false;
    std::string developmentServer;
    std::vector<std::string> nativeFolders;
    std::string graphicsApi;
    std::string link;

    [[nodiscard]] static LaunchOptions parse(int argc, char* argv[]);
};

} // namespace haylen::platform
