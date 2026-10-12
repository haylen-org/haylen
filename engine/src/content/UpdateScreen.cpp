#include "content/UpdateScreen.hpp"

#include <algorithm>
#include <array>
#include <format>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/content/ContentManager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/Window.hpp"
#include "haylen/text/Font.hpp"

namespace haylen::content {

UpdateScreen::UpdateScreen(core::Engine& owner) : engine(owner), content(owner.getContent()) {
    // The logo shows when it is on the device already, since the content it comes from may be what the screen waits for.
    const std::string& logoPath = engine.getConfig().splash.logo;
    if (!logoPath.empty() && content.isLocal(logoPath)) {
        try {
            logo = engine.getAssets().texture(logoPath);
        } catch (const std::exception&) {}
    }
}

bool UpdateScreen::canQuit() const {
    const std::string_view platform = engine.getPlatformName();
    return platform != "ios" && platform != "tvos";
}

bool UpdateScreen::hasFailed() const {
    return content.getStatus() == ContentManager::Status::Failed;
}

float UpdateScreen::getUnit() const {
    return engine.getWindow().getDpiScale() / engine.getViewport().getPixelsPerUnit().y;
}

math::Color UpdateScreen::getTextColor() const {
    const math::Color background = engine.getConfig().splash.background;
    const float luminance = 0.299F * background.r + 0.587F * background.g + 0.114F * background.b;
    return luminance > 0.6F ? math::Color::fromHex(0x1B1F2AFFU) : math::Color::fromHex(0xF2F4F8FFU);
}

std::string UpdateScreen::formatBytes(std::uint64_t bytes) {
    constexpr std::array<std::string_view, 4> kUnits = {"KB", "MB", "GB", "TB"};
    if (bytes < 1000) {
        return std::format("{} bytes", bytes);
    }
    auto value = static_cast<double>(bytes) / 1000.0;
    std::size_t unit = 0;
    while (value >= 1000.0 && unit + 1 < kUnits.size()) {
        value /= 1000.0;
        ++unit;
    }
    return std::format("{:.1f} {}", value, kUnits[unit]);
}

std::string UpdateScreen::getStatus() const {
    if (hasFailed()) {
        const std::optional<Error>& error = content.getError();
        if (!error) {
            return "The content could not be downloaded.";
        }
        switch (error->getCode()) {
        case Error::Code::NetworkOffline:
        case Error::Code::NetworkTimeout:
        case Error::Code::UpdateInterrupted:
            return "The device is offline or the server cannot be reached. Connect to the internet and try again.";
        case Error::Code::InsufficientStorage:
        case Error::Code::DiskFull:
            return std::format("The device needs {} of free room for the content. Free some room and try again.", formatBytes(error->getDetails().requiredBytes.value_or(0)));
        default:
            return error->what();
        }
    }
    const ContentManager::Progress& progress = content.getProgress();
    if (progress.totalBytes == 0) {
        return "Checking the content on the device.";
    }
    return std::format("Downloading the content of the app, {} of {}.", formatBytes(progress.receivedBytes), formatBytes(progress.totalBytes));
}

void UpdateScreen::render() {
    graphics2d::Renderer& renderer = engine.getRenderer2D();
    text::Font& font = *engine.getDefaultFont();
    const float unit = getUnit();
    const math::Rect safe = engine.getViewport().getSafeRect();
    const math::Color textColor = getTextColor();
    const math::Vec2 center = safe.getCenter();

    renderer.beginScreen();
    renderer.drawRect(renderer.getCanvasBounds(), engine.getConfig().splash.background);

    float y = center.y - (kLogoSize * 0.5F + kGap * 2.0F) * unit;
    if (logo) {
        const float size = kLogoSize * unit;
        const math::Vec2 logoSize = logo->getSize();
        const float scale = size / std::max(logoSize.x, logoSize.y);
        renderer.draw({.texture = *logo, .position = {center.x, y - size * 0.5F}, .size = logoSize * scale});
        y += kGap * unit;
    }

    const text::Style title{.size = kTitleSize * unit, .color = textColor, .align = text::Alignment::Center};
    renderer.drawText(font, engine.getConfig().name, {center.x, y}, title);
    y += font.getLineHeight(title.size) + kGap * unit;

    const float barWidth = std::min(kBarWidth * unit, safe.width - kMargin * 2.0F * unit);
    const math::Rect bar{center.x - barWidth * 0.5F, y, barWidth, kBarHeight * unit};
    const ContentManager::Progress& progress = content.getProgress();
    const float fraction = progress.totalBytes == 0 ? 0.0F : static_cast<float>(progress.receivedBytes) / static_cast<float>(progress.totalBytes);
    if (!hasFailed()) {
        renderer.drawRect(bar, kTrackColor);
        renderer.drawRect({bar.x, bar.y, bar.width * std::clamp(fraction, 0.0F, 1.0F), bar.height}, kBarColor);
    }
    y = bar.getBottom() + kGap * unit;

    const text::Style status{.size = kBodySize * unit, .color = hasFailed() ? kErrorColor : textColor, .align = text::Alignment::Center, .maxWidth = safe.width - kMargin * 2.0F * unit};
    renderer.drawText(font, getStatus(), {center.x, y}, status);
    y += font.measure(getStatus(), status).y + kGap * 1.5F * unit;

    retryButton = {};
    quitButton = {};
    if (!hasFailed()) {
        return;
    }
    const float half = canQuit() ? (kGap * 0.5F + 80.0F) * unit : 0.0F;
    retryButton = drawButton(renderer, font, {center.x - half, y}, "Try again", Action::Retry, unit);
    if (canQuit()) {
        quitButton = drawButton(renderer, font, {center.x + half, y}, "Quit", Action::Quit, unit);
    }
}

math::Rect UpdateScreen::drawButton(graphics2d::Renderer& renderer, text::Font& font, math::Vec2 center, std::string_view label, Action action, float unit) const {
    const text::Style style{.size = kBodySize * unit, .color = math::Color::fromHex(0xF2F4F8FFU)};
    const math::Vec2 size = font.measure(label, style);
    const float width = std::max(size.x + kGap * 2.0F * unit, 140.0F * unit);
    const math::Rect button{center.x - width * 0.5F, center.y, width, kButtonHeight * unit};
    renderer.drawRect(button, kButtonColor);
    if (action == focused) {
        renderer.drawRectOutline(button, std::max(2.0F, 2.0F * unit), kBarColor);
    }
    renderer.drawText(font, label, {button.x + (width - size.x) * 0.5F, button.y + (button.height - size.y) * 0.5F}, style);
    return button;
}

void UpdateScreen::run(Action action) {
    if (!hasFailed()) {
        return;
    }
    if (action == Action::Retry) {
        content.retry();
    } else if (canQuit()) {
        engine.quit(1);
    }
}

void UpdateScreen::moveFocus() {
    focused = focused == Action::Retry && canQuit() ? Action::Quit : Action::Retry;
}

void UpdateScreen::press(math::Vec2 point) {
    if (retryButton.contains(point)) {
        run(Action::Retry);
    } else if (quitButton.contains(point)) {
        run(Action::Quit);
    }
}

void UpdateScreen::handleEvent(const platform::Event& event) {
    switch (event.type) {
    case platform::Event::Type::KeyDown:
        if (event.repeat) {
            break;
        }
        if (event.key == input::Key::Left || event.key == input::Key::Right || event.key == input::Key::Tab) {
            moveFocus();
        } else if (event.key == input::Key::Enter || event.key == input::Key::Space) {
            run(focused);
        } else if (event.key == input::Key::R) {
            run(Action::Retry);
        }
        break;
    case platform::Event::Type::MouseDown:
        if (event.mouseButton == input::MouseButton::Left) {
            press(engine.getViewport().toDesign(event.position));
        }
        break;
    case platform::Event::Type::TouchBegan:
        for (std::size_t index = 0; index < event.touchCount; ++index) {
            if (event.touches[index].changed) {
                press(engine.getViewport().toDesign(event.touches[index].position));
            }
        }
        break;
    default:
        break;
    }
}

void UpdateScreen::update() {
    const input::Input& input = engine.getInput();
    for (std::size_t index = 0; index < input::Input::kMaxGamepads; ++index) {
        if (input.isGamepadPressed(index, input::GamepadButton::DpadLeft) || input.isGamepadPressed(index, input::GamepadButton::DpadRight)) {
            moveFocus();
        }
        if (input.isGamepadPressed(index, input::GamepadButton::South)) {
            run(focused);
        }
    }
}

} // namespace haylen::content
