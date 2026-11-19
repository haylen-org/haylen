#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "platform/native/NativeSignature.hpp"

namespace haylen::platform {

class NativeSignatureTest : public ::testing::Test {
  protected:
    using Kind = NativeSignature::Parameter::Kind;

    [[nodiscard]] static std::string failureOf(std::string_view declaration) {
        try {
            (void)NativeSignature::parse(declaration);
        } catch (const std::invalid_argument& error) {
            return error.what();
        }

        return {};
    }
};

TEST_F(NativeSignatureTest, ReadsNumbersPointersTextAndRangesOfBytes) {
    const NativeSignature signature = NativeSignature::parse("void (int32_t code, const char* text, const uint8_t data[size], size_t size, struct Session* session, double ratio, bool ready, unsigned long long big, float scale, const int16_t pair[2], char const * label, char* buffer, enum Mode mode)");
    const std::vector<NativeSignature::Parameter>& parameters = signature.getParameters();
    ASSERT_EQ(parameters.size(), 13U);

    EXPECT_EQ(parameters[0].kind, Kind::Integer);
    EXPECT_EQ(parameters[0].size, 4U);
    EXPECT_TRUE(parameters[0].isSigned);
    EXPECT_EQ(parameters[0].name, "code");
    EXPECT_EQ(parameters[1].kind, Kind::Text);
    EXPECT_EQ(parameters[2].kind, Kind::Bytes);
    EXPECT_EQ(parameters[2].size, 1U);
    EXPECT_EQ(parameters[2].lengthParameter, 3U);
    EXPECT_EQ(parameters[3].kind, Kind::Integer);
    EXPECT_FALSE(parameters[3].isSigned);
    EXPECT_EQ(parameters[3].size, sizeof(std::size_t));
    EXPECT_EQ(parameters[4].kind, Kind::Pointer);
    EXPECT_EQ(parameters[5].kind, Kind::Double);
    EXPECT_EQ(parameters[6].kind, Kind::Boolean);
    EXPECT_EQ(parameters[7].size, 8U);
    EXPECT_FALSE(parameters[7].isSigned);
    EXPECT_EQ(parameters[8].kind, Kind::Float);
    EXPECT_EQ(parameters[9].kind, Kind::Bytes);
    EXPECT_EQ(parameters[9].size, 2U);
    EXPECT_EQ(parameters[9].count, 2U);
    EXPECT_FALSE(parameters[9].lengthParameter.has_value());
    EXPECT_EQ(parameters[10].kind, Kind::Text);
    EXPECT_EQ(parameters[11].kind, Kind::Pointer) << "Only constant text is copied as a string.";
    EXPECT_EQ(parameters[12].kind, Kind::Integer);

    EXPECT_TRUE(NativeSignature::parse("void (void)").getParameters().empty());
    EXPECT_TRUE(NativeSignature::parse("void()").getParameters().empty());
    const NativeSignature unsignedCount = NativeSignature::parse("void (unsigned count)");
    ASSERT_EQ(unsignedCount.getParameters().size(), 1U);
    EXPECT_EQ(unsignedCount.getParameters()[0].name, "count");
}

TEST_F(NativeSignatureTest, RejectsWhatACallbackCannotReceive) {
    EXPECT_NE(failureOf("int (int value)").find("returns nothing"), std::string::npos);
    EXPECT_NE(failureOf("void* (int value)").find("returns nothing"), std::string::npos);
    EXPECT_NE(failureOf("void (Session session)").find("not a C number type"), std::string::npos);
    EXPECT_NE(failureOf("void (struct Session session)").find("not a C number type"), std::string::npos);
    EXPECT_NE(failureOf("void (void value)").find("not a C number type"), std::string::npos);
    EXPECT_NE(failureOf("void (const void data[4])").find("does not know"), std::string::npos);
    EXPECT_NE(failureOf("void (const uint8_t data[size], double size)").find("must name an integer parameter"), std::string::npos);
    EXPECT_NE(failureOf("void (const uint8_t data[missing])").find("must name an integer parameter"), std::string::npos);
    EXPECT_NE(failureOf("void (const uint8_t data[])").find("needs a count"), std::string::npos);
    EXPECT_NE(failureOf("void (uint8_t data[").find("needs a count"), std::string::npos);
    EXPECT_NE(failureOf("void (long double value)").find("unknown type \"long double\""), std::string::npos);
    EXPECT_NE(failureOf("void (int value) extra").find("continues after its parameters"), std::string::npos);
    EXPECT_NE(failureOf("void (int value; int other)").find("unexpected character \";\""), std::string::npos);
    EXPECT_NE(failureOf("void (int first int second)").find("unknown type \"int first int\""), std::string::npos);
    EXPECT_NE(failureOf("void (int first[2] int second)").find("\"int\" where a comma or \")\" belongs"), std::string::npos);
    EXPECT_NE(failureOf("void (int value,)").find("a comma without a parameter"), std::string::npos);
    EXPECT_NE(failureOf("void (int value").find("ends before the \")\""), std::string::npos);
    EXPECT_NE(failureOf("void (,)").find("has no type"), std::string::npos);
}

} // namespace haylen::platform
