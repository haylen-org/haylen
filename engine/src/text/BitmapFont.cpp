#include "haylen/text/BitmapFont.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

#include "haylen/core/Utf8.hpp"
#include "text/Segmenter.hpp"

namespace haylen::text {

Font::Metrics BitmapFont::readMetrics(const Description& description) {
    if (!(description.size > 0.0F) || !(description.lineHeight > 0.0F)) {
        throw std::invalid_argument("A bitmap font needs a positive size and line height.");
    }
    return {.nativeSize = description.size, .ascent = description.base, .lineHeight = description.lineHeight};
}

BitmapFont::BitmapFont(const Description& description, std::vector<graphics::Texture> pageTextures) : Font(readMetrics(description)), pages(std::move(pageTextures)) {
    if (pages.size() != description.pages.size()) {
        throw std::invalid_argument("A bitmap font needs one texture for each of its " + std::to_string(description.pages.size()) + " pages.");
    }
    for (const graphics::Texture& page : pages) {
        if (!page.isValid()) {
            throw std::invalid_argument("A bitmap font page needs a texture.");
        }
    }
    for (const Character& character : description.characters) {
        if (character.page >= pages.size()) {
            throw std::invalid_argument("A bitmap font character lies on page " + std::to_string(character.page) + ", which the font does not have.");
        }
        glyphs[static_cast<std::uint32_t>(character.codePoint)] = {.source = character.source, .offset = character.offset, .advance = character.advance, .page = character.page, .visible = !character.source.isEmpty()};
    }
    for (const Kerning& kerning : description.kernings) {
        kernings[pairKey(kerning.left, kerning.right)] = kerning.amount;
    }
}

std::uint64_t BitmapFont::pairKey(char32_t left, char32_t right) noexcept {
    return (static_cast<std::uint64_t>(left) << 32U) | static_cast<std::uint64_t>(right);
}

bool BitmapFont::hasGlyph(char32_t codePoint) {
    return glyphs.contains(static_cast<std::uint32_t>(codePoint));
}

// A right-to-left run draws mirrored brackets and reads backwards, and the kerning pairs apply to glyphs in the order they stand on screen.
void BitmapFont::shape(const Run& run, std::vector<ShapedGlyph>& shaped) {
    const std::size_t first = shaped.size();
    for (std::size_t index = run.begin; index < run.end; ++index) {
        const char32_t codePoint = run.rightToLeft ? Segmenter::getMirror(run.text[index]) : run.text[index];
        const auto glyph = static_cast<std::uint32_t>(codePoint);
        shaped.push_back({.index = glyph, .cluster = index, .advance = getGlyph(glyph).advance});
    }
    if (run.rightToLeft) {
        std::reverse(shaped.begin() + static_cast<std::ptrdiff_t>(first), shaped.end());
    }
    for (std::size_t index = first; index + 1 < shaped.size(); ++index) {
        shaped[index].advance += getKerning(static_cast<char32_t>(shaped[index].index), static_cast<char32_t>(shaped[index + 1].index));
    }
}

const Font::Glyph& BitmapFont::getGlyph(std::uint32_t index) {
    const auto found = glyphs.find(index);
    return found != glyphs.end() ? found->second : missing;
}

float BitmapFont::getKerning(char32_t left, char32_t right) const {
    const auto found = kernings.find(pairKey(left, right));
    return found != kernings.end() ? found->second : 0.0F;
}

const graphics::Texture& BitmapFont::getPage(std::size_t index) const {
    if (index >= pages.size()) {
        throw std::out_of_range("The bitmap font has no page " + std::to_string(index) + ".");
    }
    return pages[index];
}

BitmapFont::Description BitmapFont::parse(std::span<const std::uint8_t> bytes) {
    const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return text.starts_with(kBinaryMagic) ? parseBinary(bytes) : parseText(text);
}

// The text format has one tag per line followed by key=value pairs, where values in quotes may hold spaces.
BitmapFont::Description BitmapFont::parseText(std::string_view text) {
    Description description;
    bool hasInfo = false;
    bool hasCommon = false;
    std::size_t lineNumber = 0;
    while (!text.empty()) {
        ++lineNumber;
        const std::size_t end = std::min(text.find('\n'), text.size());
        std::string_view line = text.substr(0, end);
        text.remove_prefix(std::min(end + 1, text.size()));
        if (line.ends_with('\r')) {
            line.remove_suffix(1);
        }

        const std::size_t tagEnd = std::min(line.find(' '), line.size());
        const std::string_view tag = line.substr(0, tagEnd);
        line.remove_prefix(tagEnd);
        std::map<std::string_view, std::string_view, std::less<>> values;
        while (!line.empty()) {
            if (line.front() == ' ' || line.front() == '\t') {
                line.remove_prefix(1);
                continue;
            }
            const std::size_t equals = line.find('=');
            if (equals == std::string_view::npos) {
                break;
            }
            const std::string_view key = line.substr(0, equals);
            line.remove_prefix(equals + 1);
            std::size_t valueEnd = std::min(line.find(' '), line.size());
            std::string_view value = line.substr(0, valueEnd);
            if (line.starts_with('"')) {
                valueEnd = line.find('"', 1);
                if (valueEnd == std::string_view::npos) {
                    throw std::invalid_argument("BMFont line " + std::to_string(lineNumber) + " has a quote that never ends.");
                }
                value = line.substr(1, valueEnd - 1);
                ++valueEnd;
            }
            values.insert_or_assign(key, value);
            line.remove_prefix(std::min(valueEnd, line.size()));
        }

        // clang-format off
        const auto number = [&](std::string_view key) {
            const auto found = values.find(key);
            int value = 0;
            if (found == values.end() || std::from_chars(found->second.data(), found->second.data() + found->second.size(), value).ec != std::errc{}) {
                throw std::invalid_argument("BMFont line " + std::to_string(lineNumber) + " needs a whole number for " + std::string(key) + ".");
            }
            return value;
        };
        // clang-format on

        if (tag == "info") {
            description.size = static_cast<float>(std::abs(number("size")));
            hasInfo = true;
        } else if (tag == "common") {
            description.lineHeight = static_cast<float>(number("lineHeight"));
            description.base = static_cast<float>(number("base"));
            description.pages.resize(static_cast<std::size_t>(std::max(number("pages"), 0)));
            hasCommon = true;
        } else if (tag == "page") {
            const int id = number("id");
            if (id < 0 || static_cast<std::size_t>(id) >= description.pages.size() || !values.contains("file")) {
                throw std::invalid_argument("BMFont line " + std::to_string(lineNumber) + " names a page the common line does not count, or no file.");
            }
            description.pages[static_cast<std::size_t>(id)] = std::string(values.at("file"));
        } else if (tag == "char") {
            description.characters.push_back({
                .codePoint = static_cast<char32_t>(number("id")),
                .source = {static_cast<float>(number("x")), static_cast<float>(number("y")), static_cast<float>(number("width")), static_cast<float>(number("height"))},
                .offset = {static_cast<float>(number("xoffset")), static_cast<float>(number("yoffset")) - description.base},
                .advance = static_cast<float>(number("xadvance")),
                .page = static_cast<std::uint16_t>(number("page")),
            });
        } else if (tag == "kerning") {
            description.kernings.push_back({.left = static_cast<char32_t>(number("first")), .right = static_cast<char32_t>(number("second")), .amount = static_cast<float>(number("amount"))});
        }
    }

    if (!hasInfo || !hasCommon) {
        throw std::invalid_argument("A BMFont file needs its info and common lines.");
    }
    for (const std::string& page : description.pages) {
        if (page.empty()) {
            throw std::invalid_argument("A BMFont file must name the image of every page.");
        }
    }
    return description;
}

// The binary format is the magic BMF, version 3 and a list of blocks, each a type byte and a 32-bit size, with little-endian numbers.
BitmapFont::Description BitmapFont::parseBinary(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 4 || bytes[3] != 3) {
        throw std::invalid_argument("Only version 3 of the binary BMFont format is supported.");
    }

    // clang-format off
    const auto read = [&](std::size_t offset, std::size_t size) {
        if (offset + size > bytes.size()) {
            throw std::invalid_argument("The binary BMFont file ends in the middle of a block.");
        }
        std::uint32_t value = 0;
        for (std::size_t index = 0; index < size; ++index) {
            value |= static_cast<std::uint32_t>(bytes[offset + index]) << (index * 8U);
        }
        return value;
    };
    const auto readSigned = [&](std::size_t offset) {
        return static_cast<float>(static_cast<std::int16_t>(read(offset, 2)));
    };
    // clang-format on

    Description description;
    bool hasInfo = false;
    bool hasCommon = false;
    std::size_t offset = 4;
    while (offset < bytes.size()) {
        const std::uint32_t type = read(offset, 1);
        const std::size_t size = read(offset + 1, 4);
        const std::size_t start = offset + 5;
        if (start + size > bytes.size()) {
            throw std::invalid_argument("The binary BMFont file ends in the middle of a block.");
        }
        offset = start + size;

        switch (type) {
        case 1:
            description.size = std::fabs(readSigned(start));
            hasInfo = true;
            break;
        case 2:
            description.lineHeight = static_cast<float>(read(start, 2));
            description.base = static_cast<float>(read(start + 2, 2));
            description.pages.resize(read(start + 8, 2));
            hasCommon = true;
            break;
        case 3: {
            std::size_t cursor = start;
            for (std::string& page : description.pages) {
                while (cursor < offset && bytes[cursor] != 0) {
                    page += static_cast<char>(bytes[cursor++]);
                }
                ++cursor;
            }
            break;
        }
        case 4:
            for (std::size_t entry = start; entry + 20 <= offset; entry += 20) {
                description.characters.push_back({
                    .codePoint = static_cast<char32_t>(read(entry, 4)),
                    .source = {static_cast<float>(read(entry + 4, 2)), static_cast<float>(read(entry + 6, 2)), static_cast<float>(read(entry + 8, 2)), static_cast<float>(read(entry + 10, 2))},
                    .offset = {readSigned(entry + 12), readSigned(entry + 14) - description.base},
                    .advance = readSigned(entry + 16),
                    .page = static_cast<std::uint16_t>(read(entry + 18, 1)),
                });
            }
            break;
        case 5:
            for (std::size_t entry = start; entry + 10 <= offset; entry += 10) {
                description.kernings.push_back({.left = static_cast<char32_t>(read(entry, 4)), .right = static_cast<char32_t>(read(entry + 4, 4)), .amount = readSigned(entry + 8)});
            }
            break;
        default:
            throw std::invalid_argument("The binary BMFont file has a block of unknown type " + std::to_string(type) + ".");
        }
    }

    if (!hasInfo || !hasCommon) {
        throw std::invalid_argument("A BMFont file needs its info and common blocks.");
    }
    for (const std::string& page : description.pages) {
        if (page.empty()) {
            throw std::invalid_argument("A BMFont file must name the image of every page.");
        }
    }
    return description;
}

// Cells run left to right and top to bottom from the margin, as many per row as the image holds, which is none when the margins leave no room.
BitmapFont::Description BitmapFont::describeGrid(const Grid& grid, math::Vec2 imageSize) {
    if (!(grid.cellWidth >= 1.0F) || !(grid.cellHeight >= 1.0F)) {
        throw std::invalid_argument("A grid font needs cells at least one pixel wide and tall.");
    }
    if (!(grid.spacing.x >= 0.0F) || !(grid.spacing.y >= 0.0F) || !(grid.margin.x >= 0.0F) || !(grid.margin.y >= 0.0F)) {
        throw std::invalid_argument("A grid font cannot have a negative spacing or margin.");
    }
    const std::u32string characters = core::Utf8::decode(grid.characters);
    if (characters.empty()) {
        throw std::invalid_argument("A grid font needs the characters of its cells.");
    }
    const auto columns = static_cast<std::size_t>(std::max(0.0F, std::floor((imageSize.x - grid.margin.x * 2.0F + grid.spacing.x) / (grid.cellWidth + grid.spacing.x))));
    const auto rows = static_cast<std::size_t>(std::max(0.0F, std::floor((imageSize.y - grid.margin.y * 2.0F + grid.spacing.y) / (grid.cellHeight + grid.spacing.y))));
    if (columns == 0 || characters.size() > columns * rows) {
        throw std::invalid_argument("The grid font image holds " + std::to_string(columns * rows) + " cells, fewer than its " + std::to_string(characters.size()) + " characters.");
    }

    const float baseline = grid.baseline > 0.0F ? grid.baseline : grid.cellHeight;
    Description description{.size = grid.cellHeight, .lineHeight = grid.lineHeight > 0.0F ? grid.lineHeight : grid.cellHeight, .base = baseline, .pages = {"grid"}};
    for (std::size_t index = 0; index < characters.size(); ++index) {
        const float x = grid.margin.x + static_cast<float>(index % columns) * (grid.cellWidth + grid.spacing.x);
        const float y = grid.margin.y + static_cast<float>(index / columns) * (grid.cellHeight + grid.spacing.y);
        description.characters.push_back({.codePoint = characters[index], .source = {x, y, grid.cellWidth, grid.cellHeight}, .offset = {0.0F, -baseline}, .advance = grid.advance > 0.0F ? grid.advance : grid.cellWidth});
    }
    return description;
}

} // namespace haylen::text
