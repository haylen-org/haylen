#pragma once

#include <vector>

#include "haylen/core/TransitionEffect.hpp"

namespace haylen::test {

// Transition effect that records the progress and the images it is asked to draw, with the switch and exit points it is given.
class RecordingEffect final : public core::TransitionEffect {
  public:
    RecordingEffect(float switchAt, float exitAt) : switchProgress(switchAt), exitProgress(exitAt) {}

    [[nodiscard]] float getSwitchProgress() const noexcept override {
        return switchProgress;
    }
    [[nodiscard]] float getExitProgress() const noexcept override {
        return exitProgress;
    }
    void render(graphics2d::Renderer&, const Frames& frames, float progress) override {
        progresses.push_back(progress);
        last = frames;
    }

    std::vector<float> progresses;
    Frames last;

  private:
    float switchProgress;
    float exitProgress;
};

} // namespace haylen::test
