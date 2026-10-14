#pragma once

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/2d/animation/Animation.hpp"
#include "haylen/2d/animation/SpriteFrame.hpp"
#include "haylen/2d/graphics/NineSlice.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Rect.hpp"

namespace haylen::animation2d {

// A sprite atlas JSON in the hash or array layout: frames packed into one texture, with frame tags and slices. Frame tags become animations, slices with a nine-slice center become nine-slices, and the other slices become the boxes, points and pivots of the frames their keys cover, while the user data of a frame names its events.
class SpriteAtlas final {
  public:
    // Reads the hash and array JSON layouts. The document keeps its keys in file order, because frame tags count frames by their position in the file. Rotated frames and tags that repeat a fixed number of times above one are rejected.
    [[nodiscard]] static SpriteAtlas parse(const nlohmann::ordered_json& document, graphics::Texture image);

    // Builds an atlas from frames and animations in the order given, such as those an atlas builder packs. Throws `std::invalid_argument` without a texture or for a name given twice.
    [[nodiscard]] static SpriteAtlas assemble(graphics::Texture image, std::vector<std::pair<std::string, SpriteFrame>> frameList, std::vector<std::pair<std::string, Animation>> animationList);

    // Returns the frames and animations whose names start with the prefix, named without it, over the same texture, such as one character of an atlas that packs many.
    [[nodiscard]] SpriteAtlas subset(std::string_view prefix) const;

    // Returns the image path stored in the document, relative to the document itself.
    [[nodiscard]] static std::string imagePath(const nlohmann::ordered_json& document);

    [[nodiscard]] const graphics::Texture& getTexture() const noexcept {
        return texture;
    }

    [[nodiscard]] bool hasFrame(std::string_view name) const;
    [[nodiscard]] const SpriteFrame& getFrame(std::string_view name) const;
    [[nodiscard]] const std::vector<std::string>& getFrameNames() const noexcept {
        return frameOrder;
    }

    [[nodiscard]] bool hasAnimation(std::string_view name) const;
    [[nodiscard]] const Animation& getAnimation(std::string_view name) const;
    [[nodiscard]] std::vector<std::string> getAnimationNames() const;

    [[nodiscard]] bool hasSlice(std::string_view name) const;
    [[nodiscard]] const graphics2d::NineSlice& getSlice(std::string_view name) const;
    [[nodiscard]] std::vector<std::string> getSliceNames() const;

    // Builds an animation that shows the named frames in order.
    [[nodiscard]] Animation animationFromFrames(const std::vector<std::string>& names, float framesPerSecond, Animation::Loop loop = Animation::Loop::Loop) const;

  private:
    using Document = nlohmann::ordered_json;

    [[nodiscard]] static math::Rect readRect(const Document& value);
    [[nodiscard]] static SpriteFrame readFrame(const Document& entry);
    [[nodiscard]] static Animation::Loop readTagLoop(const Document& tag);
    void readFrameSlice(const Document& slice);

    graphics::Texture texture;
    std::map<std::string, SpriteFrame, std::less<>> frames;
    std::vector<std::string> frameOrder;
    std::map<std::string, Animation, std::less<>> animations;
    std::map<std::string, graphics2d::NineSlice, std::less<>> slices;
};

} // namespace haylen::animation2d
