#pragma once

#include <cstddef>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <string_view>

#include "haylen/2d/animation/Animation.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {
struct Sprite;
}

namespace haylen::animation2d {

// Plays named animations and applies the current frame to sprites. Callbacks run inside `update` when the frame changes and when a non-looping animation ends.
class Animator final {
  public:
    using FrameCallback = std::function<void(std::string_view animation, std::size_t frame)>;
    using FinishCallback = std::function<void(std::string_view animation)>;

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
    void update(float deltaSeconds);

    // Sets the texture, source, size and pivot of the sprite from the current frame. The pivot is given in the untrimmed frame.
    void apply(graphics2d::Sprite& sprite) const;

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
    FinishCallback onFinish;

  private:
    std::map<std::string, Animation, std::less<>> animations;
    std::string current;
    std::deque<std::string> pending;
    float time = 0.0F;
    std::size_t frame = 0;
    bool playing = false;
    bool finishReported = false;
};

} // namespace haylen::animation2d
