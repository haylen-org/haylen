#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <string_view>

#include "haylen/core/JsonValidator.hpp"

namespace haylen::core {

class JsonValidatorTest : public ::testing::Test {
  protected:
    // Returns the sentence that refuses the text of a file, or an empty text when it parses.
    [[nodiscard]] static std::string refusal(std::string_view text) {
        try {
            (void)JsonValidator::parse(text, "maps/cave.json");
        } catch (const std::invalid_argument& error) {
            return error.what();
        }

        return {};
    }

    // Returns the sentence that describes the failure of reading a value, or an empty text when it reads.
    template <typename Read> [[nodiscard]] static std::string description(Read&& read) {
        try {
            read();
        } catch (const Json::exception& failure) {
            return JsonValidator::describe(failure, {}, "The asset \"maps/cave.json\"");
        }

        return {};
    }
};

// A mistake of syntax names its file, line and column instead of the words of the JSON reader.
TEST_F(JsonValidatorTest, NamesTheLineAndColumnOfASyntaxMistake) {
    EXPECT_EQ(refusal("{\n  \"width\": @\n}"), "The file \"maps/cave.json\" is not valid JSON. Check line 2, column 12.");
    EXPECT_EQ(refusal("{\"width\": 3"), "The file \"maps/cave.json\" is not valid JSON, since it ends before its last value is complete.");
    EXPECT_EQ(refusal("{\"width\": 3}"), "");
}

// A missing key and a value of the wrong type keep only the key and the types from the words of the reader.
TEST_F(JsonValidatorTest, DescribesAMissingKeyAndAValueOfTheWrongType) {
    const Json map = Json::parse(R"({"width": "wide", "layers": {}})");
    EXPECT_EQ(description([&map] { (void)map.at("height"); }), "The asset \"maps/cave.json\" lacks the key \"height\".");
    EXPECT_EQ(description([&map] { (void)map.at("width").get<int>(); }), "The asset \"maps/cave.json\" holds a string where a number belongs.");
    EXPECT_EQ(description([&map] { (void)map.at("layers").get<std::string>(); }), "The asset \"maps/cave.json\" holds an object where a string belongs.");
    EXPECT_EQ(description([&map] { (void)map.at("width").at(2); }), "The asset \"maps/cave.json\" holds a value that is out of range or of another kind than it needs.");
}

} // namespace haylen::core
