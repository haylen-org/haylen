#include "haylen/2d/animation/SpriteAtlas.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "haylen/math/Insets.hpp"

namespace haylen::animation2d {

math::Rect SpriteAtlas::readRect(const Document& value) {
    return {value.at("x").get<float>(), value.at("y").get<float>(), value.at("w").get<float>(), value.at("h").get<float>()};
}

SpriteFrame SpriteAtlas::readFrame(const Document& entry) {
    if (entry.value("rotated", false)) {
        throw std::invalid_argument("Rotated atlas frames are not supported. Disable rotation in the packer.");
    }

    SpriteFrame frame;
    frame.source = readRect(entry.at("frame"));
    frame.originalSize = frame.source.getSize();
    if (entry.contains("spriteSourceSize") && entry.contains("sourceSize")) {
        frame.offset = readRect(entry.at("spriteSourceSize")).getMin();
        frame.originalSize = {entry.at("sourceSize").at("w").get<float>(), entry.at("sourceSize").at("h").get<float>()};
    }
    if (entry.contains("duration")) {
        frame.duration = entry.at("duration").get<float>() / 1000.0F;
    }
    return frame;
}

Animation::Loop SpriteAtlas::readTagLoop(const Document& tag) {
    const std::string direction = tag.value("direction", std::string("forward"));
    if (direction != "forward" && direction != "reverse" && direction != "pingpong" && direction != "pingpong_reverse") {
        throw std::invalid_argument("Unknown Aseprite tag direction: " + direction);
    }

    // Aseprite stores the repeat count as text, and an absent or zero count means forever.
    const int repeats = tag.contains("repeat") ? std::stoi(tag.at("repeat").get<std::string>()) : 0;
    if (repeats > 1) {
        throw std::invalid_argument("Aseprite tags that repeat a fixed number of times above one are not supported: " + tag.at("name").get<std::string>());
    }
    if (repeats == 1) {
        return Animation::Loop::Once;
    }
    return direction.starts_with("pingpong") ? Animation::Loop::PingPong : Animation::Loop::Loop;
}

SpriteAtlas SpriteAtlas::parse(const Document& document, graphics::Texture image) {
    if (!image.isValid()) {
        throw std::invalid_argument("A sprite atlas needs its texture.");
    }

    SpriteAtlas atlas;
    atlas.texture = std::move(image);

    const Document& frameList = document.at("frames");
    if (frameList.is_object()) {
        for (const auto& [name, entry] : frameList.items()) {
            atlas.frames.emplace(name, readFrame(entry));
            atlas.frameOrder.push_back(name);
        }
    } else {
        for (const Document& entry : frameList) {
            const std::string name = entry.at("filename").get<std::string>();
            atlas.frames.emplace(name, readFrame(entry));
            atlas.frameOrder.push_back(name);
        }
    }

    const Document meta = document.value("meta", Document::object());
    for (const Document& tag : meta.value("frameTags", Document::array())) {
        const auto from = tag.at("from").get<std::size_t>();
        const auto to = tag.at("to").get<std::size_t>();
        if (from > to || to >= atlas.frameOrder.size()) {
            throw std::out_of_range("An Aseprite tag refers to frames that do not exist.");
        }

        Animation animation{.texture = atlas.texture, .loop = readTagLoop(tag)};
        for (std::size_t index = from; index <= to; ++index) {
            animation.frames.push_back(atlas.frames.at(atlas.frameOrder[index]));
        }
        const std::string direction = tag.value("direction", std::string("forward"));
        if (direction == "reverse" || direction == "pingpong_reverse") {
            std::reverse(animation.frames.begin(), animation.frames.end());
        }
        atlas.animations.insert_or_assign(tag.at("name").get<std::string>(), std::move(animation));
    }

    for (const Document& slice : meta.value("slices", Document::array())) {
        const Document& key = slice.at("keys").at(0);
        if (!key.contains("center")) {
            continue;
        }

        // The bounds of a slice key are relative to the frame the key names, which sits somewhere in the packed texture.
        const math::Rect frame = atlas.frames.at(atlas.frameOrder.at(key.value("frame", std::size_t{0}))).source;
        const math::Rect bounds = readRect(key.at("bounds"));
        const math::Rect center = readRect(key.at("center"));
        const math::Rect source{frame.x + bounds.x, frame.y + bounds.y, bounds.width, bounds.height};
        const math::Insets borders{.left = center.x, .top = center.y, .right = bounds.width - center.getRight(), .bottom = bounds.height - center.getBottom()};
        atlas.slices.insert_or_assign(slice.at("name").get<std::string>(), graphics2d::NineSlice::fromBorders(atlas.texture, source, borders));
    }
    return atlas;
}

std::string SpriteAtlas::imagePath(const Document& document) {
    return document.at("meta").at("image").get<std::string>();
}

bool SpriteAtlas::hasFrame(std::string_view name) const {
    return frames.contains(name);
}

const SpriteFrame& SpriteAtlas::getFrame(std::string_view name) const {
    const auto found = frames.find(name);
    if (found == frames.end()) {
        throw std::invalid_argument("Unknown atlas frame: " + std::string(name));
    }
    return found->second;
}

bool SpriteAtlas::hasAnimation(std::string_view name) const {
    return animations.contains(name);
}

const Animation& SpriteAtlas::getAnimation(std::string_view name) const {
    const auto found = animations.find(name);
    if (found == animations.end()) {
        throw std::invalid_argument("Unknown atlas animation: " + std::string(name));
    }
    return found->second;
}

std::vector<std::string> SpriteAtlas::getAnimationNames() const {
    std::vector<std::string> names;
    for (const auto& [name, animation] : animations) {
        names.push_back(name);
    }
    return names;
}

bool SpriteAtlas::hasSlice(std::string_view name) const {
    return slices.contains(name);
}

const graphics2d::NineSlice& SpriteAtlas::getSlice(std::string_view name) const {
    const auto found = slices.find(name);
    if (found == slices.end()) {
        throw std::invalid_argument("Unknown atlas slice: " + std::string(name));
    }
    return found->second;
}

std::vector<std::string> SpriteAtlas::getSliceNames() const {
    std::vector<std::string> names;
    for (const auto& [name, slice] : slices) {
        names.push_back(name);
    }
    return names;
}

Animation SpriteAtlas::animationFromFrames(const std::vector<std::string>& names, float framesPerSecond, Animation::Loop loop) const {
    if (names.empty() || framesPerSecond <= 0.0F) {
        throw std::invalid_argument("An atlas animation needs frames and a positive frame rate.");
    }

    Animation animation{.texture = texture, .loop = loop};
    for (const std::string& name : names) {
        SpriteFrame copy = getFrame(name);
        copy.duration = 1.0F / framesPerSecond;
        animation.frames.push_back(copy);
    }
    return animation;
}

} // namespace haylen::animation2d
