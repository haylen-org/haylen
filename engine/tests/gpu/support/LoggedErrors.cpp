#include "gpu/support/LoggedErrors.hpp"

#include <gtest/gtest.h>

#include <string_view>

#include "haylen/core/Log.hpp"

namespace haylen::test {

LoggedErrors::LoggedErrors() {
    // clang-format off
    listener = core::Log::addListener([this](core::Log::Level level, std::string_view line) {
        if (level == core::Log::Level::Error) {
            const std::lock_guard lock(mutex);
            errors.emplace_back(line);
        }
    });
    // clang-format on
}

LoggedErrors::~LoggedErrors() {
    core::Log::removeListener(listener);
}

void LoggedErrors::reportAsFailures() {
    const std::lock_guard lock(mutex);
    for (const std::string& error : errors) {
        ADD_FAILURE() << "An error reached the log: " << error;
    }
    errors.clear();
}

} // namespace haylen::test
