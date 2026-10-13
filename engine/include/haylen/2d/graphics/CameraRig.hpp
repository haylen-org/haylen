#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {

// Directs a camera through the zones of a level and along a rail. A zone is an area of the world that, while the target is inside it, gives the camera its own limits, zoom and offset, such as a room that holds the view, an arena that zooms out or a corridor that looks ahead, and the camera eases into a zone it enters. A rail is a line the camera follows the target along, the point of the line nearest the target, such as the track of a side-scrolling level or a cutscene.
class CameraRig final {
  public:
    struct Zone {
        std::string name;
        math::Rect area{};

        // The limits of the camera inside the zone, its area when absent.
        std::optional<math::Rect> limits;
        std::optional<math::Vec2> zoom;
        math::Vec2 offset{};

        // A zone of higher priority wins where zones overlap, and between equal ones the zone the target is in already stays.
        int priority = 0;
    };

    // The limits and zoom the camera has outside every zone, and the seconds it takes to ease into a zone.
    struct Options {
        std::optional<math::Rect> limits;
        math::Vec2 zoom{1.0F, 1.0F};
        float blendTime = 0.5F;
    };

    explicit CameraRig(Options value);

    // Adds a zone, in place of the zone of the same name. Throws `std::invalid_argument` for a zone without a name or an area.
    void addZone(Zone zone);
    void removeZone(std::string_view name);
    void clearZones() noexcept;

    // Sets the points of the rail, and no points take it away. Throws `std::invalid_argument` for a single point.
    void setRail(std::vector<math::Vec2> points);

    // Returns the point of the rail nearest to the point, or the point itself without a rail.
    [[nodiscard]] math::Vec2 project(math::Vec2 point) const noexcept;

    // Picks the zone of the target, eases the limits, zoom and offset of the camera toward it and follows the target, or its point on the rail, with `Camera::follow`. Limits ease between two zones that have them, and arrive at once from outside every zone, where the limit smoothing of the camera eases the view.
    void update(Camera& camera, math::Vec2 target, float deltaSeconds, const math::Rect& screen);

    // Returns the zone the target is in, or nothing outside every zone.
    [[nodiscard]] const Zone* getZone() const noexcept;
    [[nodiscard]] std::span<const Zone> getZones() const noexcept {
        return zones;
    }
    [[nodiscard]] std::span<const math::Vec2> getRail() const noexcept {
        return rail;
    }
    [[nodiscard]] const Options& getOptions() const noexcept {
        return options;
    }
    void setOptions(const Options& value);

  private:
    // What the camera holds in a zone or outside every zone.
    struct Framing {
        std::optional<math::Rect> limits;
        math::Vec2 zoom{1.0F, 1.0F};
        math::Vec2 offset{};
    };

    [[nodiscard]] Framing framingOf(const Zone* zone) const;
    [[nodiscard]] static Framing mix(const Framing& start, const Framing& end, float amount);
    [[nodiscard]] std::optional<std::size_t> pick(math::Vec2 target) const noexcept;

    Options options;
    std::vector<Zone> zones;
    std::vector<math::Vec2> rail;
    std::optional<std::string> current;

    // The framing the camera has now, the one it eased from when the zone changed, and how far the easing has come, from 0 to 1.
    Framing applied;
    Framing from;
    float progress = 1.0F;
    bool started = false;
};

} // namespace haylen::graphics2d
