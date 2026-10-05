#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/2d/particles/Effect.hpp"
#include "haylen/2d/particles/Emitter.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::particles2d {

// The emitters of an effect, which update, draw, move, scale, turn and restart together, such as the flash, fireball, smoke and debris of an explosion. Every emitter sits at its offset from the position of the system and draws in the order the effect lists it.
class System final {
  public:
    explicit System(const Effect& effect, std::uint64_t seed = 0);

    void update(float deltaSeconds);
    void update(float deltaSeconds, core::JobSystem& jobs);
    void draw(graphics2d::Renderer& renderer) const;
    void restart();
    void clear() noexcept;

    [[nodiscard]] std::size_t getCount() const noexcept;
    [[nodiscard]] bool isAlive() const noexcept;
    [[nodiscard]] bool isEmitting() const noexcept;
    void setEmitting(bool value);

    [[nodiscard]] std::size_t getEmitterCount() const noexcept {
        return parts.size();
    }
    [[nodiscard]] const std::shared_ptr<Emitter>& getEmitter(std::size_t index) const;
    [[nodiscard]] const std::string& getName(std::size_t index) const;

    // Returns the emitter with the name, or nothing when no emitter has it.
    [[nodiscard]] std::shared_ptr<Emitter> findEmitter(std::string_view name) const;

    math::Vec2 position{};
    float scale = 1.0F;
    float rotation = 0.0F;

  private:
    struct Part {
        std::string name;
        math::Vec2 offset{};
        float scale = 1.0F;
        std::shared_ptr<Emitter> emitter;
    };

    // Places every emitter at its offset, turned and scaled with the system.
    void place();

    std::vector<Part> parts;
};

} // namespace haylen::particles2d
