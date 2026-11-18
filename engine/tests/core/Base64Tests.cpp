#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "core/Base64.hpp"

namespace haylen::core {

TEST(Base64Test, DecodesPaddedAndUnpaddedText) {
    EXPECT_EQ(Base64::decode("UE9SVA=="), (std::vector<std::uint8_t>{'P', 'O', 'R', 'T'}));
    EXPECT_EQ(Base64::decode("UE9SVA"), (std::vector<std::uint8_t>{'P', 'O', 'R', 'T'}));
    EXPECT_EQ(Base64::decode("UE9S\n VEU="), (std::vector<std::uint8_t>{'P', 'O', 'R', 'T', 'E'}));
    EXPECT_EQ(Base64::decode(""), std::vector<std::uint8_t>{});
}

// A last group of one character holds less than a byte, and padding only ends the text and fills its last group, so neither hides a truncated or joined text.
TEST(Base64Test, RefusesTextThatIsNotBase64) {
    EXPECT_EQ(Base64::decode("A"), std::nullopt);
    EXPECT_EQ(Base64::decode("UE9SVAA=B"), std::nullopt);
    EXPECT_EQ(Base64::decode("UE==VA=="), std::nullopt);
    EXPECT_EQ(Base64::decode("UE9SVA="), std::nullopt);
    EXPECT_EQ(Base64::decode("UE9S===="), std::nullopt);
    EXPECT_EQ(Base64::decode("UE9S@A=="), std::nullopt);
}

} // namespace haylen::core
