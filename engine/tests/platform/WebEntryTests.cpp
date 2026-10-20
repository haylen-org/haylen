#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Log.hpp"
#include "platform/web/WebEntry.hpp"

namespace haylen::platform {

// A call of the page that fails becomes an engine error that names it, and no exception reaches JavaScript.
TEST(WebEntryTest, TurnsAFailedPageCallIntoAnErrorOfTheLog) {
    std::vector<std::string> errors;
    // clang-format off
    const std::uint64_t listener = core::Log::addListener([&errors](core::Log::Level level, std::string_view line) {
        if (level == core::Log::Level::Error) {
            errors.emplace_back(line);
        }
    });
    // clang-format on
    bool ran = false;
    WebEntry::guard("haylen_web_restart", [&ran] { ran = true; });
    EXPECT_NO_THROW(WebEntry::guard("haylen_web_emit", [] { throw std::runtime_error("The payload is not valid JSON."); }));
    core::Log::removeListener(listener);

    EXPECT_TRUE(ran);
    ASSERT_EQ(errors.size(), 1U);
    EXPECT_NE(errors[0].find("haylen_web_emit"), std::string::npos) << errors[0];
    EXPECT_NE(errors[0].find("The payload is not valid JSON."), std::string::npos) << errors[0];
}

} // namespace haylen::platform
