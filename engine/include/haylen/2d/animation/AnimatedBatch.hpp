#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/2d/animation/Animation.hpp"
#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/2d/graphics/PartColors.hpp"
#include "haylen/2d/graphics/SpriteBatch.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/2d/graphics/SpriteLayout.hpp"
#include "haylen/core/Signal.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::animation2d {

// A sprite batch whose sprites each play an animation of a shared set with their own time, speed and scale, advanced in C++ without a call from scripts per sprite. Every update writes the source, size and pivot of the frame each sprite shows into the batch, mirrored where the sprite flips, and the batch draws in one call. The animations of a batch share its texture and all count seconds or all count ticks.
class AnimatedBatch final {
  public:
    // Batches this large advance their sprites on the worker pool while the frame thread takes its own share.
    static constexpr std::size_t kParallelSprites = 4096;
    static constexpr std::size_t kParallelGrain = 2048;

    explicit AnimatedBatch(graphics::Texture image);

    // Adds or replaces an animation. Throws `std::invalid_argument` for an animation of another texture, without frames, or of the other unit than the animations the batch has.
    void addAnimation(std::string name, Animation animation);
    [[nodiscard]] bool hasAnimation(std::string_view name) const noexcept;
    [[nodiscard]] std::vector<std::string> getAnimationNames() const;

    // Adds a sprite that plays an animation from its start, and returns its index. Throws `std::invalid_argument` for an animation the batch lacks.
    std::size_t add(const graphics2d::SpriteInstance& sprite, std::string_view animation, float speed = 1.0F);
    void remove(std::size_t index);
    void clear() noexcept;
    void reserve(std::size_t count);
    [[nodiscard]] std::size_t size() const noexcept {
        return states.size();
    }

    // Switches a sprite to an animation, from its start unless it already plays it and `restart` is false.
    void play(std::size_t index, std::string_view animation, bool restart = false);
    [[nodiscard]] const std::string& getAnimation(std::size_t index) const;
    [[nodiscard]] std::size_t getFrame(std::size_t index) const;
    [[nodiscard]] float getTime(std::size_t index) const;

    // Moves a sprite to a time of its animation and shows its frame there.
    void seek(std::size_t index, float value);

    // The length of one full pass of the animation of a sprite.
    [[nodiscard]] float getCycleDuration(std::size_t index) const;
    [[nodiscard]] float getSpeed(std::size_t index) const;
    void setSpeed(std::size_t index, float value);
    [[nodiscard]] math::Vec2 getScale(std::size_t index) const;
    void setScale(std::size_t index, math::Vec2 value);

    // The place, rotation, colors and flips of a sprite, whose source, size and pivot the next update writes again.
    [[nodiscard]] const graphics2d::SpriteInstance& get(std::size_t index) const;
    void set(std::size_t index, const graphics2d::SpriteInstance& sprite);
    void setPartColors(std::size_t index, const graphics2d::PartColors& colors);
    [[nodiscard]] graphics2d::PartColors getPartColors(std::size_t index) const;
    void writeFields(std::span<const float> values, const graphics2d::SpriteLayout& layout, std::size_t first = 0);
    void readFields(std::span<float> values, const graphics2d::SpriteLayout& layout, std::size_t first = 0) const;

    // Advances every sprite by the time, in seconds or in whole ticks as the animations count, times its speed, and lists the sprites whose one-shot animation ended. Each throws `std::logic_error` for animations of the other unit.
    void update(float deltaSeconds, core::JobSystem* jobs = nullptr);
    void step(std::uint32_t count, core::JobSystem* jobs = nullptr);

    // The sprites whose one-shot animation ended in the last update or step, in index order.
    [[nodiscard]] std::span<const std::size_t> getFinished() const noexcept {
        return endedSprites;
    }

    [[nodiscard]] const graphics::Texture& getTexture() const noexcept {
        return batch.getTexture();
    }
    void draw(graphics2d::Renderer& renderer, const graphics2d::DrawOrder& order = {}) const;

    // Emits the sprites whose one-shot animation ended, once per update that ended any.
    core::Signal<std::span<const std::size_t>> completed;

  private:
    struct State {
        std::uint32_t animation = 0;
        std::uint32_t frame = 0;
        float time = 0.0F;
        float speed = 1.0F;
        math::Vec2 scale{1.0F, 1.0F};
        bool ended = false;
        bool justEnded = false;
    };

    [[nodiscard]] std::uint32_t findAnimation(std::string_view name) const;
    void check(std::size_t index) const;
    void advance(float amount, core::JobSystem* jobs);

    // Writes the frame a sprite shows into its sprite of the batch.
    void show(std::size_t index);

    graphics2d::SpriteBatch batch;
    std::vector<std::string> names;
    std::vector<Animation> animations;
    std::vector<State> states;
    std::vector<std::size_t> endedSprites;
    bool ticks = false;
};

} // namespace haylen::animation2d
