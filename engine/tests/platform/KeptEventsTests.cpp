#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "platform/KeptEvents.hpp"

namespace haylen::platform {

TEST(KeptEventsTest, KeepsTheNewestEventsOfEachNameInTheOrderTheyCame) {
    KeptEvents kept;
    for (std::size_t index = 0; index < KeptEvents::kLimitPerName + 8; ++index) {
        kept.keep({.name = "sensor.sample", .json = std::to_string(index)});

        if (index == 3) {
            kept.keep({.name = "links.opened", .json = "\"first\""});
        }
    }

    kept.keep({.name = "links.opened", .json = "\"second\"", .options = {.retain = true}});
    EXPECT_EQ(kept.size(), KeptEvents::kLimitPerName + 2);

    const std::vector<KeptEvents::Event> events = kept.take();
    std::vector<std::string> samples;
    std::vector<std::string> links;
    for (const KeptEvents::Event& event : events) {
        std::vector<std::string>& list = event.name == "links.opened" ? links : samples;
        list.push_back(event.json);
    }

    ASSERT_EQ(samples.size(), KeptEvents::kLimitPerName);
    EXPECT_EQ(samples.front(), "8");
    EXPECT_EQ(samples.back(), std::to_string(KeptEvents::kLimitPerName + 7));
    EXPECT_EQ(links, (std::vector<std::string>{"\"first\"", "\"second\""}));
    ASSERT_FALSE(events.empty());
    EXPECT_TRUE(events.back().options.retain);
    EXPECT_EQ(kept.size(), 0U);
}

} // namespace haylen::platform
