#include "haylen/ui/Presentation.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "haylen/ui/Navigation.hpp"
#include "haylen/ui/Screen.hpp"

namespace haylen::ui {

Presentation::Presentation(Navigation& owner, std::shared_ptr<Screen> presented, Options settings) : navigation(owner), screen(std::move(presented)), options(std::move(settings)) {
    if (!screen) {
        throw std::invalid_argument("A presentation needs a screen.");
    }
    if (screen->getStack() || screen->getPresentation()) {
        throw std::invalid_argument("The screen already shows in a navigation stack or in a presentation.");
    }
    if (options.maxWidth <= 0.0F || options.maxHeight <= 0.0F || (options.width && *options.width <= 0.0F)) {
        throw std::invalid_argument("The widths and heights of a presentation must be positive.");
    }
    if (options.detents.empty()) {
        throw std::invalid_argument("A sheet needs at least one detent.");
    }
    if (options.detent >= options.detents.size()) {
        throw std::invalid_argument("The detent a sheet opens at must be one of its detents.");
    }
    if (std::ranges::any_of(options.detents, [](const Detent& entry) { return entry.kind == Detent::Kind::Height && entry.height <= 0.0F; })) {
        throw std::invalid_argument("The height of a detent must be positive.");
    }
    if (!options.modal && options.style != Style::Sheet) {
        throw std::invalid_argument("Only a sheet can leave the input of the app under it, so only a sheet can be not modal.");
    }
    if (options.style == Style::Popover && !options.anchor) {
        throw std::invalid_argument("A popover needs the node it points at as its anchor.");
    }

    detent = options.detent;
    NavigationTransition fallback{.effect = NavigationTransition::Effect::Rise};
    if (options.style == Style::Dialog || options.style == Style::Popover) {
        fallback = {.effect = NavigationTransition::Effect::Fade, .duration = kDialogDuration};
    } else if (options.style == Style::Drawer) {
        fallback = {.effect = NavigationTransition::Effect::Slide};
    }

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

void Presentation::dismiss(core::Json value, const std::optional<NavigationTransition>& leaving) {
    if (isDismissed()) {
        return;
    }

    if (phase == Phase::Arriving) {
        finishArriving();
    }

    phase = Phase::Leaving;
    elapsed = 0.0F;
    dragging = false;
    result = std::move(value);
    if (leaving) {
        transition = leaving->reduced(navigation.isMotionReduced());
    }

    navigation.beginDisappear(*screen);
    if (phase == Phase::Leaving && transition.isInstant()) {
        finishLeaving();
    }
}

// The surface glides from where it shows to the new detent, so a sheet never jumps.
void Presentation::setDetent(std::size_t index) {
    if (index >= options.detents.size()) {
        throw std::invalid_argument("A sheet has no detent at that place in its list.");
    }

    if (index == detent) {
        return;
    }

    if (lastFrame) {
        const float shown = getSheetLayout(*lastFrame).surface.y;
        offset = shown - getDetentTop(*lastFrame, index);
    }

    detent = index;
    detentChanged.emit(*this);
}

void Presentation::advance(float deltaSeconds) {
    if (!dragging && offset != 0.0F && phase != Phase::Leaving) {
        offset -= offset * std::min(1.0F, deltaSeconds * kGlideSpeed);
        offset = std::fabs(offset) < 0.5F ? 0.0F : offset;
    }

    if (phase != Phase::Arriving && phase != Phase::Leaving) {
        return;
    }

    // It arrives once its screen laid out with every picture it shows, so nothing pops in as it rises, or once it waited the longest a change waits.
    if (phase == Phase::Arriving && !moving) {
        moving = navigation.isReady(*screen) || waited >= Navigation::kReadyWait;
        waited += deltaSeconds;
        if (!moving) {
            return;
        }
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

// A screen is compact while its safe area is narrower than a form sheet with its margins, such as a phone in portrait, or too short to float a card in.
bool Presentation::isCompact(const Frame& frame) const {
    return frame.safe.width < kSheetWidth + kSheetMargin * 2.0F || frame.safe.height < kSheetMargin * 4.0F;
}

bool Presentation::isSheet(const Frame& frame) const {
    return options.style == Style::Sheet || (options.style == Style::Popover && isCompact(frame));
}

// A detent rests the top of the surface: at the top of the safe area for the large one, at half the screen for the medium one, and above the bottom of the safe area by the height of the screen, or the height of the detent, plus the band of the grabber.
float Presentation::getDetentTop(const Frame& frame, std::size_t index) const {
    const float largeTop = frame.safe.y + kSheetTop;
    const bool grabbed = options.grabber.value_or(options.detents.size() > 1);
    const float band = grabbed ? kGrabberBand : 0.0F;
    const Detent& entry = options.detents[index];
    float top = largeTop;
    switch (entry.kind) {
    case Detent::Kind::Large:
        break;
    case Detent::Kind::Medium:
        top = frame.display.getBottom() - frame.display.height * 0.5F;
        break;
    case Detent::Kind::Fit:
        top = frame.safe.getBottom() - frame.contentHeight - band;
        break;
    case Detent::Kind::Height:
        top = frame.safe.getBottom() - entry.height - band;
        break;
    }

    return std::clamp(top, largeTop, std::max(largeTop, frame.safe.getBottom() - band));
}

Presentation::Layout Presentation::getLayout(const Frame& frame) const {
    switch (options.style) {
    case Style::FullScreen:
        return {.area = frame.safe, .surface = frame.display};
    case Style::Dialog:
        return {.area = frame.safe};
    case Style::Drawer:
        return getDrawerLayout(frame);
    case Style::Popover:
        return isCompact(frame) ? getSheetLayout(frame) : getPopoverLayout(frame);
    case Style::Sheet:
        break;
    }

    return getSheetLayout(frame);
}

float Presentation::getAreaWidth(const Frame& frame) const {
    return getLayout(frame).area.width;
}

// A sheet rests at its detent, grows and shrinks as a drag moves it between detents, stretches a little past its highest detent, and moves down whole below its lowest one. A form sheet on a wide screen floats as a card in the middle that a drag moves down whole.
Presentation::Layout Presentation::getSheetLayout(const Frame& frame) const {
    const bool compact = isCompact(frame);
    const bool grabbed = options.grabber.value_or(options.detents.size() > 1);
    const float band = grabbed ? kGrabberBand : 0.0F;
    const float radius = frame.radius;
    Layout layout{.compact = compact};
    if (!compact && options.size == Size::Form && options.style == Style::Sheet) {
        const float width = std::min(options.maxWidth, frame.safe.width - kSheetMargin * 2.0F);
        const float height = std::min(options.maxHeight, frame.safe.height - kSheetMargin * 2.0F);
        const float moved = offset >= 0.0F ? offset : offset * kStretch;
        const math::Rect card{frame.safe.getCenter().x - width * 0.5F, frame.safe.getCenter().y - height * 0.5F + moved, width, height};
        layout.surface = card;
        layout.area = {card.x, card.y + band, card.width, card.height - band};
        layout.radii = {radius, radius, radius, radius};
        layout.grabber = grabbed ? math::Rect::fromCenter({card.getCenter().x, card.y + band * 0.5F}, kGrabberSize) : math::Rect{};
        return layout;
    }

    float highest = frame.display.getBottom();
    float lowest = frame.display.y;
    for (std::size_t index = 0; index < options.detents.size(); ++index) {
        const float top = getDetentTop(frame, index);
        highest = std::min(highest, top);
        lowest = std::max(lowest, top);
    }

    float top = getDetentTop(frame, detent) + offset;
    if (top < highest) {
        top = highest - (highest - top) * kStretch;
    }

    const float resized = std::min(top, lowest);
    const float shift = top - resized;
    float left = frame.display.x;
    float width = frame.display.width;
    float areaLeft = frame.safe.x;
    float areaWidth = frame.safe.width;
    if (!compact) {
        width = std::min(kPageWidth, frame.safe.width - kSheetMargin * 2.0F);
        left = frame.safe.getCenter().x - width * 0.5F;
        areaLeft = left;
        areaWidth = width;
    }

    layout.surface = {left, resized + shift, width, frame.display.getBottom() - resized};
    layout.area = {areaLeft, resized + band + shift, areaWidth, std::max(0.0F, frame.safe.getBottom() - resized - band)};
    layout.radii = {radius, radius, 0.0F, 0.0F};
    layout.grabber = grabbed ? math::Rect::fromCenter({left + width * 0.5F, layout.surface.y + band * 0.5F}, kGrabberSize) : math::Rect{};

    // The GUIs behind a modal sheet shrink as it nears the top of the screen, from half the screen up.
    const float half = frame.display.getBottom() - frame.display.height * 0.5F;
    const float largeTop = frame.safe.y + kSheetTop;
    if (compact && options.modal && half > largeTop) {
        const float near = std::clamp((half - top) / (half - largeTop), 0.0F, 1.0F);
        layout.behindScale = 1.0F - (1.0F - kBehindScale) * near;
    }

    return layout;
}

// A drawer covers the whole height at the start edge, as wide as its width within most of the screen, rounds its end corners, and a drag toward the start moves it out.
Presentation::Layout Presentation::getDrawerLayout(const Frame& frame) const {
    const float width = std::min(options.width.value_or(kDrawerWidth), frame.display.width * 0.85F);
    const float moved = std::max(0.0F, offset);
    const float radius = frame.radius;
    Layout layout;
    if (frame.rightToLeft) {
        const float left = frame.display.getRight() - width + moved;
        const float inset = frame.display.getRight() - frame.safe.getRight();
        layout.surface = {left, frame.display.y, width, frame.display.height};
        layout.area = {left, frame.safe.y, std::max(0.0F, width - inset), frame.safe.height};
        layout.radii = {radius, 0.0F, 0.0F, radius};
        return layout;
    }

    const float left = frame.display.x - moved;
    const float inset = frame.safe.x - frame.display.x;
    layout.surface = {left, frame.display.y, width, frame.display.height};
    layout.area = {left + inset, frame.safe.y, std::max(0.0F, width - inset), frame.safe.height};
    layout.radii = {0.0F, radius, radius, 0.0F};
    return layout;
}

// A popover sits under its anchor when its screen fits there or there is more room below than above, and over it otherwise, within the safe area, with its arrow at the middle of the anchor as far as its rounded corners allow. A popover whose anchor did not draw floats in the middle.
Presentation::Layout Presentation::getPopoverLayout(const Frame& frame) const {
    const float width = std::min(options.width.value_or(kPopoverWidth), frame.safe.width - kSheetMargin * 2.0F);
    const float radius = frame.radius;
    Layout layout{.radii = {radius, radius, radius, radius}};
    if (!frame.anchor) {
        const float height = std::min(frame.contentHeight, frame.safe.height - kSheetMargin * 2.0F);
        layout.surface = math::Rect::fromCenter(frame.safe.getCenter(), {width, height});
        layout.area = layout.surface;
        return layout;
    }

    const math::Rect& anchor = *frame.anchor;
    const float below = frame.safe.getBottom() - anchor.getBottom() - kArrowSize - kSheetMargin;
    const float above = anchor.y - frame.safe.y - kArrowSize - kSheetMargin;
    const bool under = frame.contentHeight <= below || below >= above;
    const float height = std::max(0.0F, std::min(frame.contentHeight, under ? below : above));
    const float left = std::clamp(anchor.getCenter().x - width * 0.5F, frame.safe.x + kSheetMargin, std::max(frame.safe.x + kSheetMargin, frame.safe.getRight() - kSheetMargin - width));
    const float top = under ? anchor.getBottom() + kArrowSize : anchor.y - kArrowSize - height;
    layout.surface = {left, top, width, height};
    layout.area = layout.surface;
    const float tipX = std::clamp(anchor.getCenter().x, left + radius + kArrowSize, std::max(left + radius + kArrowSize, left + width - radius - kArrowSize));
    layout.arrow = math::Vec2{tipX, under ? anchor.getBottom() : anchor.y};
    layout.arrowUp = under;
    return layout;
}

void Presentation::drag(float distance, float value) {
    dragging = true;
    offset = distance;
    speed = value;
}

// A sheet heads to the detent nearest to where its speed would carry it, and past half the room under its lowest detent it leaves. A card, a drawer and a page leave once dragged a third of their length or flung, and glide back otherwise.
void Presentation::release() {
    dragging = false;
    if (!lastFrame) {
        return;
    }

    const Frame& frame = *lastFrame;
    const Layout layout = getLayout(frame);
    const bool sliding = options.style == Style::Drawer || (!isCompact(frame) && options.size == Size::Form && options.style == Style::Sheet);
    if (sliding) {
        const float length = options.style == Style::Drawer ? layout.surface.width : layout.surface.height;
        if (offset > length / 3.0F || (speed > kFlingSpeed && offset > 0.0F)) {
            static_cast<void>(dismissByPerson());
        }

        return;
    }

    // The sheet heads where its speed carries it, and the detent nearest to that place takes it.
    const float top = layout.surface.y;
    const float heading = top + speed * kCarrySeconds;
    std::size_t nearest = detent;
    std::size_t lowest = 0;
    float closest = -1.0F;
    for (std::size_t index = 0; index < options.detents.size(); ++index) {
        const float rest = getDetentTop(frame, index);
        if (rest > getDetentTop(frame, lowest)) {
            lowest = index;
        }
        if (closest < 0.0F || std::fabs(rest - heading) < closest) {
            closest = std::fabs(rest - heading);
            nearest = index;
        }
    }

    // Past half the room under the lowest detent, or flung down below it, the sheet leaves, unless its screen keeps it, and then it rests at its lowest detent.
    const float lowestTop = getDetentTop(frame, lowest);
    const bool leaving = heading > lowestTop + (frame.display.getBottom() - lowestTop) * 0.5F || (speed > kFlingSpeed && top > lowestTop);
    if (leaving && (dismissByPerson() || isDismissed())) {
        return;
    }

    if (leaving) {
        nearest = lowest;
    }

    offset = top - getDetentTop(frame, nearest);
    if (nearest != detent) {
        detent = nearest;
        detentChanged.emit(*this);
    }
}

bool Presentation::dismissByPerson() {
    if (!options.dismissible || isDismissed()) {
        return false;
    }

    if (navigation.askBack(*screen)) {
        return false;
    }

    dismiss();
    return true;
}

} // namespace haylen::ui
