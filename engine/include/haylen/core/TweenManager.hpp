#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/FrameClock.hpp"
#include "haylen/core/Tween.hpp"

namespace haylen::core {

class PropertyTween;

// Plays root tweens on the frame clock. Each tween runs by its process mode on scaled or unscaled time, frame tweens advance in update and fixed-step tweens in fixedUpdate, and tweens with a tag share the time scale of their group. Updating allocates nothing, so thousands of tweens can run at once.
class TweenManager final {
  public:
    TweenManager() = default;
    ~TweenManager();

    TweenManager(const TweenManager&) = delete;
    TweenManager& operator=(const TweenManager&) = delete;

    // Starts playing the tween on the next update. A tween plays in one manager or timeline at a time.
    void add(std::shared_ptr<Tween> tween);

    void update(const FrameClock& clock);

    // Advances fixed-step tweens by one fixed step of the clock.
    void fixedUpdate(const FrameClock& clock);

    // Makes the tween the only one animating its fields: every other tween gives up the same fields of the same targets, and tweens left with nothing to animate are killed.
    void overwrite(const PropertyTween& tween);

    // Kills every tween, or tween inside a timeline, that animates the target.
    void killTarget(const void* target);

    // Acts on every root tween with the tag.
    void killTag(std::string_view tag);
    void completeTag(std::string_view tag, bool withCallbacks = true);
    void pauseTag(std::string_view tag, bool paused);

    // Scales the time of every tween with the tag, including tweens added later.
    void setTimeScale(std::string_view tag, float value);
    [[nodiscard]] float getTimeScale(std::string_view tag) const noexcept;

    // Kills every root tween, running their kill callbacks.
    void killAll();

    // Drops every tween without running any callback, which is what shutdown needs.
    void clear() noexcept;

    // Counts the root tweens that are still alive, including delayed, paused and finished ones that do not auto-kill.
    [[nodiscard]] std::size_t size() const noexcept;

  private:
    friend class Timeline;

    struct Group {
        std::string tag;
        float timeScale = 1.0F;
    };

    [[nodiscard]] std::size_t getGroup(std::string_view tag);
    void advanceAll(const FrameClock& clock, bool fixed);
    [[nodiscard]] std::vector<std::shared_ptr<Tween>> collectTagged(std::string_view tag) const;

    // Takes a tween out of the manager because it joined a timeline.
    void detach(Tween& tween);
    void finishUpdate();

    std::vector<std::shared_ptr<Tween>> tweens;
    std::vector<std::shared_ptr<Tween>> pending;
    std::vector<Group> groups{Group{}};
    bool updating = false;
};

} // namespace haylen::core
