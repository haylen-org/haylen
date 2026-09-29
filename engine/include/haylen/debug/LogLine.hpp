#pragma once

#include <string>

#include "haylen/core/Log.hpp"

namespace haylen::debug {

// One printed log line with its level, as the debug overlay and haylen.debug show it.
struct LogLine {
    core::Log::Level level = core::Log::Level::Info;
    std::string text;
};

} // namespace haylen::debug
