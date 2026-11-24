#include <gtest/gtest.h>

#include <stdexcept>

#include "haylen/plugins/TextPlugin.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::text {

TEST(TextPluginTest, RequiresAStartedEngine) {
    test::EngineFixture fixture;
    haylen::plugins::TextPlugin plugin(fixture.host());
    EXPECT_THROW((void)plugin.getRegistry(), std::logic_error);
}

} // namespace haylen::text
