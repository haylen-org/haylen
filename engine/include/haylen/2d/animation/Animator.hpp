#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "haylen/2d/animation/Animation.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {
struct Sprite;
}

namespace haylen::animation2d {

// Plays named animations and applies the current frame to sprites. Callbacks run inside `update` and `step`: for every frame the time enters, in order, even the frames a long step passes, with the events of each frame, and when a non-looping animation ends. The events of the first frame run with the first update after an animation starts.
class Animator final {
  public:
    using FrameCallback = std::function<void(std::string_view animation, std::size_t frame)>;
    using EventCallback = std::function<void(std::string_view animation, std::string_view event, std::size_t frame)>;
    using CompleteCallback = std::function<void(std::string_view animation)>;

    void add(std::string name, Animation animation);
    [[nodiscard]] bool has(std::string_view name) const;
    [[nodiscard]] const Animation& getAnimation(std::string_view name) const;

    // Switches animations and clears the queue. Playing the current animation again keeps its time unless `restart` is set.
    void play(std::string_view name, bool restart = false);

    // Plays an animation once the current pass ends: when a one-shot animation finishes or a looping one completes the cycle it is in. A finished one-shot animation hands over at once. Queued animations play in order, each from its first frame, even the animation that just finished.
    void queue(std::string_view name);
    void clearQueue() noexcept {
        pending.clear();
    }
    [[nodiscard]] std::size_t getQueuedCount() const noexcept {
        return pending.size();
    }

    void stop() noexcept;

    // Moves the current animation to a time and shows its frame there, without callbacks, such as when a character turns and keeps the step it was on.
    void seek(float value);

    // Advances an animation that counts seconds, or one that counts ticks by whole ticks. Each throws `std::logic_error` for an animation of the other kind.
    void update(float deltaSeconds);
    void step(std::uint32_t ticks);

    // Sets the texture, source, size and pivot of the sprite from the current frame. The pivot is the pivot of the frame, or the pivot of the animator in the untrimmed frame, mirrored where the sprite flips.
    void apply(graphics2d::Sprite& sprite) const;

    // Returns the box of the current frame with the name in the world, around a sprite placed, sized, scaled, turned and flipped as the sprite is, or nothing when the frame has no such box. A turned box returns the rectangle around its corners.
    [[nodiscard]] std::optional<math::Rect> getBox(std::string_view name, const graphics2d::Sprite& sprite) const;
    [[nodiscard]] std::optional<math::Vec2> getPoint(std::string_view name, const graphics2d::Sprite& sprite) const;

    [[nodiscard]] const std::string& getCurrent() const noexcept {
        return current;
    }
    [[nodiscard]] std::size_t getFrame() const noexcept {
        return frame;
    }
    [[nodiscard]] float getTime() const noexcept {
        return time;
    }
    [[nodiscard]] bool isPlaying() const noexcept {
        return playing;
    }
    [[nodiscard]] bool isFinished() const noexcept;

    float speed = 1.0F;
    math::Vec2 pivot{0.5F, 0.5F};
    FrameCallback onFrame;
    EventCallback onEvent;
    CompleteCallback onComplete;

  private:
    // A step that spans more passes than this runs only the last ones, so one call never visits an unbounded number of frames.
    static constexpr float kMaxPasses = 2.0F;

    [[nodiscard]] const SpriteFrame* getShown() const;

    // Maps a point of the untrimmed current frame into the world through the sprite.
    [[nodiscard]] math::Vec2 place(math::Vec2 point, const graphics2d::Sprite& sprite) const;
    void advance(const Animation& active, float amount);
    void enter(std::size_t shown);
    std::map<std::string, Animation, std::less<>> animations;
    std::string current;
    std::deque<std::string> pending;
    float time = 0.0F;
    std::size_t frame = 0;

    // Counts the animations played, which tells a step that a callback switched animations.
    std::uint64_t plays = 0;
    bool playing = false;
    bool entered = false;
    bool completeReported = false;
};

} // namespace haylen::animation2d
