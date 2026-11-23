#include <gtest/gtest.h>

#include <string>

#include "debug/DebuggerTest.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::lua::debug {

class DebuggerWebTest : public DebuggerTest {};

// A web page cannot halt, so it keeps the log points and says why the other breakpoints never take effect.
TEST_F(DebuggerWebTest, KeepsOnlyTheLogPointsOnAPageThatCannotHalt) {
    fixture.host().setHaltable(false);
    client.connect(fixture.lua());
    EXPECT_TRUE(client.getResponse(1).at("body").at("exceptionBreakpointFilters").empty());
    const int set = client.request("setBreakpoints", breakpoints({{{"line", 14}}, {{"line", 16}, {"logMessage", "sum {sum}"}}}));
    const int pause = client.request("pause");
    client.getDebugger().update(fixture.lua());
    const haylen::core::Json results = client.getResponse(set).at("body").at("breakpoints");
    EXPECT_EQ(results.at(0).at("verified"), false);
    EXPECT_NE(results.at(0).at("message").get<std::string>().find("cannot halt"), std::string::npos);
    EXPECT_EQ(results.at(1).at("verified"), true);
    EXPECT_FALSE(client.getResponse(pause).at("success").get<bool>());

    EXPECT_EQ(run(fixture.lua(), "require('mod').loop()"), "");
    EXPECT_TRUE(client.getEvents("stopped").empty());
    ASSERT_FALSE(client.getEvents("output").empty());
    EXPECT_EQ(client.getEvents("output").back().at("output"), "sum 21\n");
}

} // namespace haylen::lua::debug
