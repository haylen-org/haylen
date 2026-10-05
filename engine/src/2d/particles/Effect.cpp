#include "haylen/2d/particles/Effect.hpp"

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>

#include "haylen/2d/particles/ImageShape.hpp"
#include "haylen/graphics/BlendMode.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/io/Path.hpp"

namespace haylen::particles2d {

float Effect::readNumber(const core::Json& value, std::string_view key) {
    if (!value.is_number()) {
        throw std::invalid_argument("The particle effect value \"" + std::string(key) + "\" needs a number.");
    }
    return value.get<float>();
}

bool Effect::readBool(const core::Json& value, std::string_view key) {
    if (!value.is_boolean()) {
        throw std::invalid_argument("The particle effect value \"" + std::string(key) + "\" needs true or false.");
    }
    return value.get<bool>();
}

math::FloatRange Effect::readRange(const core::Json& value, std::string_view key) {
    if (value.is_number()) {
        return {value.get<float>(), value.get<float>()};
    }
    if (!value.is_array() || value.size() != 2 || !value[0].is_number() || !value[1].is_number()) {
        throw std::invalid_argument("The particle effect value \"" + std::string(key) + "\" needs a number or a [min, max] pair.");
    }
    return {value[0].get<float>(), value[1].get<float>()};
}

math::Vec2 Effect::readVec2(const core::Json& value, std::string_view key) {
    if (!value.is_array() || value.size() != 2 || !value[0].is_number() || !value[1].is_number()) {
        throw std::invalid_argument("The particle effect value \"" + std::string(key) + "\" needs an [x, y] pair.");
    }
    return {value[0].get<float>(), value[1].get<float>()};
}

math::Rect Effect::readRect(const core::Json& value, std::string_view key) {
    if (!value.is_array() || value.size() != 4 || !std::all_of(value.begin(), value.end(), [](const core::Json& item) { return item.is_number(); })) {
        throw std::invalid_argument("The particle effect value \"" + std::string(key) + "\" needs an [x, y, width, height] rectangle.");
    }
    return {value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>()};
}

std::size_t Effect::readCount(const core::Json& value, std::string_view name) {
    if (!value.is_number_integer() || value < 0) {
        throw std::invalid_argument("The particle effect value \"" + std::string(name) + "\" needs an integer of at least 0.");
    }
    return value.get<std::size_t>();
}

EmitterConfig::CountRange Effect::readCountRange(const core::Json& value, std::string_view key) {
    if (value.is_array() && value.size() == 2) {
        return {readCount(value[0], key), readCount(value[1], key)};
    }
    const std::size_t count = readCount(value, key);
    return {count, count};
}

math::Color Effect::readColor(const core::Json& value) {
    const std::string text = value.is_string() ? value.get<std::string>() : value.dump();
    const std::optional<math::Color> color = math::Color::parse(text);
    if (!color) {
        throw std::invalid_argument("The particle color \"" + text + "\" is not a \"#RRGGBB\" or \"#AARRGGBB\" color.");
    }
    return *color;
}

std::vector<math::Color> Effect::readColors(const core::Json& value, std::string_view key) {
    if (!value.is_array()) {
        throw std::invalid_argument("The particle effect value \"" + std::string(key) + "\" needs a list of colors.");
    }
    std::vector<math::Color> colors;
    for (const core::Json& color : value) {
        colors.push_back(readColor(color));
    }
    return colors;
}

const core::Json& Effect::requireObject(const core::Json& value, std::string_view key, std::span<const std::string_view> fields) {
    if (!value.is_object()) {
        throw std::invalid_argument("The particle effect value \"" + std::string(key) + "\" needs an object.");
    }
    for (const auto& [name, item] : value.items()) {
        if (std::find(fields.begin(), fields.end(), name) == fields.end()) {
            throw std::invalid_argument("The particle effect value \"" + std::string(key) + "\" has the unknown option \"" + name + "\".");
        }
    }
    return value;
}

template <typename Enum, std::size_t Size> Enum Effect::readName(const core::Json& value, std::string_view key, const std::array<std::pair<std::string_view, Enum>, Size>& names) {
    const std::string text = value.is_string() ? value.get<std::string>() : value.dump();
    if (const std::optional<Enum> found = EmitterConfig::fromName(names, text)) {
        return *found;
    }
    std::string choices;
    for (std::size_t index = 0; index < Size; ++index) {
        choices += (index == 0 ? "" : index + 1 == Size ? " or " : ", ") + std::string("\"") + std::string(names[index].first) + "\"";
    }
    throw std::invalid_argument("The particle effect value \"" + std::string(key) + "\" must be " + choices + ", not \"" + text + "\".");
}

// Curves take the forms the easing of tweens takes, without functions: a name, `{"curve", "overshoot"}`, `{"curve", "amplitude", "period"}`, `{"steps", "position"}`, `{"cubicBezier"}` or `{"points"}`.
math::EasingCurve Effect::readCurve(const core::Json& value, std::string_view key) {
    static constexpr std::array<std::string_view, 8> kCurveFields{"curve", "overshoot", "amplitude", "period", "steps", "position", "cubicBezier", "points"};
    if (value.is_string()) {
        const std::optional<math::Easing::Type> type = math::Easing::parse(value.get<std::string>());
        if (!type) {
            throw std::invalid_argument("The particle effect value \"" + std::string(key) + "\" names the unknown curve \"" + value.get<std::string>() + "\".");
        }
        return math::EasingCurve(*type);
    }
    const core::Json& curve = requireObject(value, key, kCurveFields);
    if (curve.contains("steps")) {
        const std::optional<math::Easing::StepPosition> position = math::Easing::parseStepPosition(curve.value("position", std::string("end")));
        if (!position) {
            throw std::invalid_argument("The step position of the particle effect value \"" + std::string(key) + "\" must be \"start\", \"end\", \"both\" or \"none\".");
        }
        return math::EasingCurve::steps(static_cast<int>(readCount(curve.at("steps"), "steps")), *position);
    }
    if (curve.contains("cubicBezier")) {
        const core::Json& handles = curve.at("cubicBezier");
        if (!handles.is_array() || handles.size() != 4) {
            throw std::invalid_argument("A \"cubicBezier\" curve needs the four numbers \"x1\", \"y1\", \"x2\" and \"y2\".");
        }
        return math::EasingCurve::cubicBezier(readNumber(handles[0], key), readNumber(handles[1], key), readNumber(handles[2], key), readNumber(handles[3], key));
    }
    if (curve.contains("points")) {
        const core::Json& list = curve.at("points");
        std::vector<math::Vec2> points;
        for (std::size_t index = 0; index < list.size(); ++index) {
            const float x = list.size() > 1 ? static_cast<float>(index) / static_cast<float>(list.size() - 1) : 0.0F;
            points.push_back(list[index].is_number() ? math::Vec2{x, list[index].get<float>()} : readVec2(list[index], key));
        }
        return math::EasingCurve::points(std::move(points));
    }
    const std::optional<math::Easing::Type> type = math::Easing::parse(curve.value("curve", std::string("linear")));
    if (!type) {
        throw std::invalid_argument("The particle effect value \"" + std::string(key) + "\" names an unknown curve.");
    }
    if (curve.contains("overshoot")) {
        return math::EasingCurve::back(*type, readNumber(curve.at("overshoot"), "overshoot"));
    }
    if (curve.contains("amplitude") || curve.contains("period")) {
        return math::EasingCurve::elastic(*type, readNumber(curve.value("amplitude", core::Json(math::Easing::kElasticAmplitude)), "amplitude"), readNumber(curve.value("period", core::Json(math::Easing::kElasticPeriod)), "period"));
    }
    return math::EasingCurve(*type);
}

bool Effect::isEmitterKey(std::string_view key) noexcept {
    static constexpr std::array<std::string_view, 70> kKeys{
        "texture", "filter", "wrap", "frames", "frameGrid", "frameMode", "frameRate", "rate", "rateOverDistance", "bursts", "delay", "duration", "loop", "prewarm", "maxParticles", "lifetime", "speed", "speedCurve", "direction", "directionMode", "spread", "inheritVelocity", "gravity", "radialAcceleration", "tangentialAcceleration", "damping", "turbulence", "attractors", "collision", "bounds", "boundsMode", "startSize", "endSize", "endSizeScale", "sizeCurve", "aspect", "stretch", "rotation", "rotationStep", "alignToVelocity", "spin", "spinCurve", "colors", "colorTimes", "colorBlend", "tints", "tintMode", "shape", "shapeSize", "shapeAngle", "shapeArc", "shapePoints", "shapeThickness", "shapeImage", "colorFromImage", "localSpace", "pixelSnap", "particleOrder", "subEmitters", "trail", "light", "particleLights", "layer", "depth", "sortOffset", "visibility", "blend", "emission", "unshaded", "lightMask",
    };
    return std::find(kKeys.begin(), kKeys.end(), key) != kKeys.end();
}

Effect Effect::parse(const core::Json& document, std::string_view path, const Reader& read) {
    Context context{.read = read, .reading = {std::string(path)}};
    Effect effect;
    if (!document.is_object()) {
        throw std::invalid_argument("The particle effect \"" + std::string(path) + "\" must be a JSON object.");
    }
    if (!document.contains("emitters")) {
        readEmitter(document, path, {}, effect.config, effect.files, context);
        if (effect.files.texture.empty()) {
            throw std::invalid_argument("The particle effect \"" + std::string(path) + "\" needs a texture.");
        }
        return effect;
    }

    if (document.size() != 1 || !document.at("emitters").is_array() || document.at("emitters").empty()) {
        throw std::invalid_argument("The composite particle effect \"" + std::string(path) + "\" holds only a list of emitters, with at least one.");
    }
    for (const core::Json& entry : document.at("emitters")) {
        effect.parts.push_back(readPart(entry, path, context));
    }
    return effect;
}

// A part is another effect file with its overrides, or the emitter options themselves, next to the keys that place it in the system.
Effect::Part Effect::readPart(const core::Json& entry, std::string_view path, Context& context) {
    if (!entry.is_object()) {
        throw std::invalid_argument("Every emitter of the composite particle effect \"" + std::string(path) + "\" must be an object.");
    }
    Part part;
    part.name = entry.value("name", std::string());
    if (entry.contains("offset")) {
        part.offset = readVec2(entry.at("offset"), "offset");
    }
    if (entry.contains("scale")) {
        part.scale = readNumber(entry.at("scale"), "scale");
    }
    if (entry.contains("effect")) {
        for (const auto& [key, value] : entry.items()) {
            if (std::find(kPartFields.begin(), kPartFields.end(), key) == kPartFields.end()) {
                throw std::invalid_argument("The emitter \"" + part.name + "\" of the composite particle effect \"" + std::string(path) + "\" names an effect file, so it takes no option \"" + key + "\" outside \"overrides\".");
            }
        }
        readReference(entry, path, part.config, part.files, context);
    } else {
        readEmitter(entry, path, kPartFields, part.config, part.files, context);
        if (part.files.texture.empty()) {
            throw std::invalid_argument("The emitter \"" + part.name + "\" of the composite particle effect \"" + std::string(path) + "\" needs a texture or an effect file.");
        }
    }
    if (entry.contains("delay")) {
        part.config.delay += readNumber(entry.at("delay"), "delay");
    }
    if (entry.contains("layerOffset")) {
        part.config.order.layer += static_cast<int>(readNumber(entry.at("layerOffset"), "layerOffset"));
    }
    return part;
}

void Effect::readReference(const core::Json& entry, std::string_view path, EmitterConfig& config, Files& files, Context& context) {
    const core::Json& reference = entry.at("effect");
    if (!reference.is_string()) {
        throw std::invalid_argument("The value \"effect\" of the particle effect \"" + std::string(path) + "\" needs the path of an effect file.");
    }
    const std::string target = io::Path::join(io::Path::directory(path), reference.get<std::string>());
    if (std::find(context.reading.begin(), context.reading.end(), target) != context.reading.end()) {
        throw std::invalid_argument("The particle effect \"" + std::string(path) + "\" refers to itself through \"" + target + "\".");
    }

    const std::vector<std::uint8_t> bytes = context.read(target);
    const core::Json document = core::Json::parse(bytes.begin(), bytes.end());
    if (document.contains("emitters")) {
        throw std::invalid_argument("The particle effect \"" + std::string(path) + "\" refers to the composite effect \"" + target + "\", where it needs an effect of one emitter.");
    }
    context.reading.push_back(target);
    readEmitter(document, target, {}, config, files, context);
    if (entry.contains("overrides")) {
        readEmitter(entry.at("overrides"), path, {}, config, files, context);
    }
    context.reading.pop_back();
    if (files.texture.empty()) {
        throw std::invalid_argument("The particle effect \"" + target + "\" needs a texture.");
    }
}

EmitterConfig::SubEmitter Effect::readSubEmitter(const core::Json& entry, std::string_view path, Files& files, Context& context) {
    const core::Json& object = requireObject(entry, "subEmitters", kSubEmitterFields);
    if (!object.contains("effect")) {
        throw std::invalid_argument("Every sub-emitter of the particle effect \"" + std::string(path) + "\" needs the path of an effect file in \"effect\".");
    }
    EmitterConfig::SubEmitter sub;
    sub.config = std::make_shared<EmitterConfig>();
    readReference(object, path, *sub.config, files, context);
    for (const auto& [key, value] : object.items()) {
        if (key == "trigger") {
            sub.trigger = readName(value, key, EmitterConfig::kTriggerNames);
        } else if (key == "count") {
            sub.count = readCountRange(value, key);
        } else if (key == "rate") {
            sub.rate = readNumber(value, key);
        } else if (key == "probability") {
            sub.probability = readNumber(value, key);
        } else if (key == "inheritVelocity") {
            sub.inheritVelocity = readNumber(value, key);
        } else if (key == "inheritColor") {
            sub.inheritColor = readBool(value, key);
        }
    }
    return sub;
}

// Reads every emitter key of the object onto the configuration, which keeps the values of the keys the object leaves out, so overrides apply on top of an effect.
void Effect::readEmitter(const core::Json& document, std::string_view path, std::span<const std::string_view> ignored, EmitterConfig& config, Files& files, Context& context) {
    if (!document.is_object()) {
        throw std::invalid_argument("The options of the particle effect \"" + std::string(path) + "\" must be a JSON object.");
    }
    for (const auto& [key, value] : document.items()) {
        if (std::find(ignored.begin(), ignored.end(), key) != ignored.end()) {
            continue;
        }
        if (!isEmitterKey(key)) {
            throw std::invalid_argument("The particle effect \"" + std::string(path) + "\" has the unknown option \"" + key + "\".");
        }
        readKey(key, value, path, config, files, context);
    }
}

void Effect::readKey(const std::string& key, const core::Json& value, std::string_view path, EmitterConfig& config, Files& files, Context& context) {
    static constexpr std::array<std::string_view, 6> kTrailFields{"length", "lifetime", "widthStart", "widthEnd", "colors", "texture"};
    if (key == "texture") {
        if (!value.is_string()) {
            throw std::invalid_argument("The particle effect value \"texture\" needs the path of an image.");
        }
        files.texture = io::Path::join(io::Path::directory(path), value.get<std::string>());
    } else if (key == "filter") {
        const std::string name = value.is_string() ? value.get<std::string>() : value.dump();
        if (!graphics::Texture::filterFromName(name)) {
            throw std::invalid_argument("The particle effect value \"filter\" must be \"nearest\" or \"linear\", not \"" + name + "\".");
        }
        files.textureOptions["filter"] = name;
    } else if (key == "wrap") {
        const std::string name = value.is_string() ? value.get<std::string>() : value.dump();
        if (!graphics::Texture::wrapFromName(name)) {
            throw std::invalid_argument("The particle effect value \"wrap\" must be \"clamp\", \"repeat\" or \"mirror\", not \"" + name + "\".");
        }
        files.textureOptions["wrap"] = name;
    } else if (key == "subEmitters") {
        if (!value.is_array()) {
            throw std::invalid_argument("The particle effect value \"subEmitters\" needs a list.");
        }
        config.subEmitters.clear();
        files.subEmitters.clear();
        for (const core::Json& entry : value) {
            files.subEmitters.emplace_back();
            config.subEmitters.push_back(readSubEmitter(entry, path, files.subEmitters.back(), context));
        }
    } else if (key == "trail") {
        const core::Json& trail = requireObject(value, key, kTrailFields);
        for (const auto& [name, item] : trail.items()) {
            if (name == "length") {
                config.trail.length = readCount(item, name);
            } else if (name == "lifetime") {
                config.trail.lifetime = readNumber(item, name);
            } else if (name == "widthStart") {
                config.trail.widthStart = readNumber(item, name);
            } else if (name == "widthEnd") {
                config.trail.widthEnd = readNumber(item, name);
            } else if (name == "colors") {
                config.trail.colors = readColors(item, name);
            } else if (item.is_string()) {
                files.trailTexture = io::Path::join(io::Path::directory(path), item.get<std::string>());
            } else {
                throw std::invalid_argument("The particle effect value \"texture\" of a trail needs the path of an image.");
            }
        }
    } else if (key.starts_with("shape") || key == "colorFromImage") {
        readShape(key, value, path, config, context);
    } else if (key == "layer" || key == "depth" || key == "sortOffset" || key == "visibility" || key == "blend" || key == "emission" || key == "unshaded" || key == "lightMask") {
        readOrder(key, value, config);
    } else {
        readMotion(key, value, config);
    }
}

void Effect::readMotion(const std::string& key, const core::Json& value, EmitterConfig& config) {
    static constexpr std::array<std::string_view, 5> kBurstFields{"time", "count", "cycles", "interval", "probability"};
    static constexpr std::array<std::string_view, 3> kTurbulenceFields{"strength", "frequency", "speed"};
    static constexpr std::array<std::string_view, 6> kAttractorFields{"x", "y", "strength", "radius", "killRadius", "space"};
    static constexpr std::array<std::string_view, 7> kCollisionFields{"type", "y", "area", "bounce", "friction", "result", "lifeLoss"};
    if (key == "rate") {
        config.rate = readNumber(value, key);
    } else if (key == "rateOverDistance") {
        config.rateOverDistance = readNumber(value, key);
    } else if (key == "bursts") {
        if (!value.is_array()) {
            throw std::invalid_argument("The particle effect value \"bursts\" needs a list.");
        }
        config.bursts.clear();
        for (const core::Json& entry : value) {
            const core::Json& burst = requireObject(entry, key, kBurstFields);
            EmitterConfig::Burst planned;
            planned.time = readNumber(burst.value("time", core::Json(0.0)), "time");
            planned.count = readCountRange(burst.value("count", core::Json(0)), "count");
            planned.cycles = static_cast<int>(std::min<std::size_t>(readCount(burst.value("cycles", core::Json(1)), "cycles"), std::numeric_limits<int>::max()));
            planned.interval = readNumber(burst.value("interval", core::Json(0.0)), "interval");
            planned.probability = readNumber(burst.value("probability", core::Json(1.0)), "probability");
            config.bursts.push_back(planned);
        }
    } else if (key == "delay") {
        config.delay = readNumber(value, key);
    } else if (key == "duration") {
        config.duration = readNumber(value, key);
    } else if (key == "loop") {
        config.loop = readBool(value, key);
    } else if (key == "prewarm") {
        config.prewarm = readNumber(value, key);
    } else if (key == "maxParticles") {
        config.maxParticles = readCount(value, key);
    } else if (key == "lifetime") {
        config.lifetime = readRange(value, key);
    } else if (key == "speed") {
        config.speed = readRange(value, key);
    } else if (key == "speedCurve") {
        config.speedCurve = readCurve(value, key);
    } else if (key == "direction") {
        config.direction = readNumber(value, key);
    } else if (key == "directionMode") {
        config.directionMode = readName(value, key, EmitterConfig::kDirectionModeNames);
    } else if (key == "spread") {
        config.spread = readNumber(value, key);
    } else if (key == "inheritVelocity") {
        config.inheritVelocity = readNumber(value, key);
    } else if (key == "gravity") {
        config.gravity = readVec2(value, key);
    } else if (key == "radialAcceleration") {
        config.radialAcceleration = readRange(value, key);
    } else if (key == "tangentialAcceleration") {
        config.tangentialAcceleration = readRange(value, key);
    } else if (key == "damping") {
        config.damping = readNumber(value, key);
    } else if (key == "turbulence") {
        const core::Json& turbulence = requireObject(value, key, kTurbulenceFields);
        config.turbulence.strength = readNumber(turbulence.value("strength", core::Json(0.0)), "strength");
        config.turbulence.frequency = readNumber(turbulence.value("frequency", core::Json(0.01)), "frequency");
        config.turbulence.speed = readNumber(turbulence.value("speed", core::Json(0.5)), "speed");
    } else if (key == "attractors") {
        if (!value.is_array()) {
            throw std::invalid_argument("The particle effect value \"attractors\" needs a list.");
        }
        config.attractors.clear();
        for (const core::Json& entry : value) {
            const core::Json& attractor = requireObject(entry, key, kAttractorFields);
            EmitterConfig::Attractor made;
            made.position = {readNumber(attractor.value("x", core::Json(0.0)), "x"), readNumber(attractor.value("y", core::Json(0.0)), "y")};
            made.strength = readNumber(attractor.value("strength", core::Json(0.0)), "strength");
            made.radius = readNumber(attractor.value("radius", core::Json(100.0)), "radius");
            made.killRadius = readNumber(attractor.value("killRadius", core::Json(0.0)), "killRadius");
            if (attractor.contains("space")) {
                made.space = readName(attractor.at("space"), "space", EmitterConfig::kSpaceNames);
            }
            config.attractors.push_back(made);
        }
    } else if (key == "collision") {
        const core::Json& collision = requireObject(value, key, kCollisionFields);
        EmitterConfig::Collision& made = config.collision;
        for (const auto& [name, item] : collision.items()) {
            if (name == "type") {
                made.type = readName(item, name, EmitterConfig::kCollisionTypeNames);
            } else if (name == "y") {
                made.y = readNumber(item, name);
            } else if (name == "area") {
                made.area = readRect(item, name);
            } else if (name == "bounce") {
                made.bounce = readNumber(item, name);
            } else if (name == "friction") {
                made.friction = readNumber(item, name);
            } else if (name == "result") {
                made.result = readName(item, name, EmitterConfig::kCollisionResultNames);
            } else {
                made.lifeLoss = readNumber(item, name);
            }
        }
    } else if (key == "bounds") {
        config.bounds = readRect(value, key);
    } else if (key == "boundsMode") {
        config.boundsMode = readName(value, key, EmitterConfig::kBoundsModeNames);
    } else if (key == "localSpace") {
        config.localSpace = readBool(value, key);
    } else if (key == "pixelSnap") {
        config.pixelSnap = readNumber(value, key);
    } else if (key == "particleOrder") {
        config.particleOrder = readName(value, key, EmitterConfig::kParticleOrderNames);
    } else {
        readLook(key, value, config);
    }
}

void Effect::readLook(const std::string& key, const core::Json& value, EmitterConfig& config) {
    static constexpr std::array<std::string_view, 3> kGridFields{"columns", "rows", "count"};
    static constexpr std::array<std::string_view, 7> kLightFields{"type", "offset", "radius", "color", "intensity", "flicker", "fade"};
    static constexpr std::array<std::string_view, 2> kFlickerFields{"speed", "amount"};
    static constexpr std::array<std::string_view, 3> kParticleLightFields{"radius", "intensity", "max"};
    if (key == "frames") {
        if (!value.is_array()) {
            throw std::invalid_argument("The particle effect value \"frames\" needs a list of rectangles.");
        }
        config.frames.clear();
        for (const core::Json& frame : value) {
            config.frames.push_back(readRect(frame, key));
        }
    } else if (key == "frameGrid") {
        const core::Json& grid = requireObject(value, key, kGridFields);
        config.frameGrid.columns = static_cast<int>(readCount(grid.value("columns", core::Json(1)), "columns"));
        config.frameGrid.rows = static_cast<int>(readCount(grid.value("rows", core::Json(1)), "rows"));
        config.frameGrid.count = static_cast<int>(readCount(grid.value("count", core::Json(0)), "count"));
    } else if (key == "frameMode") {
        config.frameMode = readName(value, key, EmitterConfig::kFrameModeNames);
    } else if (key == "frameRate") {
        config.frameRate = readNumber(value, key);
    } else if (key == "startSize") {
        config.startSize = readRange(value, key);
    } else if (key == "endSize") {
        config.endSize = readRange(value, key);
    } else if (key == "endSizeScale") {
        config.endSizeScale = readRange(value, key);
    } else if (key == "sizeCurve") {
        config.sizeCurve = readCurve(value, key);
    } else if (key == "aspect") {
        config.aspect = readRange(value, key);
    } else if (key == "stretch") {
        config.stretch = readNumber(value, key);
    } else if (key == "rotation") {
        config.rotation = readRange(value, key);
    } else if (key == "rotationStep") {
        config.rotationStep = readNumber(value, key);
    } else if (key == "alignToVelocity") {
        config.alignToVelocity = readBool(value, key);
    } else if (key == "spin") {
        config.spin = readRange(value, key);
    } else if (key == "spinCurve") {
        config.spinCurve = readCurve(value, key);
    } else if (key == "colors") {
        config.colors = readColors(value, key);
    } else if (key == "colorTimes") {
        if (!value.is_array()) {
            throw std::invalid_argument("The particle effect value \"colorTimes\" needs a list of numbers.");
        }
        config.colorTimes.clear();
        for (const core::Json& item : value) {
            config.colorTimes.push_back(readNumber(item, key));
        }
    } else if (key == "colorBlend") {
        config.colorBlend = readName(value, key, EmitterConfig::kColorBlendNames);
    } else if (key == "tints") {
        config.tints = readColors(value, key);
    } else if (key == "tintMode") {
        config.tintMode = readName(value, key, EmitterConfig::kTintModeNames);
    } else if (key == "light") {
        const core::Json& light = requireObject(value, key, kLightFields);
        EmitterConfig::Light made;
        for (const auto& [name, item] : light.items()) {
            if (name == "type") {
                const std::optional<lighting2d::Light::Type> type = lighting2d::Light::typeFromName(item.is_string() ? item.get<std::string>() : std::string());
                if (!type) {
                    throw std::invalid_argument("The particle effect value \"type\" of a light must be \"point\", \"spot\" or \"directional\".");
                }
                made.type = *type;
            } else if (name == "offset") {
                made.offset = readVec2(item, name);
            } else if (name == "radius") {
                made.radius = readNumber(item, name);
            } else if (name == "color") {
                made.color = readColor(item);
            } else if (name == "intensity") {
                made.intensity = readNumber(item, name);
            } else if (name == "flicker") {
                const core::Json& flicker = requireObject(item, name, kFlickerFields);
                made.flicker.speed = readNumber(flicker.value("speed", core::Json(0.0)), "speed");
                made.flicker.amount = readNumber(flicker.value("amount", core::Json(0.0)), "amount");
            } else {
                made.fade = readName(item, name, EmitterConfig::kFadeNames);
            }
        }
        config.light = made;
    } else if (key == "particleLights") {
        const core::Json& lights = requireObject(value, key, kParticleLightFields);
        EmitterConfig::ParticleLights made;
        made.radius = readNumber(lights.value("radius", core::Json(made.radius)), "radius");
        made.intensity = readNumber(lights.value("intensity", core::Json(made.intensity)), "intensity");
        made.max = readCount(lights.value("max", core::Json(made.max)), "max");
        config.particleLights = made;
    }
}

void Effect::readShape(const std::string& key, const core::Json& value, std::string_view path, EmitterConfig& config, Context& context) {
    static constexpr std::array<std::string_view, 3> kImageFields{"path", "source", "alphaThreshold"};
    if (key == "shape") {
        config.shape = readName(value, key, EmitterConfig::kShapeNames);
    } else if (key == "shapeSize") {
        config.shapeSize = readVec2(value, key);
    } else if (key == "shapeAngle") {
        config.shapeAngle = readNumber(value, key);
    } else if (key == "shapeArc") {
        config.shapeArc = readRange(value, key);
    } else if (key == "shapeThickness") {
        config.shapeThickness = readNumber(value, key);
    } else if (key == "shapePoints") {
        if (!value.is_array()) {
            throw std::invalid_argument("The particle effect value \"shapePoints\" needs a list of [x, y] points.");
        }
        config.shapePoints.clear();
        for (const core::Json& point : value) {
            config.shapePoints.push_back(readVec2(point, key));
        }
    } else if (key == "colorFromImage") {
        config.colorFromImage = readBool(value, key);
    } else {
        // An image shape reads its image once, while the effect loads.
        const core::Json described = value.is_string() ? core::Json{{"path", value}} : value;
        const core::Json& image = requireObject(described, key, kImageFields);
        if (!image.contains("path") || !image.at("path").is_string()) {
            throw std::invalid_argument("The particle effect value \"shapeImage\" needs the path of an image.");
        }
        ImageShape::Options options;
        if (image.contains("source")) {
            options.source = readRect(image.at("source"), "source");
        }
        options.alphaThreshold = readNumber(image.value("alphaThreshold", core::Json(options.alphaThreshold)), "alphaThreshold");
        const std::vector<std::uint8_t> bytes = context.read(io::Path::join(io::Path::directory(path), image.at("path").get<std::string>()));
        config.shapeImage = std::make_shared<ImageShape>(graphics::Image::decode(bytes), options);
    }
}

void Effect::readOrder(const std::string& key, const core::Json& value, EmitterConfig& config) {
    graphics2d::DrawOrder& order = config.order;
    if (key == "layer") {
        order.layer = static_cast<int>(readNumber(value, key));
    } else if (key == "depth") {
        order.depth = readNumber(value, key);
    } else if (key == "sortOffset") {
        order.sortOffset = readNumber(value, key);
    } else if (key == "visibility") {
        order.visibility = static_cast<std::uint32_t>(readCount(value, key));
    } else if (key == "emission") {
        order.emission = readNumber(value, key);
    } else if (key == "unshaded") {
        order.unshaded = readBool(value, key);
    } else if (key == "lightMask") {
        const std::size_t mask = readCount(value, key);
        if (mask > 255) {
            throw std::invalid_argument("The particle effect value \"lightMask\" needs an integer from 0 to 255.");
        }
        order.lightMask = static_cast<std::uint8_t>(mask);
    } else {
        const std::string name = value.is_string() ? value.get<std::string>() : value.dump();
        const std::optional<graphics::BlendMode::Type> blend = graphics::BlendMode::parse(name);
        if (!blend) {
            throw std::invalid_argument("The particle effect value \"blend\" must be \"alpha\", \"additive\", \"multiply\", \"screen\", \"premultiplied\" or \"opaque\", not \"" + name + "\".");
        }
        order.blend = *blend;
    }
}

void Effect::applyTextures(EmitterConfig& config, const Files& files, const TextureMaker& make) {
    config.texture = make(files.texture, files.textureOptions);
    if (!files.trailTexture.empty()) {
        config.trail.texture = make(files.trailTexture, files.textureOptions);
    }
    for (std::size_t index = 0; index < files.subEmitters.size(); ++index) {
        applyTextures(*config.subEmitters[index].config, files.subEmitters[index], make);
    }
}

void Effect::applyTextures(const TextureMaker& make) {
    if (!isComposite()) {
        applyTextures(config, files, make);
    }
    for (Part& part : parts) {
        applyTextures(part.config, part.files, make);
    }
}

void Effect::collectTextures(const Files& source, std::vector<std::string>& paths) {
    for (const std::string& path : {source.texture, source.trailTexture}) {
        if (!path.empty() && std::find(paths.begin(), paths.end(), path) == paths.end()) {
            paths.push_back(path);
        }
    }
    for (const Files& sub : source.subEmitters) {
        collectTextures(sub, paths);
    }
}

std::vector<std::string> Effect::getTexturePaths() const {
    std::vector<std::string> paths;
    if (!isComposite()) {
        collectTextures(files, paths);
    }
    for (const Part& part : parts) {
        collectTextures(part.files, paths);
    }
    return paths;
}

} // namespace haylen::particles2d
