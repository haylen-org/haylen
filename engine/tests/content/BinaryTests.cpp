#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "content/format/BinaryReader.hpp"
#include "content/format/BinaryWriter.hpp"
#include "haylen/content/Error.hpp"

namespace haylen::content {

TEST(BinaryReaderTest, NeverMovesPastItsBytes) {
    const std::vector<std::uint8_t> bytes = {2, 0, 'a', 'b', 0};
    BinaryReader reader(bytes, Error::Code::CorruptIndex, "The test structure");
    try {
        reader.skip(bytes.size() + 1);
        FAIL() << "A skip past the end must fail.";
    } catch (const Error& error) {
        EXPECT_EQ(error.getCode(), Error::Code::CorruptIndex);
        EXPECT_EQ(std::string(error.what()), "The test structure is damaged or was not made by this version of Haylen.");
    }

    EXPECT_EQ(reader.getPosition(), 0U);
    EXPECT_EQ(reader.readString(2), "ab");
    EXPECT_THROW(reader.requireEnd(), Error);
    reader.skipZeros(1);
    reader.requireEnd();
}

TEST(BinaryWriterTest, RefusesStringsLongerThanTheirLengthField) {
    BinaryWriter writer;
    EXPECT_THROW(writer.writeString(std::string(65536, 'a')), std::length_error);
    writer.writeString(std::string(65535, 'a'));
    EXPECT_EQ(writer.getBytes().size(), 65537U);
}

} // namespace haylen::content
