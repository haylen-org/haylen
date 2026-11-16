#include "haylen/ui/Presentation.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "haylen/ui/Navigation.hpp"
#include "haylen/ui/Screen.hpp"

namespace haylen::ui {

Presentation::Presentation(Navigation& owner, std::shared_ptr<Screen> presented, Options settings) : navigation(owner), screen(std::move(presented)), options(settings) {
    if (!screen) {
        throw std::invalid_argument("A presentation needs a screen.");
    }
    if (screen->getStack() || screen->getPresentation()) {
        throw std::invalid_argument("The screen already shows in a navigation stack or in a presentation.");
    }
    if (options.maxWidth <= 0.0F || options.maxHeight <= 0.0F) {
        throw std::invalid_argument("The largest width and height of a sheet must be positive.");
    }

    const NavigationTransition fallback = options.style == Style::Dialog ? NavigationTransition{.effect = NavigationTransition::Effect::Fade, .duration = kDialogDuration} : NavigationTransition{.effect = NavigationTransition::Effect::Rise};
    transition = options.transition.value_or(fallback).reduced(navigation.isMotionReduced());
}

Presentation::~Presentation() = default;

std::optional<Presentation::Style> Presentation::styleFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kStyleNames, name, &std::pair<std::string_view, Style>::first);
    return found != kStyleNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Presentation::styleName(Style value) noexcept {
    return std::ranges::find(kStyleNames, value, &std::pair<std::string_view, Style>::second)->first;
}

void Presentation::open() {
    navigation.place(*screen, shared_from_this());
    navigation.beginAppear(*screen);
    if (phase == Phase::Arriving && transition.isInstant()) {
        finishArriving();
    }
}

void Presentation::dismiss(const std::optional<NavigationTransition>& leaving) {
    if (isDismissed()) {
        return;
    }

    if (phase == Phase::Arriving) {
        finishArriving();
    }

    phase = Phase::Leaving;
    elapsed = 0.0F;
    if (leaving) {
        transition = leaving->reduced(navigation.isMotionReduced());
    }

    navigation.beginDisappear(*screen);
    if (phase == Phase::Leaving && transition.isInstant()) {
        finishLeaving();
    }
}

void Presentation::advance(float deltaSeconds) {
    if (phase != Phase::Arriving && phase != Phase::Leaving) {
        return;
    }

    elapsed += deltaSeconds;
    if (elapsed < transition.duration) {
        return;
    }

    if (phase == Phase::Arriving) {
        finishArriving();
        return;
    }

    finishLeaving();
}

void Presentation::finishArriving() {
    phase = Phase::Shown;
    if (screen->getState() == Screen::State::Appearing) {
        navigation.endAppear(*screen);
    }
}

// The presentation leaves the navigation before its signal, so a handler that presents again finds this one gone.
void Presentation::finishLeaving() {
    phase = Phase::Gone;
    const std::shared_ptr<Presentation> self = shared_from_this();
    navigation.hideAtOnce(*screen);
    navigation.removeScreen(*screen);
    navigation.forget(*this);
    dismissed.emit(*self);
}

float Presentation::getDistance() const {
    const float linear = transition.duration > 0.0F ? std::clamp(elapsed / transition.duration, 0.0F, 1.0F) : 1.0F;
    switch (phase) {
    case Phase::Arriving:
        return 1.0F - transition.ease.apply(linear);
    case Phase::Shown:
        return 0.0F;
    case Phase::Leaving:
        return transition.ease.apply(linear);
    case Phase::Gone:
        return 1.0F;
    }

    return 1.0F;
}

// A sheet fills the width of a narrow screen from a little below the top of the safe area, on a surface that reaches the bottom edge, and floats as a card in the middle of a wide one.
Presentation::Layout Presentation::getLayout(const math::Rect& display, const math::Rect& safe, float radius) const {
    switch (options.style) {
    case Style::FullScreen:
        return {.area = safe, .surface = display, .radii = {}};
    case Style::Dialog:
        return {.area = safe, .surface = {}, .radii = {}};
    case Style::Sheet:
        break;
    }

    const bool narrow = safe.width < options.maxWidth + kSheetMargin * 2.0F || safe.height < kSheetMargin * 4.0F;
    if (narrow) {
        const float top = safe.y + kSheetTop;
        const math::Rect area{safe.x, top, safe.width, std::max(0.0F, safe.getBottom() - top)};
        const math::Rect surface{display.x, top, display.width, std::max(0.0F, display.getBottom() - top)};
        return {.area = area, .surface = surface, .radii = {radius, radius, 0.0F, 0.0F}};
    }

    const float width = std::min(options.maxWidth, safe.width - kSheetMargin * 2.0F);
    const float height = std::min(options.maxHeight, safe.height - kSheetMargin * 2.0F);
    const math::Rect area{safe.getCenter().x - width * 0.5F, safe.getCenter().y - height * 0.5F, width, height};
    return {.area = area, .surface = area, .radii = {radius, radius, radius, radius}};
}

} // namespace haylen::ui
