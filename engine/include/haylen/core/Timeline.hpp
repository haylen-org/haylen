#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Tween.hpp"

namespace haylen::core {

// A tween that plays other tweens and callbacks at their places in time. Appended items start a new step at the end, joined items run in parallel with the last step, and inserted items go at a time or a label. Timelines nest, repeat and yoyo like any tween, and every tween inside renders from the timeline, so seeking and reversing a timeline moves everything in it.
class Timeline final : public Tween {
  public:
    // Where a stagger starts: from the first tween, from the last one or from the middle of the list outwards.
    enum class StaggerOrigin : std::uint8_t {
        Start,
        End,
        Center,
    };

    Timeline() = default;

    // A tween joins a timeline before it starts, leaving the manager if it was playing there. Its delay pushes it further along the timeline.
    Timeline& append(std::shared_ptr<Tween> tween);
    Timeline& join(std::shared_ptr<Tween> tween);
    Timeline& insert(float at, std::shared_ptr<Tween> tween);
    Timeline& insert(std::string_view label, std::shared_ptr<Tween> tween);

    Timeline& appendInterval(float seconds);
    Timeline& appendCall(std::function<void()> call);
    Timeline& joinCall(std::function<void()> call);
    Timeline& insertCall(float at, std::function<void()> call);

    // Names a time, the end of the timeline by default, for `insert` and `seekLabel`.
    Timeline& addLabel(std::string name);
    Timeline& addLabel(std::string name, float at);
    [[nodiscard]] float getLabelTime(std::string_view name) const;
    void seekLabel(std::string_view name);

    // Appends the tweens as one step, each one starting `each` seconds after the previous one in the order the origin gives.
    Timeline& stagger(std::vector<std::shared_ptr<Tween>> tweens, float each, StaggerOrigin origin = StaggerOrigin::Start);

    [[nodiscard]] float getDuration() const noexcept override {
        return duration;
    }
    [[nodiscard]] std::size_t size() const noexcept;

  protected:
    void renderLoop(float loopTime, float previous, int loops, bool silent) override;
    void killChildren() override;
    bool killTarget(const void* target) override;
    void yieldTo(const PropertyTween& newer) override;
    void discard() noexcept override;

  private:
    friend class Tween;

    struct Child {
        std::shared_ptr<Tween> tween;
        std::function<void()> call;
        float start = 0.0F;
        int step = -1;
        bool removed = false;
    };

    // Guards the list of children while the timeline walks it, so tweens that leave during the walk are only erased once it ends.
    class WalkGuard final {
      public:
        explicit WalkGuard(Timeline& owner) noexcept;
        ~WalkGuard();

        WalkGuard(const WalkGuard&) = delete;
        WalkGuard& operator=(const WalkGuard&) = delete;

      private:
        Timeline& timeline;
    };

    void place(std::shared_ptr<Tween> tween, float at, int step);
    void placeCall(std::function<void()> call, float at, int step);
    void remove(const Tween& child);
    void compact();
    void updateDuration();
    [[nodiscard]] static float getExtent(const Tween& tween) noexcept;

    std::vector<Child> children;
    std::vector<float> stepEnds;
    std::map<std::string, float, std::less<>> labels;
    float duration = 0.0F;
    float lastStepStart = 0.0F;
    int walking = 0;
};

} // namespace haylen::core
