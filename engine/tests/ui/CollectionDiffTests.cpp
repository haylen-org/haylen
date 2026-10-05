#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ui/components/collections/CollectionDiff.hpp"

namespace haylen::ui {

TEST(CollectionDiffTest, ReportsInsertionsRemovalsAndTheItemsThatStay) {
    const std::vector<std::string_view> before{"a", "b", "c", "d"};
    const std::vector<std::string_view> after{"d", "a", "c", "e"};
    const CollectionDiff::Result result = CollectionDiff::compare(before, after, "list");
    EXPECT_EQ(result.previous, (std::vector<std::size_t>{3, 0, 2, CollectionDiff::kNew}));
    EXPECT_EQ(result.inserted, 1U);
    EXPECT_EQ(result.removed, 1U);
}

TEST(CollectionDiffTest, RejectsRepeatedIds) {
    const std::vector<std::string_view> before{"x"};
    const std::vector<std::string_view> after{"x", "y", "x"};
    try {
        (void)CollectionDiff::compare(before, after, "list");
        FAIL() << "A repeated id was accepted.";
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "The items of the collection \"list\" use the id \"x\" more than once.");
    }
}

TEST(CollectionDiffTest, ComparesOneHundredThousandIdsWithAFewChanges) {
    std::vector<std::string> ids(100000);
    for (std::size_t index = 0; index < ids.size(); ++index) {
        ids[index] = "item-" + std::to_string(index);
    }
    const std::vector<std::string_view> before(ids.begin(), ids.end());
    std::vector<std::string_view> after = before;
    std::swap(after[500], after[501]);

    const CollectionDiff::Result result = CollectionDiff::compare(before, after, "feed");
    EXPECT_EQ(result.inserted, 0U);
    EXPECT_EQ(result.removed, 0U);
    EXPECT_EQ(result.previous[500], 501U);
    EXPECT_EQ(result.previous[501], 500U);
    EXPECT_EQ(result.previous[99999], 99999U);
}

TEST(CollectionDiffTest, NeverMatchesItemsThatDidNotLoad) {
    const std::vector<std::string_view> before{"a", "", ""};
    const std::vector<std::string_view> after{"", "", "a"};
    const CollectionDiff::Result result = CollectionDiff::compare(before, after, "pages");
    EXPECT_EQ(result.previous, (std::vector<std::size_t>{CollectionDiff::kNew, CollectionDiff::kNew, 0}));
    EXPECT_EQ(result.removed, 0U);
}

} // namespace haylen::ui
