#include "haylen/math/MarchingSquares.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

namespace haylen::math {

const MarchingSquares::Options MarchingSquares::kDefaultOptions{};

// Walks the cells of the field framed by one ring of empty padding samples, links the oriented segments of every cell by their shared edges and follows the links into outlines.
class MarchingSquares::Tracer final {
  public:
    Tracer(std::span<const float> field, int columns, int rows, const Options& settings) : values(field), width(columns), height(rows), stride(columns + 2), options(settings), next(edgeCount(columns, rows), -1), positions(next.size()) {}

    [[nodiscard]] std::vector<std::vector<Vec2>> run() {
        for (int row = 0; row <= height; ++row) {
            for (int column = 0; column <= width; ++column) {
                march(column, row);
            }
        }
        return link();
    }

  private:
    enum Edge : std::uint8_t {
        Top,
        Right,
        Bottom,
        Left,
    };

    struct Segment {
        Edge from;
        Edge to;
    };

    // The segments of each corner case, with the top left corner as bit 8, top right 4, bottom right 2 and bottom left 1, oriented so the inside lies on their left. Saddle cases 5 and 10 list the variant whose center is outside.
    static constexpr std::array<std::array<Segment, 2>, 16> kSegments{{
        {},
        {{{Left, Bottom}}},
        {{{Bottom, Right}}},
        {{{Left, Right}}},
        {{{Right, Top}}},
        {{{Left, Bottom}, {Right, Top}}},
        {{{Bottom, Top}}},
        {{{Left, Top}}},
        {{{Top, Left}}},
        {{{Top, Bottom}}},
        {{{Top, Left}, {Bottom, Right}}},
        {{{Top, Right}}},
        {{{Right, Left}}},
        {{{Right, Bottom}}},
        {{{Bottom, Left}}},
        {},
    }};
    static constexpr std::array<std::uint8_t, 16> kSegmentCounts{0, 1, 1, 1, 1, 2, 1, 1, 1, 1, 2, 1, 1, 1, 1, 0};
    static constexpr std::array<Segment, 2> kConnectedFive{{{Left, Top}, {Right, Bottom}}};
    static constexpr std::array<Segment, 2> kConnectedTen{{{Top, Right}, {Bottom, Left}}};

    [[nodiscard]] static std::size_t edgeCount(int columns, int rows) {
        const auto count = static_cast<std::size_t>(columns + 2) * static_cast<std::size_t>(rows + 2) * 2;
        if (count > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
            throw std::invalid_argument("The field is too large to trace.");
        }
        return count;
    }

    // Padded coordinates start at the padding column and row, so sample (x, y) of the field sits at (x + 1, y + 1).
    [[nodiscard]] bool isPadding(int column, int row) const noexcept {
        return column == 0 || row == 0 || column > width || row > height;
    }

    [[nodiscard]] float valueAt(int column, int row) const noexcept {
        return values[static_cast<std::size_t>(row - 1) * static_cast<std::size_t>(width) + static_cast<std::size_t>(column - 1)];
    }

    [[nodiscard]] bool isInside(int column, int row) const noexcept {
        return !isPadding(column, row) && valueAt(column, row) >= options.threshold;
    }

    [[nodiscard]] std::int32_t edgeId(int column, int row, Edge edge) const noexcept {
        const int right = edge == Right ? 1 : 0;
        const int below = edge == Bottom ? 1 : 0;
        const int vertical = edge == Left || edge == Right ? 1 : 0;
        return ((row + below) * stride + column + right) * 2 + vertical;
    }

    // A crossing toward the padding sits on the real sample, so areas that reach the edge close along it.
    [[nodiscard]] Vec2 crossing(int fromColumn, int fromRow, int toColumn, int toRow) const noexcept {
        float t = 0.0F;
        if (isPadding(fromColumn, fromRow)) {
            t = 1.0F;
        } else if (!isPadding(toColumn, toRow)) {
            const float from = valueAt(fromColumn, fromRow);
            const float to = valueAt(toColumn, toRow);
            t = std::clamp((options.threshold - from) / (to - from), 0.0F, 1.0F);
        }
        const Vec2 local{static_cast<float>(fromColumn - 1) + static_cast<float>(toColumn - fromColumn) * t, static_cast<float>(fromRow - 1) + static_cast<float>(toRow - fromRow) * t};
        return options.origin + local * options.spacing;
    }

    [[nodiscard]] Vec2 crossingOf(int column, int row, Edge edge) const noexcept {
        switch (edge) {
        case Top:
            return crossing(column, row, column + 1, row);
        case Right:
            return crossing(column + 1, row, column + 1, row + 1);
        case Bottom:
            return crossing(column, row + 1, column + 1, row + 1);
        case Left:
            break;
        }
        return crossing(column, row, column, row + 1);
    }

    // Saddle cells only occur between four real samples, because padding samples are never inside.
    [[nodiscard]] bool isCenterInside(int column, int row) const noexcept {
        const float sum = valueAt(column, row) + valueAt(column + 1, row) + valueAt(column + 1, row + 1) + valueAt(column, row + 1);
        return sum * 0.25F >= options.threshold;
    }

    void march(int column, int row) {
        const int corners = (isInside(column, row) ? 8 : 0) | (isInside(column + 1, row) ? 4 : 0) | (isInside(column + 1, row + 1) ? 2 : 0) | (isInside(column, row + 1) ? 1 : 0);
        const auto index = static_cast<std::size_t>(corners);
        std::array<Segment, 2> segments = kSegments[index];
        if ((corners == 5 || corners == 10) && isCenterInside(column, row)) {
            segments = corners == 5 ? kConnectedFive : kConnectedTen;
        }

        for (std::size_t segment = 0; segment < kSegmentCounts[index]; ++segment) {
            const std::int32_t from = edgeId(column, row, segments[segment].from);
            const std::int32_t to = edgeId(column, row, segments[segment].to);
            positions[static_cast<std::size_t>(from)] = crossingOf(column, row, segments[segment].from);
            positions[static_cast<std::size_t>(to)] = crossingOf(column, row, segments[segment].to);
            next[static_cast<std::size_t>(from)] = to;
        }
    }

    [[nodiscard]] std::vector<std::vector<Vec2>> link() {
        std::vector<std::vector<Vec2>> outlines;
        for (std::size_t start = 0; start < next.size(); ++start) {
            if (next[start] < 0) {
                continue;
            }

            std::vector<Vec2> outline;
            auto current = static_cast<std::int32_t>(start);
            while (next[static_cast<std::size_t>(current)] >= 0) {
                const Vec2 point = positions[static_cast<std::size_t>(current)];
                if (outline.empty() || outline.back() != point) {
                    outline.push_back(point);
                }
                current = std::exchange(next[static_cast<std::size_t>(current)], -1);
            }

            if (outline.size() > 1 && outline.back() == outline.front()) {
                outline.pop_back();
            }
            if (outline.size() >= 3) {
                outlines.push_back(std::move(outline));
            }
        }
        return outlines;
    }

    std::span<const float> values;
    int width;
    int height;
    int stride;
    const Options& options;
    std::vector<std::int32_t> next;
    std::vector<Vec2> positions;
};

std::vector<std::vector<Vec2>> MarchingSquares::trace(std::span<const float> values, int width, int height, const Options& options) {
    if (width < 0 || height < 0 || values.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        throw std::invalid_argument("The field needs width times height values.");
    }
    return Tracer(values, width, height, options).run();
}

std::vector<std::vector<Vec2>> MarchingSquares::traceBitmap(std::span<const std::uint8_t> pixels, int width, int height, float spacing, Vec2 origin) {
    if (width < 0 || height < 0 || pixels.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        throw std::invalid_argument("The bitmap needs width times height pixels.");
    }

    // A frame of empty samples around the pixel centers puts the outline on the outer edge of the border pixels.
    const int columns = width + 2;
    const int rows = height + 2;
    std::vector<float> field(static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows), 0.0F);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t pixel = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
            field[static_cast<std::size_t>(y + 1) * static_cast<std::size_t>(columns) + static_cast<std::size_t>(x + 1)] = pixels[pixel] != 0 ? 1.0F : 0.0F;
        }
    }
    const Options options{.threshold = 0.5F, .spacing = spacing, .origin = origin - Vec2{spacing, spacing} * 0.5F};
    return Tracer(field, columns, rows, options).run();
}

} // namespace haylen::math
