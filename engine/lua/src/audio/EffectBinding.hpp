#pragma once

#include <lua.hpp>

#include <array>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/audio/Delay.hpp"
#include "haylen/audio/Filter.hpp"
#include "haylen/audio/Limiter.hpp"
#include "haylen/audio/Reverb.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/lua/Type.hpp"

namespace haylen::lua {

template <> struct Type<haylen::audio::Filter> {
    static constexpr const char* name = "haylen.Filter";
    using Storage = std::shared_ptr<haylen::audio::Filter>;
};

template <> struct Type<haylen::audio::Delay> {
    static constexpr const char* name = "haylen.Delay";
    using Storage = std::shared_ptr<haylen::audio::Delay>;
};

template <> struct Type<haylen::audio::Reverb> {
    static constexpr const char* name = "haylen.Reverb";
    using Storage = std::shared_ptr<haylen::audio::Reverb>;
};

template <> struct Type<haylen::audio::Limiter> {
    static constexpr const char* name = "haylen.Limiter";
    using Storage = std::shared_ptr<haylen::audio::Limiter>;
};

} // namespace haylen::lua

namespace haylen::core {

template <> struct EnumNames<haylen::audio::Filter::Kind> {
    static constexpr std::array<std::pair<std::string_view, haylen::audio::Filter::Kind>, 7> kNames{{{"lowpass", haylen::audio::Filter::Kind::Lowpass}, {"highpass", haylen::audio::Filter::Kind::Highpass}, {"bandpass", haylen::audio::Filter::Kind::Bandpass}, {"notch", haylen::audio::Filter::Kind::Notch}, {"peak", haylen::audio::Filter::Kind::Peak}, {"lowShelf", haylen::audio::Filter::Kind::LowShelf}, {"highShelf", haylen::audio::Filter::Kind::HighShelf}}};

    static std::optional<haylen::audio::Filter::Kind> fromName(std::string_view name) {
        for (const auto& [candidate, kind] : kNames) {
            if (candidate == name) {
                return kind;
            }
        }

        return std::nullopt;
    }
    static std::string_view name(haylen::audio::Filter::Kind value) {
        for (const auto& [candidate, kind] : kNames) {
            if (kind == value) {
                return candidate;
            }
        }

        return kNames.front().first;
    }
};

} // namespace haylen::core

namespace haylen::lua::audio {

// Installs the `Filter`, `Delay`, `Reverb` and `Limiter` classes of `haylen.audio`. Their parameters are properties that tweens animate natively.
class EffectBinding final {
  public:
    static void install(lua_State* L);

    // Creates an effect with `newEffect(kind, options)`, where the kind names a filter, `'delay'`, `'reverb'` or `'limiter'`.
    static int newEffect(lua_State* L);

    // Returns the effect of any effect class at the index, or null for another value.
    [[nodiscard]] static std::shared_ptr<haylen::audio::Effect> test(lua_State* L, int index);

    // Returns the effect of any effect class at the index, or raises an argument error.
    [[nodiscard]] static std::shared_ptr<haylen::audio::Effect> check(lua_State* L, int index);
    static void push(lua_State* L, const std::shared_ptr<haylen::audio::Effect>& effect);

  private:
    static constexpr std::array<std::string_view, 2> kFilterFields{"cutoff", "q"};
    static constexpr std::array<std::string_view, 3> kGainFilterFields{"cutoff", "q", "gain"};
    static constexpr std::array<std::string_view, 5> kDelayFields{"time", "maxTime", "feedback", "wet", "dry"};
    static constexpr std::array<std::string_view, 5> kReverbFields{"roomSize", "damping", "width", "wet", "dry"};
    static constexpr std::array<std::string_view, 2> kLimiterFields{"threshold", "release"};

    static void newFilter(lua_State* L, haylen::audio::Filter::Kind kind);
    static void newDelay(lua_State* L);
    static void newReverb(lua_State* L);
    static void newLimiter(lua_State* L);
    static int reduction(lua_State* L);

    static int filterKind(lua_State* L);

    template <typename T> static int attached(lua_State* L);
    template <typename T> static int tail(lua_State* L);
};

} // namespace haylen::lua::audio
