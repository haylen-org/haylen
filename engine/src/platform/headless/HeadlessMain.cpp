#include <cstdio>
#include <optional>

#include "platform/headless/HeadlessPlayer.hpp"

int main(int argc, char** argv) {
    const std::optional<haylen::platform::HeadlessPlayer::Options> options = haylen::platform::HeadlessPlayer::parse(argc, argv);
    if (!options) {
        std::fprintf(stderr, "Usage: \"haylen-headless <app folder or zip> [--frames <count>] [--dev]\".\n");
        return 2;
    }
    return haylen::platform::HeadlessPlayer::run(*options);
}
