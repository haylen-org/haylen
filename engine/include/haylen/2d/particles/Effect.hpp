#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/2d/particles/EmitterConfig.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/EasingCurve.hpp"
#include "haylen/math/FloatRange.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::particles2d {

// A particle effect file holds the options of `particles2d.newEmitter` as JSON, with the files it names relative to the effect file, or a composite of several emitters under `emitters`, each its own options or another effect file. Colors use `#AARRGGBB` and ranges take a number or `[min, max]`.
struct Effect {
    // The files an emitter of the effect names, which the loader turns into textures, in the shape of its sub-emitters.
    struct Files {
        std::string texture;
        std::string trailTexture;

        // The `filter` and `wrap` the file gives its textures.
        core::Json textureOptions = core::Json::object();
        std::vector<Files> subEmitters;
    };

    // One emitter of a composite effect, placed at its offset from the system and scaled with it.
    struct Part {
        std::string name;
        math::Vec2 offset{};
        float scale = 1.0F;
        EmitterConfig config;
        Files files;
    };

    // Reads a file of the package by its path, for the effect files and images that a file names.
    using Reader = std::function<std::vector<std::uint8_t>(const std::string& path)>;

    // Hands out the texture of a path with the options the file gives it.
    using TextureMaker = std::function<graphics::Texture(const std::string& path, const core::Json& options)>;

    EmitterConfig config;
    Files files;
    std::vector<Part> parts;

    [[nodiscard]] bool isComposite() const noexcept {
        return !parts.empty();
    }

    [[nodiscard]] static Effect parse(const core::Json& document, std::string_view path, const Reader& read);

    // Gives every emitter of the effect and of its sub-emitters the textures of the files they name.
    void applyTextures(const TextureMaker& make);

    // Lists the texture paths of the effect once each, for the loader to decode.
    [[nodiscard]] std::vector<std::string> getTexturePaths() const;

  private:
    // What reading a file needs: the reader and the files being read, which tells an effect that refers to itself apart.
    struct Context {
        const Reader& read;
        std::vector<std::string> reading;
    };

    static constexpr std::array<std::string_view, 7> kPartFields{"name", "effect", "offset", "scale", "delay", "layerOffset", "overrides"};
    static constexpr std::array<std::string_view, 8> kSubEmitterFields{"effect", "trigger", "count", "rate", "probability", "inheritVelocity", "inheritColor", "overrides"};

    static void readEmitter(const core::Json& document, std::string_view path, std::span<const std::string_view> ignored, EmitterConfig& config, Files& files, Context& context);
    static void readKey(const std::string& key, const core::Json& value, std::string_view path, EmitterConfig& config, Files& files, Context& context);
    static void readMotion(const std::string& key, const core::Json& value, EmitterConfig& config);
    static void readLook(const std::string& key, const core::Json& value, EmitterConfig& config);
    static void readShape(const std::string& key, const core::Json& value, std::string_view path, EmitterConfig& config, Context& context);
    static void readOrder(const std::string& key, const core::Json& value, EmitterConfig& config);
    [[nodiscard]] static bool isEmitterKey(std::string_view key) noexcept;

    // Reads a referenced effect file, applies the overrides on top of it and checks that the result has a texture.
    static void readReference(const core::Json& entry, std::string_view path, EmitterConfig& config, Files& files, Context& context);
    static EmitterConfig::SubEmitter readSubEmitter(const core::Json& entry, std::string_view path, Files& files, Context& context);
    static Part readPart(const core::Json& entry, std::string_view path, Context& context);

    [[nodiscard]] static float readNumber(const core::Json& value, std::string_view key);
    [[nodiscard]] static bool readBool(const core::Json& value, std::string_view key);
    [[nodiscard]] static math::FloatRange readRange(const core::Json& value, std::string_view key);
    [[nodiscard]] static math::Vec2 readVec2(const core::Json& value, std::string_view key);
    [[nodiscard]] static math::Rect readRect(const core::Json& value, std::string_view key);
    [[nodiscard]] static std::size_t readCount(const core::Json& value, std::string_view name);
    [[nodiscard]] static EmitterConfig::CountRange readCountRange(const core::Json& value, std::string_view key);
    [[nodiscard]] static math::Color readColor(const core::Json& value);
    [[nodiscard]] static std::vector<math::Color> readColors(const core::Json& value, std::string_view key);
    [[nodiscard]] static math::EasingCurve readCurve(const core::Json& value, std::string_view key);
    [[nodiscard]] static const core::Json& requireObject(const core::Json& value, std::string_view key, std::span<const std::string_view> fields);

    template <typename Enum, std::size_t Size> [[nodiscard]] static Enum readName(const core::Json& value, std::string_view key, const std::array<std::pair<std::string_view, Enum>, Size>& names);

    static void applyTextures(EmitterConfig& config, const Files& files, const TextureMaker& make);
    static void collectTextures(const Files& files, std::vector<std::string>& paths);
};

} // namespace haylen::particles2d
