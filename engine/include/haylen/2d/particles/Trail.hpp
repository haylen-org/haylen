#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/2d/graphics/MeshVertex.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::particles2d {

// A ribbon that follows a moving point, such as a blade tip, a projectile or a streamer. It adds a point whenever its position moves `minDistance` away from the last one and drops the points older than its lifetime, and its width and colors go from the head to the tail. A trail that stops emitting keeps fading until its last point expires.
class Trail final {
  public:
    struct Options {
        float lifetime = 0.4F;
        float minDistance = 4.0F;
        std::size_t maxPoints = 64;
        float widthStart = 12.0F;
        float widthEnd = 0.0F;
        std::vector<math::Color> colors{math::Color::white(), math::Color::transparent()};
        graphics::Texture texture;
        graphics2d::DrawOrder order{};
    };

    explicit Trail(Options settings);

    void update(float deltaSeconds);
    void draw(graphics2d::Renderer& renderer) const;
    void clear() noexcept;

    [[nodiscard]] std::size_t getCount() const noexcept {
        return points.size();
    }
    [[nodiscard]] bool isAlive() const noexcept {
        return emitting || !points.empty();
    }
    [[nodiscard]] const Options& getOptions() const noexcept {
        return options;
    }
    void setOptions(Options value);

    math::Vec2 position{};
    bool emitting = true;

  private:
    struct Point {
        math::Vec2 position{};
        float age = 0.0F;
    };

    static constexpr std::size_t kMaxPoints = 4096;

    static void validate(const Options& settings);

    Options options;

    // The points of the ribbon from the oldest to the newest.
    std::vector<Point> points;
    mutable std::vector<math::Vec2> line;
    mutable std::vector<float> places;
    mutable std::vector<graphics2d::MeshVertex> vertices;
    mutable std::vector<std::uint32_t> indices;
};

} // namespace haylen::particles2d
