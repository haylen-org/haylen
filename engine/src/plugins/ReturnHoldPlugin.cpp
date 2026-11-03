#include "plugins/ReturnHoldPlugin.hpp"

#include <algorithm>
#include <cstddef>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/Shape.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/platform/Window.hpp"
#include "haylen/text/Font.hpp"
#include "haylen/text/Style.hpp"
#include "platform/Host.hpp"

namespace haylen::plugins {

void ReturnHoldPlugin::event(core::Engine& engine, const platform::Event& event) {
    if (engine.isLaunched()) {
        hold.handleEvent(event, engine.getWindow().getDpiScale());
    }
}

// The back button of a gamepad counts too, such as the one of a controller paired with a TV.
void ReturnHoldPlugin::beginFrame(core::Engine& engine, float deltaSeconds) {
    if (!engine.isLaunched()) {
        return;
    }
    const input::Input& input = engine.getInput();
    bool backHeld = host.isBackHeld();
    for (std::size_t index = 0; index < input::Input::kMaxGamepads && !backHeld; ++index) {
        const input::GamepadState& gamepad = input.getGamepad(index);
        backHeld = gamepad.connected && gamepad.buttons[static_cast<std::size_t>(input::GamepadButton::Back)];
    }
    if (hold.update(deltaSeconds, backHeld)) {
        engine.quit();
    }
}

// A panel at the top of the safe area fills its bar as the hold goes on, in the points of the screen at any design size.
void ReturnHoldPlugin::renderOverlay(core::Engine& engine) {
    const float progress = hold.getProgress();
    if (progress <= 0.0F) {
        return;
    }
    graphics2d::Renderer& renderer = engine.getRenderer2D();
    const graphics::Viewport& viewport = engine.getViewport();
    const float unit = engine.getWindow().getDpiScale() / viewport.getPixelsPerUnit().y;
    const math::Rect safe = viewport.getSafeRect();
    const float width = std::min(kWidth * unit, safe.width - 2.0F * kMargin * unit);
    const math::Rect panel{safe.x + (safe.width - width) * 0.5F, safe.y + kMargin * unit, width, kHeight * unit};
    const float radius = 12.0F * unit;

    renderer.beginScreen();
    renderer.drawShape({.bounds = panel, .radii = {radius, radius, radius, radius}, .color = math::Color::fromHex(0x161925E6U)});
    renderer.drawText(*engine.getDefaultFont(), "Keep holding to leave the app", {panel.x + panel.width * 0.5F, panel.y + 12.0F * unit}, {.size = kTextSize * unit, .color = math::Color::fromHex(0xECEEF4FFU), .align = text::Alignment::Center, .anchor = {0.5F, 0.0F}});
    const math::Rect track{panel.x + 16.0F * unit, panel.getBottom() - 18.0F * unit, panel.width - 32.0F * unit, 6.0F * unit};
    const float round = track.height * 0.5F;
    renderer.drawShape({.bounds = track, .radii = {round, round, round, round}, .color = math::Color::fromHex(0xFFFFFF33U)});
    renderer.drawShape({.bounds = {track.x, track.y, std::max(track.height, track.width * progress), track.height}, .radii = {round, round, round, round}, .color = math::Color::fromHex(0xF5C26BFFU)});
}

} // namespace haylen::plugins
