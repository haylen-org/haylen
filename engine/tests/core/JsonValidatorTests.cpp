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

    // Returns the sentence that refuses the text of a file read with a bound of nesting, or an empty text when it parses.
    [[nodiscard]] static std::string refusalWithin(std::string_view text, std::size_t maxDepth) {
        try {
            (void)JsonValidator::parse(text, "models/ship.gltf", maxDepth);
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

// A number too large for the reader names its file, line and column like a mistake of syntax, while a number too small to tell from zero and digits inside strings read.
TEST_F(JsonValidatorTest, NamesTheLineAndColumnOfANumberTooLargeToRead) {
    EXPECT_EQ(refusal("{\n  \"note\": \"1e999\",\n  \"scale\": [1, 0.833e333333333334]\n}"), "The file \"maps/cave.json\" holds a number too large to read. Check line 3, column 16.");
    EXPECT_EQ(refusal("-1e400"), "The file \"maps/cave.json\" holds a number too large to read. Check line 1, column 1.");
    EXPECT_EQ(refusal("{\"note\": \"1e999\", \"tiny\": 1e-400}"), "");
}

// A missing key and a value of the wrong type keep only the key and the types from the words of the reader.
TEST_F(JsonValidatorTest, DescribesAMissingKeyAndAValueOfTheWrongType) {
    const Json map = Json::parse(R"({"width": "wide", "layers": {}})");
    EXPECT_EQ(description([&map] { (void)map.at("height"); }), "The asset \"maps/cave.json\" lacks the key \"height\".");
    EXPECT_EQ(description([&map] { (void)map.at("width").get<int>(); }), "The asset \"maps/cave.json\" holds a string where a number belongs.");
    EXPECT_EQ(description([&map] { (void)map.at("layers").get<std::string>(); }), "The asset \"maps/cave.json\" holds an object where a string belongs.");
    EXPECT_EQ(description([&map] { (void)map.at("width").at(2); }), "The asset \"maps/cave.json\" holds a value that is out of range or of another kind than it needs.");
}

// A document from a file of anywhere nests at most the levels its reader names, so nothing that walks it recurses without bound, and one that nests deeper names the line and column of the bracket too deep, while brackets inside strings never count.
TEST_F(JsonValidatorTest, RefusesADocumentThatNestsDeeperThanTheBound) {
    EXPECT_EQ(JsonValidator::parse(R"({"a": [{"b": "[[[[\"[["}]})", "models/ship.gltf", 3).at("a").at(0).at("b"), "[[[[\"[[");
    EXPECT_EQ(refusalWithin("{\"a\": [\n  {\"b\": [1]}]}", 3), "The file \"models/ship.gltf\" nests its objects and arrays more than 3 levels deep, more than the engine reads. Check line 2, column 9.");
    EXPECT_EQ(refusalWithin(std::string(100000, '[') + std::string(100000, ']'), 128), "The file \"models/ship.gltf\" nests its objects and arrays more than 128 levels deep, more than the engine reads. Check line 1, column 129.");
    EXPECT_EQ(refusalWithin("{\"a\": [1, 2", 3), "The file \"models/ship.gltf\" is not valid JSON, since it ends before its last value is complete.");
}

} // namespace haylen::core
