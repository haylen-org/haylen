#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

#include "haylen/3d/gltf/AccessorReader.hpp"
#include "haylen/3d/gltf/Reader.hpp"
#include "support/GltfFiles.hpp"

namespace haylen::gltf {

class AccessorReaderTest : public ::testing::Test {
  protected:
    [[nodiscard]] static Document triangle() {
        return Reader::read(test::GltfFiles::glb(test::GltfFiles::triangle()), "models/triangle.glb");
    }
};

// The accessors of a document read as the numbers they store, floats as floats and indices as unsigned integers.
TEST_F(AccessorReaderTest, ReadsFloatsAndUnsignedIntegers) {
    const Document document = triangle();
    EXPECT_EQ(AccessorReader::readFloats(document, 0), (std::vector<float>{0, 0, 0, 1, 0, 0, 0, 1, 0}));
    EXPECT_EQ(AccessorReader::readUnsigned(document, 2), (std::vector<std::uint32_t>{0, 1, 2}));
    EXPECT_EQ(AccessorReader::readFloats(document, 2), (std::vector<float>{0, 1, 2}));
}

// Unsigned integers come only from accessors of unsigned integers that are not normalized, and an accessor the document lacks is out of range.
TEST_F(AccessorReaderTest, RefusesAccessorsOfAnotherKind) {
    const Document document = triangle();
    EXPECT_THROW((void)AccessorReader::readUnsigned(document, 0), std::invalid_argument);
    EXPECT_THROW((void)AccessorReader::readFloats(document, 3), std::out_of_range);
}

// A document that code changed after the reader checked it never reads outside its bytes: an accessor past its buffer view, a buffer view past its buffer, sparse indices that do not grow or pass the count, and a count beyond the bytes of a buffer all throw.
TEST_F(AccessorReaderTest, NeverReadsOutsideTheBytesOfAChangedDocument) {
    Document document = triangle();
    document.accessors[0].count = 4;
    EXPECT_THROW((void)AccessorReader::readFloats(document, 0), std::invalid_argument);

    document = triangle();
    document.bufferViews[0].byteLength = 1U << 20U;
    EXPECT_THROW((void)AccessorReader::readFloats(document, 0), std::invalid_argument);

    document = triangle();
    document.buffers[0].data.resize(8);
    EXPECT_THROW((void)AccessorReader::readUnsigned(document, 2), std::invalid_argument);

    document = triangle();
    document.accessors[1].bufferView.reset();
    document.accessors[1].count = std::uint64_t{1} << 40U;
    EXPECT_THROW((void)AccessorReader::readFloats(document, 1), std::invalid_argument);

    // Sparse indices of the third accessor replace elements of the second, from the index data of the triangle.
    document = triangle();
    document.accessors[1].sparse = Accessor::Sparse{.count = 2, .indicesBufferView = 2, .indicesByteOffset = 2, .indicesComponentType = Accessor::ComponentType::UnsignedShort, .valuesBufferView = 0, .valuesByteOffset = 0, .extras = {}, .extensions = {}};
    EXPECT_EQ(AccessorReader::readFloats(document, 1), (std::vector<float>{0, 0, 1, 0, 0, 0, 1, 0, 0}));
    document.accessors[1].sparse->indicesByteOffset = 0;
    document.accessors[1].sparse->count = 3;
    EXPECT_NO_THROW((void)AccessorReader::readFloats(document, 1));
    document.buffers[0].data[document.bufferViews[2].byteOffset + 2] = 0;
    EXPECT_THROW((void)AccessorReader::readFloats(document, 1), std::invalid_argument);
    document.buffers[0].data[document.bufferViews[2].byteOffset + 2] = 7;
    EXPECT_THROW((void)AccessorReader::readFloats(document, 1), std::invalid_argument);
}

} // namespace haylen::gltf
