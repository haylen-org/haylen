#pragma once

#include "haylen/graphics/Texture.hpp"

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::core {

// The look of a scene transition. The scene manager eases the progress, renders the scenes before and after the change into the outgoing and incoming images and asks the effect to draw them over the frame. An effect that covers the screen at its switch progress holds there while the next scene loads, and an effect with a switch progress of 0 shows both scenes throughout and starts once the next scene loaded.
class TransitionEffect {
  public:
    // The two images of a transition, each with the pixel size of the viewport. The incoming image is empty until the next scene enters, and the outgoing image keeps the last frame of the scenes before the change once they stop rendering into it.
    struct Frames {
        graphics::Texture outgoing;
        graphics::Texture incoming;
    };

    virtual ~TransitionEffect() = default;

    // Returns the eased progress, from 0 to 1, at which the effect covers the whole screen, where the scenes change, or 0 for an effect that shows both scenes throughout.
    [[nodiscard]] virtual float getSwitchProgress() const noexcept = 0;

    // Returns the eased progress after which an effect that shows both scenes no longer draws the outgoing image, where the scenes that leave the stack exit. An effect that covers the screen exits at its switch progress.
    [[nodiscard]] virtual float getExitProgress() const noexcept = 0;

    // Draws the effect over the frame at the eased progress, after every scene has rendered into its image.
    virtual void render(graphics2d::Renderer& renderer, const Frames& frames, float progress) = 0;
};

} // namespace haylen::core
