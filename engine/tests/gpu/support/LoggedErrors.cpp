#include "gpu/support/LoggedErrors.hpp"

#include <gtest/gtest.h>

#include <algorithm>
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

bool LoggedErrors::take(std::string_view text) {
    const std::lock_guard lock(mutex);
    const auto found = std::ranges::find_if(errors, [text](const std::string& error) { return error.find(text) != std::string::npos; });
    if (found == errors.end()) {
        return false;
    }

    errors.erase(found);
    return true;
}

} // namespace haylen::test
