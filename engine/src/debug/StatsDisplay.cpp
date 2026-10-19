#include "haylen/debug/StatsDisplay.hpp"

#include <lua.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <iterator>
#include <limits>
#include <utility>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/audio/Mixer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/core/TimerScheduler.hpp"
#include "haylen/core/TweenManager.hpp"
#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/Profiler.hpp"
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

std::optional<StatsDisplay::Item> StatsDisplay::itemFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kItemNames, name, &std::pair<std::string_view, Item>::first);
    return found != kItemNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view StatsDisplay::itemName(Item value) noexcept {
    return std::ranges::find(kItemNames, value, &std::pair<std::string_view, Item>::second)->first;
}

std::vector<StatsDisplay::Item> StatsDisplay::getDefaultItems() {
    return {Item::FrameRate, Item::DrawCalls, Item::Geometry};
}

void StatsDisplay::setShowing(Item item, bool value) noexcept {
    itemsChanged = itemsChanged || showing[static_cast<std::size_t>(item)] != value;
    showing[static_cast<std::size_t>(item)] = value;
}

void StatsDisplay::setItems(const std::vector<Item>& items) noexcept {
    for (const auto& [name, item] : kItemNames) {
        setShowing(item, std::ranges::find(items, item) != items.end());
    }
}

std::vector<StatsDisplay::Item> StatsDisplay::getItems() const {
    std::vector<Item> items;
    for (const auto& [name, item] : kItemNames) {
        if (isShowing(item)) {
            items.push_back(item);
        }
    }
    return items;
}

std::uint64_t StatsDisplay::countAlive(std::string_view counter) {
    const std::optional<ObjectCounter::Snapshot> found = ObjectCounter::find(counter);
    return found ? found->alive : 0;
}

// The strings keep their capacity, so the numbers format in place.
void StatsDisplay::format(core::Engine& engine, const graphics2d::Renderer::Stats& rendering) {
    lineCount = 0;
    for (const auto& [name, item] : kItemNames) {
        if (!isShowing(item)) {
            continue;
        }
        std::string& line = lines[lineCount++];
        line.clear();
        const auto out = std::back_inserter(line);
        switch (item) {
        case Item::FrameRate:
            std::format_to(out, "FPS {:.1f}, frame {:.2f} ms", engine.getProfiler().getFramesPerSecond(), engine.getProfiler().getLastFrameMilliseconds());
            break;
        case Item::DrawCalls:
            std::format_to(out, "Draw calls {}, passes {}", rendering.drawCalls, rendering.passes);
            break;
        case Item::Geometry:
            std::format_to(out, "Vertices {}, instances {}", rendering.vertices, rendering.instances);
            break;
        case Item::Memory: {
            const std::optional<ObjectCounter::Snapshot> textures = ObjectCounter::find("Texture");
            const double lua = static_cast<double>(lua_gc(engine.getLuaState(), LUA_GCCOUNT, 0)) / 1024.0;
            std::format_to(out, "Lua {:.1f} MB, textures {:.1f} MB", lua, textures ? static_cast<double>(textures->bytes) / (1024.0 * 1024.0) : 0.0);
            break;
        }
        case Item::Counters:
            std::format_to(out, "Scenes {}, tweens {}, timers {}", engine.getScenes().size(), engine.getTweens().size(), engine.getTimers().size());
            break;
        case Item::Physics:
            std::format_to(out, "Bodies {}, contacts {}", countAlive("PhysicsBody"), countAlive("PhysicsContact"));
            break;
        case Item::Particles:
            std::format_to(out, "Particles {}", countAlive("Particle"));
            break;
        case Item::Audio:
            std::format_to(out, "Voices {}", engine.getAudio().getVoiceCount());
            break;
        }
    }
}

void StatsDisplay::drawCompact(core::Engine& engine, const graphics2d::Renderer::Stats& rendering) {
    const core::FrameClock& clock = engine.getClock();
    const bool drewLastFrame = clock.getFrameIndex() == drawnFrame + 1 && !std::exchange(itemsChanged, false);
    sinceRefresh = drewLastFrame ? sinceRefresh + clock.getUnscaledDelta() : kRefreshSeconds;
    drawnFrame = clock.getFrameIndex();
    if (sinceRefresh >= kRefreshSeconds) {
        sinceRefresh = 0.0;
        format(engine, rendering);
    }
    if (lineCount == 0) {
        return;
    }

    graphics2d::Renderer& renderer = engine.getRenderer2D();
    text::Font& font = *engine.getDefaultFont();
    const float unit = engine.getWindow().getDpiScale() / engine.getViewport().getPixelsPerUnit().y;
    const text::Style style{.size = kTextSize * unit, .color = math::Color::fromHex(0xE8F0FFFFU)};
    float width = 0.0F;
    for (std::size_t index = 0; index < lineCount; ++index) {
        width = std::max(width, font.measure(lines[index], style).x);
    }
    const float padding = kPadding * unit;
    const float lineHeight = font.getLineHeight(style.size);
    const math::Rect safe = engine.getViewport().getSafeRect();
    const math::Vec2 size{width + padding * 2.0F, lineHeight * static_cast<float>(lineCount) + padding * 2.0F};
    const math::Rect panel{safe.x + kMargin * unit, safe.getBottom() - kMargin * unit - size.y, size.x, size.y};

    // The highest canvas order keeps the display above every canvas of the app.
    renderer.beginScreen({.order = std::numeric_limits<int>::max()});
    renderer.drawRect(panel, math::Color::fromHex(0x0B0F18C0U));
    for (std::size_t index = 0; index < lineCount; ++index) {
        renderer.drawText(font, lines[index], {panel.x + padding, panel.y + padding + lineHeight * static_cast<float>(index)}, style);
    }
}

} // namespace haylen::debug
