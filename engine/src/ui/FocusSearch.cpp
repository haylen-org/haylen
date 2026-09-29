#include "ui/FocusSearch.hpp"

#include <algorithm>
#include <cmath>

namespace haylen::ui {

std::optional<std::size_t> FocusSearch::find(const math::Rect& source, std::span<const math::Rect> candidates, Direction direction) {
    std::optional<std::size_t> best;
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        if (!isCandidate(source, candidates[index], direction)) {
            continue;
        }
        if (!best || isBetter(source, candidates[index], candidates[*best], direction)) {
            best = index;
        }
    }
    return best;
}

math::Rect FocusSearch::wrap(const math::Rect& source, const math::Rect& bounds, Direction direction) {
    switch (direction) {
    case Direction::Left:
        return {bounds.getRight() + 1.0F, source.y, source.width, source.height};
    case Direction::Right:
        return {bounds.x - source.width - 1.0F, source.y, source.width, source.height};
    case Direction::Up:
        return {source.x, bounds.getBottom() + 1.0F, source.width, source.height};
    case Direction::Down:
        break;
    }
    return {source.x, bounds.y - source.height - 1.0F, source.width, source.height};
}

bool FocusSearch::isHorizontal(Direction direction) noexcept {
    return direction == Direction::Left || direction == Direction::Right;
}

// A target outside the beam of the source also has to lie wholly past the source, so moving right at the end of a row never drops to a longer row below.
bool FocusSearch::isCandidate(const math::Rect& source, const math::Rect& target, Direction direction) noexcept {
    bool ahead = false;
    switch (direction) {
    case Direction::Left:
        ahead = (source.getRight() > target.getRight() || source.x >= target.getRight()) && source.x > target.x;
        break;
    case Direction::Right:
        ahead = (source.x < target.x || source.getRight() <= target.x) && source.getRight() < target.getRight();
        break;
    case Direction::Up:
        ahead = (source.getBottom() > target.getBottom() || source.y >= target.getBottom()) && source.y > target.y;
        break;
    case Direction::Down:
        ahead = (source.y < target.y || source.getBottom() <= target.y) && source.getBottom() < target.getBottom();
        break;
    }
    return ahead && (beamsOverlap(source, target, direction) || isToDirectionOf(source, target, direction));
}

bool FocusSearch::beamsOverlap(const math::Rect& source, const math::Rect& target, Direction direction) noexcept {
    if (isHorizontal(direction)) {
        return target.getBottom() > source.y && target.y < source.getBottom();
    }
    return target.getRight() > source.x && target.x < source.getRight();
}

bool FocusSearch::isToDirectionOf(const math::Rect& source, const math::Rect& target, Direction direction) noexcept {
    switch (direction) {
    case Direction::Left:
        return source.x >= target.getRight();
    case Direction::Right:
        return source.getRight() <= target.x;
    case Direction::Up:
        return source.y >= target.getBottom();
    case Direction::Down:
        break;
    }
    return source.getBottom() <= target.y;
}

float FocusSearch::getMajorAxisDistance(const math::Rect& source, const math::Rect& target, Direction direction) noexcept {
    switch (direction) {
    case Direction::Left:
        return std::max(0.0F, source.x - target.getRight());
    case Direction::Right:
        return std::max(0.0F, target.x - source.getRight());
    case Direction::Up:
        return std::max(0.0F, source.y - target.getBottom());
    case Direction::Down:
        break;
    }
    return std::max(0.0F, target.y - source.getBottom());
}

float FocusSearch::getMajorAxisDistanceToFarEdge(const math::Rect& source, const math::Rect& target, Direction direction) noexcept {
    switch (direction) {
    case Direction::Left:
        return std::max(1.0F, source.x - target.x);
    case Direction::Right:
        return std::max(1.0F, target.getRight() - source.getRight());
    case Direction::Up:
        return std::max(1.0F, source.y - target.y);
    case Direction::Down:
        break;
    }
    return std::max(1.0F, target.getBottom() - source.getBottom());
}

float FocusSearch::getMinorAxisDistance(const math::Rect& source, const math::Rect& target, Direction direction) noexcept {
    if (isHorizontal(direction)) {
        return std::fabs(source.getCenter().y - target.getCenter().y);
    }
    return std::fabs(source.getCenter().x - target.getCenter().x);
}

// A candidate in the beam of the source beats one outside it, unless the one outside lies closer than the far edge of the one inside.
bool FocusSearch::beamBeats(const math::Rect& source, const math::Rect& first, const math::Rect& second, Direction direction) noexcept {
    if (beamsOverlap(source, second, direction) || !beamsOverlap(source, first, direction)) {
        return false;
    }
    if (!isToDirectionOf(source, second, direction) || isHorizontal(direction)) {
        return true;
    }
    return getMajorAxisDistance(source, first, direction) < getMajorAxisDistanceToFarEdge(source, second, direction);
}

bool FocusSearch::isBetter(const math::Rect& source, const math::Rect& first, const math::Rect& second, Direction direction) noexcept {
    if (!isCandidate(source, first, direction)) {
        return false;
    }
    if (!isCandidate(source, second, direction) || beamBeats(source, first, second, direction)) {
        return true;
    }
    if (beamBeats(source, second, first, direction)) {
        return false;
    }

    // Of two candidates in the beam, one that lies wholly before the other is nearer, however wide the source is, so a narrow checkbox under a full-width row wins over a centered control further down.
    if (beamsOverlap(source, first, direction) && beamsOverlap(source, second, direction)) {
        if (getMajorAxisDistanceToFarEdge(source, first, direction) <= getMajorAxisDistance(source, second, direction)) {
            return true;
        }
        if (getMajorAxisDistanceToFarEdge(source, second, direction) <= getMajorAxisDistance(source, first, direction)) {
            return false;
        }
    }

    const float firstMajor = getMajorAxisDistance(source, first, direction);
    const float firstMinor = getMinorAxisDistance(source, first, direction);
    const float secondMajor = getMajorAxisDistance(source, second, direction);
    const float secondMinor = getMinorAxisDistance(source, second, direction);
    return kMajorAxisWeight * firstMajor * firstMajor + firstMinor * firstMinor < kMajorAxisWeight * secondMajor * secondMajor + secondMinor * secondMinor;
}

} // namespace haylen::ui
