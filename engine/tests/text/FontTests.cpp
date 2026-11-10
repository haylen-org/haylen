#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <hb-raster.h>
#include <hb.h>

#include "core/EmbeddedFiles.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/text/Font.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/text/TrueTypeFont.hpp"
#include "math/HalfFloat.hpp"
#include "support/DamagedFonts.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"
#include "text/DistanceField.hpp"
#include "text/FontFile.hpp"

namespace haylen::text {

namespace {

// Reads the test fonts next to the built-in font, and opens them with HarfBuzz to compare against.
class FontFileTest : public ::testing::Test {
  protected:
    // The pixels that the rasterizer of HarfBuzz covers for a glyph, in rows from the top, with the place of its top left corner from the pen on the baseline.
    struct Coverage {
        std::vector<std::uint8_t> pixels;
        int width = 0;
        int height = 0;
        int x = 0;
        int y = 0;
    };

    [[nodiscard]] static std::vector<std::uint8_t> read(std::string_view name) {
        if (name == "default") {
            const std::span<const std::uint8_t> data = core::EmbeddedFiles::getDefaultFont();
            return {data.begin(), data.end()};
        }
        std::ifstream file(std::string(HAYLEN_TEST_FONTS) + "/" + std::string(name), std::ios::binary);
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    [[nodiscard]] static FontFile open(const std::vector<std::uint8_t>& data) {
        std::optional<FontFile> file = FontFile::read(data);
        if (!file) {
            throw std::runtime_error("The test font does not open.");
        }
        return std::move(*file);
    }

    [[nodiscard]] static float scaleFor(const FontFile& file, float size) {
        return size / static_cast<float>(file.getUnitsPerEm());
    }

    [[nodiscard]] static Coverage cover(const FontFile& file, std::uint32_t glyph, float scale) {
        const std::unique_ptr<hb_raster_draw_t, decltype(&hb_raster_draw_destroy)> raster(hb_raster_draw_create_or_fail(), &hb_raster_draw_destroy);
        hb_raster_draw_set_transform(raster.get(), scale, 0.0F, 0.0F, scale, 0.0F, 0.0F);
        hb_raster_draw_glyph(raster.get(), &file.getFont(), glyph);
        const std::unique_ptr<hb_raster_image_t, decltype(&hb_raster_image_destroy)> image(hb_raster_draw_render(raster.get()), &hb_raster_image_destroy);
        hb_raster_extents_t extents{};
        hb_raster_image_get_extents(image.get(), &extents);
        Coverage coverage{.width = static_cast<int>(extents.width), .height = static_cast<int>(extents.height), .x = extents.x_origin, .y = -(extents.y_origin + static_cast<int>(extents.height))};
        const std::span<const std::uint8_t> rows(hb_raster_image_get_buffer(image.get()), static_cast<std::size_t>(extents.stride) * extents.height);
        for (int row = coverage.height - 1; row >= 0; --row) {
            const std::span<const std::uint8_t> from = rows.subspan(static_cast<std::size_t>(row) * extents.stride, static_cast<std::size_t>(coverage.width));
            coverage.pixels.insert(coverage.pixels.end(), from.begin(), from.end());
        }
        return coverage;
    }

    // Reads a distance of a field, a half float from 0 to 1.
    [[nodiscard]] static float toFloat(std::uint16_t bits) {
        const auto exponent = static_cast<int>(bits >> 10U);
        const auto mantissa = static_cast<float>(bits & 0x3FFU);
        return exponent == 0 ? std::ldexp(mantissa, -24) : std::ldexp(1.0F + mantissa / 1024.0F, exponent - 15);
    }

    // Fills the pixels of a field that the glyph covers once a synthetic bold moves it left and right by the weight, reading the field between its pixels along each row.
    [[nodiscard]] static std::vector<bool> spread(const DistanceField& field, float weight) {
        // clang-format off
        const auto read = [&field](int row, float x) {
            const float clamped = std::clamp(x, 0.0F, static_cast<float>(field.width - 1));
            const int left = static_cast<int>(clamped);
            const int right = std::min(left + 1, field.width - 1);
            const float share = clamped - static_cast<float>(left);
            const std::size_t start = static_cast<std::size_t>(row) * static_cast<std::size_t>(field.width);
            return toFloat(field.distances[start + static_cast<std::size_t>(left)]) * (1.0F - share) + toFloat(field.distances[start + static_cast<std::size_t>(right)]) * share;
        };
        // clang-format on
        std::vector<bool> filled(field.distances.size());
        for (int row = 0; row < field.height; ++row) {
            for (int column = 0; column < field.width; ++column) {
                const float x = static_cast<float>(column);
                filled[static_cast<std::size_t>(row * field.width + column)] = std::max(read(row, x - weight), read(row, x + weight)) > 0.5F;
            }
        }
        return filled;
    }

    // Counts the regions of pixels that share a value, joined through their sides, and leaves out the regions that touch the border when `inner` is set, which counts the holes of a shape.
    [[nodiscard]] static int countRegions(const std::vector<bool>& mask, int width, int height, bool value, bool inner) {
        std::vector<bool> seen(mask.size(), false);
        std::vector<int> pending;
        int regions = 0;
        for (int first = 0; first < width * height; ++first) {
            if (seen[static_cast<std::size_t>(first)] || mask[static_cast<std::size_t>(first)] != value) {
                continue;
            }
            bool touches = false;
            pending.assign(1, first);
            seen[static_cast<std::size_t>(first)] = true;
            while (!pending.empty()) {
                const int at = pending.back();
                pending.pop_back();
                const int x = at % width;
                const int y = at / width;
                touches = touches || x == 0 || y == 0 || x == width - 1 || y == height - 1;
                for (const auto [dx, dy] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}}) {
                    const int nx = x + dx;
                    const int ny = y + dy;
                    const std::size_t next = static_cast<std::size_t>(ny * width + nx);
                    if (nx >= 0 && ny >= 0 && nx < width && ny < height && !seen[next] && mask[next] == value) {
                        seen[next] = true;
                        pending.push_back(ny * width + nx);
                    }
                }
            }
            regions += inner && touches ? 0 : 1;
        }
        return regions;
    }
};

} // namespace

// A minimum scale lays text out at the largest size that fits its box, keeps text that fits at its size, and takes the smallest size for text that never fits.
TEST(FontTest, FitsTextInItsBox) {
    test::EngineFixture fixture;
    text::Font& font = *fixture.engine().getDefaultFont();
    const std::string_view title = "Treasure of the deep";
    const float natural = font.layout(title, {.size = 40.0F})->size.x;

    const std::shared_ptr<const text::Layout> line = font.layout(title, {.size = 40.0F, .maxWidth = natural * 0.6F, .minScale = 0.25F, .wrap = false});
    EXPECT_EQ(line->lineCount, 1U);
    EXPECT_LE(line->size.x, natural * 0.6F);
    EXPECT_GE(line->size.x, natural * 0.58F);

    const std::string_view story = "The tide rose over the reef while the crew counted the coins of the old wreck.";
    const std::shared_ptr<const text::Layout> paragraph = font.layout(story, {.size = 40.0F, .maxWidth = 300.0F, .maxHeight = 120.0F, .minScale = 0.25F});
    EXPECT_GT(paragraph->lineCount, 1U);
    EXPECT_LE(paragraph->size.y, 120.0F);
    EXPECT_GT(font.layout(story, {.size = 40.0F, .maxWidth = 300.0F})->size.y, 120.0F);

    EXPECT_EQ(font.layout(title, {.size = 40.0F, .maxWidth = natural + 10.0F, .minScale = 0.5F, .wrap = false})->size, font.layout(title, {.size = 40.0F})->size);
    EXPECT_EQ(font.layout(title, {.size = 40.0F, .maxWidth = 10.0F, .minScale = 0.5F, .wrap = false})->size, font.layout(title, {.size = 20.0F})->size);
}

TEST(FontTest, LaysOutWrapsAndAlignsText) {
    test::EngineFixture fixture;
    text::Font& font = *fixture.engine().getDefaultFont();

    const std::shared_ptr<const text::Layout> single = font.layout("Hello", {.size = 32.0F});
    EXPECT_EQ(single->lineCount, 1U);
    EXPECT_EQ(single->glyphs.size(), 5U);
    EXPECT_EQ(single->characters.size(), 5U);
    EXPECT_GT(single->size.x, 40.0F);
    EXPECT_NEAR(single->size.y, font.getLineHeight(32.0F), 0.01F);

    const std::shared_ptr<const text::Layout> wrapped = font.layout("one two three four five", {.size = 32.0F, .maxWidth = 120.0F});
    EXPECT_GT(wrapped->lineCount, 2U);
    EXPECT_FLOAT_EQ(wrapped->size.x, 120.0F);

    const std::shared_ptr<const text::Layout> lines = font.layout("a\n\nb", {.size = 20.0F});
    EXPECT_EQ(lines->lineCount, 3U);
    EXPECT_EQ(lines->characters[1].begin, 3U);

    const std::shared_ptr<const text::Layout> centered = font.layout("i\nwide text", {.size = 20.0F, .align = text::Alignment::Center});
    const std::shared_ptr<const text::Layout> right = font.layout("i\nwide text", {.size = 20.0F, .align = text::Alignment::Right});
    EXPECT_GT(centered->glyphs.front().position.x, 0.0F);
    EXPECT_GT(right->glyphs.front().position.x, centered->glyphs.front().position.x);
    EXPECT_EQ(font.measure("Hello", {.size = 32.0F}), single->size);
    EXPECT_GT(font.getAscent(32.0F), 0.0F);
    EXPECT_GT(font.toDistance(2.0F, 32.0F), 0.0F);
    EXPECT_FLOAT_EQ(font.toDistance(4.0F, 32.0F), font.toDistance(2.0F, 16.0F));

    const std::shared_ptr<const text::Layout> unicode = font.layout("Olá 世界", {.size = 24.0F});
    EXPECT_GE(unicode->glyphs.size(), 3U);
    font.sync();
    EXPECT_TRUE(font.getPage(0).isValid());

    EXPECT_THROW(text::TrueTypeFont(fixture.engine().getGraphics(), test::TestFiles::bytes("not a font")), std::runtime_error);
    EXPECT_THROW(text::TrueTypeFont(fixture.engine().getGraphics(), {}), std::runtime_error);
    EXPECT_THROW(text::TrueTypeFont(fixture.engine().getGraphics(), {0, 1, 0}), std::runtime_error);
}

// Plain text draws a line under itself or through itself across every line it wraps into, and a layout without the lines stays apart in the cache.
TEST(FontTest, UnderlinesAndStrikesPlainText) {
    test::EngineFixture fixture;
    text::Font& font = *fixture.engine().getDefaultFont();
    const auto count = [](const text::Layout& layout, text::Layout::Box::Kind kind) { return std::ranges::count(layout.boxes, kind, &text::Layout::Box::kind); };
    const std::shared_ptr<const text::Layout> plain = font.layout("Read me twice", {.size = 32.0F, .maxWidth = 140.0F});
    const std::shared_ptr<const text::Layout> underlined = font.layout("Read me twice", {.size = 32.0F, .maxWidth = 140.0F, .underline = true});
    const std::shared_ptr<const text::Layout> struck = font.layout("Read me twice", {.size = 32.0F, .strike = true});
    EXPECT_EQ(count(*plain, text::Layout::Box::Kind::Underline), 0);
    EXPECT_EQ(count(*underlined, text::Layout::Box::Kind::Underline), static_cast<std::ptrdiff_t>(underlined->lineCount));
    EXPECT_GT(underlined->lineCount, 1U);
    EXPECT_EQ(count(*struck, text::Layout::Box::Kind::Strike), 1);
    EXPECT_EQ(underlined->size, plain->size);
}

TEST(FontTest, GrowsTheAtlasWhenGlyphsDoNotFit) {
    test::EngineFixture fixture;
    const std::span<const std::uint8_t> data = core::EmbeddedFiles::getDefaultFont();
    text::TrueTypeFont font(fixture.engine().getGraphics(), std::vector<std::uint8_t>(data.begin(), data.end()), {.bakeSize = 48.0F, .spread = 6, .atlasSize = 64});

    // Glyphs that fit replace the pixels of the page in place, and a grown atlas becomes a new page, so text queued before keeps the image its coordinates were measured on.
    (void)font.layout(".", {.size = 48.0F});
    font.sync();
    const graphics::Texture first = font.getPage(0);
    (void)font.layout(",", {.size = 48.0F});
    font.sync();
    EXPECT_EQ(font.getPage(0), first);

    const std::shared_ptr<const text::Layout> layout = font.layout("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", {.size = 48.0F});
    font.sync();
    EXPECT_EQ(layout->glyphs.size(), 62U);
    EXPECT_NE(font.getPage(0), first);
    EXPECT_EQ(first.getWidth(), 64);
    EXPECT_GT(font.getPage(0).getWidth(), 64);
    EXPECT_GT(font.getPage(0).getHeight(), 64);

    for (const text::Layout::Glyph& glyph : layout->glyphs) {
        EXPECT_LE(glyph.source.x + glyph.source.width, static_cast<float>(font.getPage(0).getWidth()));
        EXPECT_LE(glyph.source.y + glyph.source.height, static_cast<float>(font.getPage(0).getHeight()));
    }
}

// Every draw that adds glyphs hands the atlas to the device, and the frame sends it to the GPU once, in place.
TEST(FontTest, UploadsItsAtlasOncePerFrameForTheGlyphsOfEveryDraw) {
    test::EngineFixture fixture;
    fixture.frames(1);
    const std::span<const std::uint8_t> data = core::EmbeddedFiles::getDefaultFont();
    text::TrueTypeFont font(fixture.engine().getGraphics(), std::vector<std::uint8_t>(data.begin(), data.end()));
    fixture.frames(1);
    const graphics::Texture page = font.getPage(0);
    const std::uint32_t image = page.getResource()->image.id;

    for (const char* line : {"Harbor", "Lighthouse", "Quay 42", "Market", "Pier & dock", "Zephyr"}) {
        (void)font.layout(line, {.size = 24.0F});
        font.sync();
    }
    fixture.frames(1);
    EXPECT_EQ(fixture.host().getGpu().getLastFrame().imageWrites, 1);
    EXPECT_EQ(fixture.host().getGpu().getLastFrame().imageWriteBytes, static_cast<std::size_t>(page.getWidth() * page.getHeight()) * sizeof(std::uint16_t));
    EXPECT_EQ(font.getPage(0).getResource()->image.id, image);
}

// A glyph wider than any texture of the device fails every time it is drawn, rather than drawing nothing after the first failure, and it fails before its distance field is computed.
TEST(FontTest, FailsAgainForAGlyphNoAtlasHolds) {
    test::EngineFixture fixture;
    graphics::Device& device = fixture.engine().getGraphics();
    const int limit = device.getMaxTextureSize();
    const std::span<const std::uint8_t> data = core::EmbeddedFiles::getDefaultFont();
    text::TrueTypeFont font(device, std::vector<std::uint8_t>(data.begin(), data.end()), {.bakeSize = static_cast<float>(limit), .spread = limit / 8});
    EXPECT_THROW((void)font.layout("W", {.size = 32.0F}), std::runtime_error);
    EXPECT_THROW((void)font.layout("W", {.size = 32.0F}), std::runtime_error);
    EXPECT_THROW((void)font.getGlyph(100000), std::out_of_range);

    const std::vector<std::uint8_t> bytes(data.begin(), data.end());
    EXPECT_THROW(text::TrueTypeFont(device, bytes, {.bakeSize = static_cast<float>(limit) * 2.0F}), std::invalid_argument);
    EXPECT_THROW(text::TrueTypeFont(device, bytes, {.spread = -1}), std::invalid_argument);
    EXPECT_THROW(text::TrueTypeFont(device, bytes, {.atlasSize = 0}), std::invalid_argument);
}

// A synthetic bold spreads strokes along the baseline by at most a quarter of the counter of the letter o, so a heavy face keeps the dot of its i apart from the stem and the counter of its o open, while a face with wide counters takes the whole weight.
TEST_F(FontFileTest, KeepsTheHolesOfLettersInSyntheticBold) {
    test::EngineFixture fixture;
    const std::vector<std::uint8_t> data = read("lilita-one-regular.ttf");
    const auto heavy = std::make_shared<FontFamily>(FontFamily::Faces{.regular = std::make_shared<TrueTypeFont>(fixture.engine().getGraphics(), data)});
    const float weight = heavy->layout("o", {.size = 48.0F, .bold = true})->looks[0].weight;
    EXPECT_GT(weight, 0.0F);
    EXPECT_LT(weight, 48.0F * 0.03F);
    const auto light = std::make_shared<FontFamily>(FontFamily::Faces{.regular = fixture.engine().getDefaultFont()});
    EXPECT_FLOAT_EQ(light->layout("o", {.size = 48.0F, .bold = true})->looks[0].weight, 48.0F * 0.03F);

    // The fonts bake at 48 pixels, so a pixel of the field is a pixel of the text at that size.
    const FontFile file = open(data);
    const float scale = scaleFor(file, 48.0F);
    const std::optional<DistanceField> dotted = DistanceField::build(file, file.findGlyph('i'), scale, 8, 4096);
    const std::optional<DistanceField> round = DistanceField::build(file, file.findGlyph('o'), scale, 8, 4096);
    ASSERT_TRUE(dotted && round);
    EXPECT_EQ(countRegions(spread(*dotted, weight), dotted->width, dotted->height, true, false), 2);
    EXPECT_EQ(countRegions(spread(*round, weight), round->width, round->height, false, true), 1);
}

// A file that holds no font, or whose character map lies outside the file, is not a font.
TEST_F(FontFileTest, RejectsFilesWithoutAFont) {
    test::EngineFixture fixture;
    graphics::Device& device = fixture.engine().getGraphics();
    EXPECT_THROW(TrueTypeFont(device, {}), std::runtime_error);
    EXPECT_THROW(TrueTypeFont(device, {0, 1, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}), std::runtime_error);

    std::vector<std::uint8_t> data = read("default");
    test::DamagedFonts::moveTablePastTheEnd(data, "cmap");
    EXPECT_THROW(TrueTypeFont(device, data), std::runtime_error);
}

// A composite glyph made of itself would draw forever, so it draws nothing while the glyph keeps its advance.
TEST_F(FontFileTest, DrawsNothingForAGlyphMadeOfItself) {
    test::EngineFixture fixture;
    std::vector<std::uint8_t> data = read("crimson-text-regular.ttf");
    const std::uint32_t glyph = open(data).findGlyph('A');
    test::DamagedFonts::makeGlyphOfItself(data, glyph);
    TrueTypeFont font(fixture.engine().getGraphics(), data);
    EXPECT_FALSE(font.getGlyph(glyph).visible);
    EXPECT_GT(font.measure("A", {.size = 20.0F}).x, 0.0F);
}

// The location of an outline past the end of the table of outlines reads as a glyph without an outline, and a character map that counts more segments than it holds reads as a font without characters.
TEST_F(FontFileTest, ReadsOffsetsAndCountsPastTheirTablesAsMissing) {
    test::EngineFixture fixture;
    std::vector<std::uint8_t> data = read("crimson-text-regular.ttf");
    const std::uint32_t glyph = open(data).findGlyph('A');
    test::DamagedFonts::moveOutlinePastTheEnd(data, glyph);
    TrueTypeFont font(fixture.engine().getGraphics(), data);
    EXPECT_FALSE(font.getGlyph(glyph).visible);

    std::vector<std::uint8_t> counted = read("crimson-text-regular.ttf");
    test::DamagedFonts::overcountSegments(counted);
    TrueTypeFont unmapped(fixture.engine().getGraphics(), counted);
    EXPECT_FALSE(unmapped.hasGlyph(U'A'));
}

// A character map that names glyphs past the count of glyphs of the file maps no character, and text that the shaper turns into such glyphs lays out with the glyph of missing characters.
TEST_F(FontFileTest, LaysOutGlyphsPastTheCountOfTheFileAsMissing) {
    test::EngineFixture fixture;
    std::vector<std::uint8_t> data = read("crimson-text-regular.ttf");
    test::DamagedFonts::shrinkGlyphCount(data, 1);
    TrueTypeFont font(fixture.engine().getGraphics(), data);
    EXPECT_FALSE(font.hasGlyph(U'A'));
    const float width = font.measure("Zebra", {.size = 20.0F}).x;
    EXPECT_TRUE(std::isfinite(width));
    EXPECT_GE(width, 0.0F);
}

// A table of advances shorter than the count of advances it claims, and an em square of no units, still measure finite text.
TEST_F(FontFileTest, MeasuresFontsWithTruncatedAdvancesAndNoEmSquare) {
    test::EngineFixture fixture;
    std::vector<std::uint8_t> data = read("crimson-text-regular.ttf");
    test::DamagedFonts::overcountAdvances(data);
    test::DamagedFonts::removeEmSquare(data);
    TrueTypeFont font(fixture.engine().getGraphics(), data);
    EXPECT_TRUE(std::isfinite(font.getAscent(20.0F)));
    EXPECT_TRUE(std::isfinite(font.getLineHeight(20.0F)));
    const float width = font.measure("Zebra", {.size = 20.0F}).x;
    EXPECT_TRUE(std::isfinite(width));
    EXPECT_GT(width, 0.0F);
}

// The inside of a distance field covers the pixels the glyph covers, for the quadratic curves of TrueType outlines and the cubic curves of CFF outlines alike.
TEST_F(FontFileTest, BuildsDistanceFieldsFromWholeOutlines) {
    for (const std::string_view name : {"default", "fira-sans-regular.otf", "crimson-text-regular.ttf"}) {
        const FontFile file = open(read(name));
        const float scale = scaleFor(file, 64.0F);
        for (const char32_t character : std::u32string_view(U"Sphinx&@8Og")) {
            const std::uint32_t glyph = file.findGlyph(character);
            const std::optional<DistanceField> field = DistanceField::build(file, glyph, scale, 8, 4096);
            ASSERT_TRUE(field.has_value()) << name;

            // Pixels the edge crosses may go either way, so only clearly covered and clearly empty pixels count.
            const Coverage coverage = cover(file, glyph, scale);
            int agreeing = 0;
            int counted = 0;
            for (int row = 0; row < coverage.height; ++row) {
                for (int column = 0; column < coverage.width; ++column) {
                    const int covered = coverage.pixels[static_cast<std::size_t>(row * coverage.width + column)];
                    if (covered > 64 && covered < 192) {
                        continue;
                    }
                    const std::uint16_t distance = field->distances[static_cast<std::size_t>((row + coverage.y - field->offsetY) * field->width + column + coverage.x - field->offsetX)];
                    agreeing += (distance >= math::HalfFloat::fromFloat(0.5F)) == (covered >= 192) ? 1 : 0;
                    ++counted;
                }
            }
            EXPECT_GE(agreeing, counted * 995 / 1000) << name << " " << static_cast<int>(character);
        }
        EXPECT_FALSE(DistanceField::build(file, file.findGlyph(' '), scale, 8, 4096).has_value());
    }
}

// Every face sizes by its em square, so a fallback font draws at the size of the face around it, and the line metrics are those of the font at that size.
TEST_F(FontFileTest, SizesEveryFaceByItsEm) {
    test::EngineFixture fixture;
    for (const std::string_view name : {"default", "noto-sans-symbols-2-regular.ttf", "fira-sans-regular.otf"}) {
        const std::vector<std::uint8_t> data = read(name);
        const FontFile file = open(data);
        const FontFile::LineMetrics& lines = file.getLineMetrics();
        const bool symbols = name == "noto-sans-symbols-2-regular.ttf";
        const int advance = file.getAdvance(file.findGlyph(symbols ? U'☀' : U'M'));
        const float em = scaleFor(file, 100.0F);

        TrueTypeFont font(fixture.engine().getGraphics(), data);
        EXPECT_NEAR(font.getAscent(100.0F), static_cast<float>(lines.ascender) * em, 0.01F) << name;
        EXPECT_NEAR(font.getLineHeight(100.0F), static_cast<float>(lines.ascender - lines.descender + lines.lineGap) * em, 0.01F) << name;
        EXPECT_NEAR(font.measure(symbols ? "\xE2\x98\x80" : "M", {.size = 100.0F}).x, static_cast<float>(advance) * em, 0.01F) << name;
    }
}

} // namespace haylen::text
