#include "haylen/debug/StatsDisplay.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <limits>
#include <string>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/platform/Window.hpp"
#include "haylen/text/Font.hpp"

namespace haylen::debug {

const std::array<std::pair<std::string_view, StatsDisplay::Mode>, 3> StatsDisplay::kModeNames{{{"off", Mode::Off}, {"compact", Mode::Compact}, {"full", Mode::Full}}};

std::optional<StatsDisplay::Mode> StatsDisplay::modeFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kModeNames, name, &std::pair<std::string_view, Mode>::first);
    return found != kModeNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view StatsDisplay::modeName(Mode value) noexcept {
    return std::ranges::find(kModeNames, value, &std::pair<std::string_view, Mode>::second)->first;
}

void StatsDisplay::drawCompact(core::Engine& engine, const Numbers& numbers) {
    graphics2d::Renderer& renderer = engine.getRenderer2D();
    text::Font& font = *engine.getDefaultFont();
    const float unit = engine.getWindow().getDpiScale() / engine.getViewport().getPixelsPerUnit().y;
    const text::Style style{.size = kTextSize * unit, .color = math::Color::fromHex(0xE8F0FFFFU)};
    const std::array<std::string, 3> lines{
        std::format("{:.1f} FPS  {:.2f} ms", numbers.fps, numbers.milliseconds),
        std::format("{} draws  {} vertices", numbers.drawCalls, numbers.vertices),
        std::format("{} instances", numbers.instances),
    };

    float width = 0.0F;
    for (const std::string& line : lines) {
        width = std::max(width, font.measure(line, style).x);
    }
    const float padding = kPadding * unit;
    const float lineHeight = font.getLineHeight(style.size);
    const math::Rect safe = engine.getViewport().getSafeRect();
    const math::Vec2 size{width + padding * 2.0F, lineHeight * static_cast<float>(lines.size()) + padding * 2.0F};
    const math::Rect panel{safe.x + kMargin * unit, safe.getBottom() - kMargin * unit - size.y, size.x, size.y};

    // The highest canvas order keeps the display above every canvas of the app.
    renderer.beginScreen({.order = std::numeric_limits<int>::max()});
    renderer.drawRect(panel, math::Color::fromHex(0x0B0F18C0U));
    for (std::size_t index = 0; index < lines.size(); ++index) {
        renderer.drawText(font, lines[index], {panel.x + padding, panel.y + padding + lineHeight * static_cast<float>(index)}, style);
    }
}

} // namespace haylen::debug
