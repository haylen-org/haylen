#include "haylen/2d/particles/Effect.hpp"

#include <optional>
#include <set>
#include <stdexcept>

#include "haylen/graphics/BlendMode.hpp"
#include "haylen/io/Path.hpp"

namespace haylen::particles2d {

math::FloatRange Effect::readRange(const core::Json& value) {
    if (value.is_number()) {
        return {value.get<float>(), value.get<float>()};
    }
    return {value.at(0).get<float>(), value.at(1).get<float>()};
}

math::Vec2 Effect::readVec2(const core::Json& value) {
    return {value.at(0).get<float>(), value.at(1).get<float>()};
}

std::size_t Effect::readCount(const core::Json& value, std::string_view name) {
    if (!value.is_number_integer() || value < 0) {
        throw std::invalid_argument("The particle effect value '" + std::string(name) + "' needs an integer of at least 0.");
    }
    return value.get<std::size_t>();
}

math::Color Effect::readColor(const core::Json& value) {
    const std::string text = value.get<std::string>();
    const std::optional<math::Color> color = math::Color::parse(text);
    if (!color) {
        throw std::invalid_argument("The particle color '" + text + "' is not a #RRGGBB or #AARRGGBB color.");
    }
    return *color;
}

Effect Effect::parse(const core::Json& document, std::string_view path) {
    static const std::set<std::string, std::less<>>& known = *new const std::set<std::string, std::less<>>{"texture", "frames", "rate", "bursts", "duration", "loop", "prewarm", "maxParticles", "lifetime", "speed", "direction", "spread", "gravity", "radialAcceleration", "tangentialAcceleration", "damping", "startSize", "endSize", "spin", "colors", "shape", "shapeSize", "localSpace", "layer", "depth", "blend"};
    for (const auto& [key, value] : document.items()) {
        if (!known.contains(key)) {
            throw std::invalid_argument("The particle effect '" + std::string(path) + "' has the unknown option '" + key + "'.");
        }
    }

    Effect effect{.texturePath = io::Path::join(io::Path::directory(path), document.at("texture").get<std::string>())};
    EmitterConfig& settings = effect.config;
    for (const core::Json& frame : document.value("frames", core::Json::array())) {
        settings.frames.push_back({frame.at(0).get<float>(), frame.at(1).get<float>(), frame.at(2).get<float>(), frame.at(3).get<float>()});
    }
    for (const core::Json& burst : document.value("bursts", core::Json::array())) {
        settings.bursts.push_back({.time = burst.at("time").get<float>(), .count = readCount(burst.at("count"), "count")});
    }
    settings.rate = document.value("rate", settings.rate);
    settings.duration = document.value("duration", settings.duration);
    settings.loop = document.value("loop", settings.loop);
    settings.prewarm = document.value("prewarm", settings.prewarm);
    if (document.contains("maxParticles")) {
        settings.maxParticles = readCount(document.at("maxParticles"), "maxParticles");
    }
    settings.direction = document.value("direction", settings.direction);
    settings.spread = document.value("spread", settings.spread);
    settings.damping = document.value("damping", settings.damping);
    settings.localSpace = document.value("localSpace", settings.localSpace);

    // clang-format off
    const auto range = [&document](const char* key, math::FloatRange& target) {
        if (document.contains(key)) {
            target = readRange(document.at(key));
        }
    };
    // clang-format on
    range("lifetime", settings.lifetime);
    range("speed", settings.speed);
    range("radialAcceleration", settings.radialAcceleration);
    range("tangentialAcceleration", settings.tangentialAcceleration);
    range("startSize", settings.startSize);
    range("endSize", settings.endSize);
    range("spin", settings.spin);
    if (document.contains("gravity")) {
        settings.gravity = readVec2(document.at("gravity"));
    }
    if (document.contains("shapeSize")) {
        settings.shapeSize = readVec2(document.at("shapeSize"));
    }
    if (document.contains("colors")) {
        settings.colors.clear();
        for (const core::Json& color : document.at("colors")) {
            settings.colors.push_back(readColor(color));
        }
    }

    if (document.contains("shape")) {
        const std::string name = document.at("shape").get<std::string>();
        const std::optional<EmitterConfig::Shape> shape = EmitterConfig::shapeFromName(name);
        if (!shape) {
            throw std::invalid_argument("The emitter shape of a particle effect must be point, circle, ring, rectangle or cone, not '" + name + "'.");
        }
        settings.shape = *shape;
    }
    if (document.contains("blend")) {
        const std::string name = document.at("blend").get<std::string>();
        const std::optional<graphics::BlendMode::Type> blend = graphics::BlendMode::parse(name);
        if (!blend) {
            throw std::invalid_argument("The blend mode of a particle effect must be alpha, additive, multiply, screen, premultiplied or opaque, not '" + name + "'.");
        }
        settings.order.blend = *blend;
    }
    settings.order.layer = document.value("layer", settings.order.layer);
    settings.order.depth = document.value("depth", settings.order.depth);
    return effect;
}

} // namespace haylen::particles2d
