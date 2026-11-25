#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "2d/graphics/VectorAtlas.hpp"
#include "gpu/NullDevice.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/VectorImage.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::graphics2d {

class VectorAtlasTest : public ::testing::Test {
  protected:
    [[nodiscard]] static graphics::VectorImage parse(std::string_view svg) {
        return graphics::VectorImage::parse(std::span(reinterpret_cast<const std::uint8_t*>(svg.data()), svg.size()));
    }

    test::EngineFixture fixture;
};

// A page sends the GPU only the region of the rasters it placed since the last upload, so a new image costs its own pixels, never the whole page.
TEST_F(VectorAtlasTest, UploadsOnlyTheRegionOfTheRastersAPageTook) {
    fixture.frames(1);
    VectorAtlas atlas(fixture.engine().getGraphics(), fixture.engine().getJobs());
    const graphics::VectorImage first = parse(R"(<svg xmlns="http://www.w3.org/2000/svg" width="10" height="10"><rect width="10" height="10" fill="red"/></svg>)");
    (void)atlas.find(first, 1.0F, 1);
    atlas.stage();
    fixture.frames(1);
    const std::size_t pageBytes = std::size_t{VectorAtlas::kPageSize} * VectorAtlas::kPageSize * 4U;
    EXPECT_EQ(fixture.host().getGpu().getLastFrame().imageWriteBytes, pageBytes);

    const graphics::VectorImage second = parse(R"(<svg xmlns="http://www.w3.org/2000/svg" width="20" height="12"><rect width="20" height="12" fill="blue"/></svg>)");
    const VectorAtlas::Raster raster = atlas.find(second, 1.0F, 2);
    atlas.stage();
    fixture.frames(1);
    const auto rasterBytes = static_cast<std::size_t>(raster.source.width * raster.source.height) * 4U;
    EXPECT_EQ(fixture.host().getGpu().getLastFrame().imageWrites, 1);
    EXPECT_EQ(fixture.host().getGpu().getLastFrame().imageWriteBytes, rasterBytes);
    EXPECT_LT(rasterBytes, pageBytes);
}

} // namespace haylen::graphics2d
