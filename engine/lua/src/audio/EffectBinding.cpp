#include "audio/EffectBinding.hpp"

#include <stdexcept>
#include <string>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::audio {

int EffectBinding::newEffect(lua_State* L) {
    const auto kind = lua::Stack::read<std::string_view>(L, 1);

    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
    }

    if (kind == "delay") {
        newDelay(L);
        return 1;
    }

    if (kind == "reverb") {
        newReverb(L);
        return 1;
    }

    if (kind == "limiter") {
        newLimiter(L);
        return 1;
    }

    const std::optional<haylen::audio::Filter::Kind> filter = haylen::core::EnumNames<haylen::audio::Filter::Kind>::fromName(kind);
    if (!filter) {
        throw std::invalid_argument("The audio effect must be \"lowpass\", \"highpass\", \"bandpass\", \"notch\", \"peak\", \"lowShelf\", \"highShelf\", \"delay\", \"reverb\" or \"limiter\", not \"" + std::string(kind) + "\".");
    }

    newFilter(L, *filter);
    return 1;
}

void EffectBinding::newFilter(lua_State* L, haylen::audio::Filter::Kind kind) {
    haylen::audio::Filter::Settings settings;
    if (lua_istable(L, 2)) {
        const bool gain = kind == haylen::audio::Filter::Kind::Peak || kind == haylen::audio::Filter::Kind::LowShelf || kind == haylen::audio::Filter::Kind::HighShelf;
        lua::Table::checkFields(L, 2, {gain ? lua::Table::FieldNames(kGainFilterFields) : lua::Table::FieldNames(kFilterFields)});
        lua::Table::readField(L, 2, "cutoff", settings.cutoff);
        lua::Table::readField(L, 2, "q", settings.q);
        lua::Table::readField(L, 2, "gain", settings.gain);
    }

    lua::Userdata::emplace<haylen::audio::Filter>(L, std::make_shared<haylen::audio::Filter>(kind, settings));
}

void EffectBinding::newDelay(lua_State* L) {
    haylen::audio::Delay::Settings settings;
    if (lua_istable(L, 2)) {
        lua::Table::checkFields(L, 2, {kDelayFields});
        lua::Table::readField(L, 2, "time", settings.time);
        lua::Table::readField(L, 2, "maxTime", settings.maxTime);
        lua::Table::readField(L, 2, "feedback", settings.feedback);
        lua::Table::readField(L, 2, "wet", settings.wet);
        lua::Table::readField(L, 2, "dry", settings.dry);
    }

    lua::Userdata::emplace<haylen::audio::Delay>(L, std::make_shared<haylen::audio::Delay>(settings));
}

void EffectBinding::newReverb(lua_State* L) {
    haylen::audio::Reverb::Settings settings;
    if (lua_istable(L, 2)) {
        lua::Table::checkFields(L, 2, {kReverbFields});
        lua::Table::readField(L, 2, "roomSize", settings.roomSize);
        lua::Table::readField(L, 2, "damping", settings.damping);
        lua::Table::readField(L, 2, "width", settings.width);
        lua::Table::readField(L, 2, "wet", settings.wet);
        lua::Table::readField(L, 2, "dry", settings.dry);
    }

    lua::Userdata::emplace<haylen::audio::Reverb>(L, std::make_shared<haylen::audio::Reverb>(settings));
}

void EffectBinding::newLimiter(lua_State* L) {
    haylen::audio::Limiter::Settings settings;
    if (lua_istable(L, 2)) {
        lua::Table::checkFields(L, 2, {kLimiterFields});
        lua::Table::readField(L, 2, "threshold", settings.threshold);
        lua::Table::readField(L, 2, "release", settings.release);
    }

    lua::Userdata::emplace<haylen::audio::Limiter>(L, std::make_shared<haylen::audio::Limiter>(settings));
}

int EffectBinding::reduction(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::audio::Limiter>(L, 1).getReduction());
    return 1;
}

std::shared_ptr<haylen::audio::Effect> EffectBinding::test(lua_State* L, int index) {
    if (lua::Userdata::test<haylen::audio::Filter>(L, index) != nullptr) {
        return lua::Userdata::checkShared<haylen::audio::Filter>(L, index);
    }

    if (lua::Userdata::test<haylen::audio::Delay>(L, index) != nullptr) {
        return lua::Userdata::checkShared<haylen::audio::Delay>(L, index);
    }

    if (lua::Userdata::test<haylen::audio::Reverb>(L, index) != nullptr) {
        return lua::Userdata::checkShared<haylen::audio::Reverb>(L, index);
    }

    if (lua::Userdata::test<haylen::audio::Limiter>(L, index) != nullptr) {
        return lua::Userdata::checkShared<haylen::audio::Limiter>(L, index);
    }

    return nullptr;
}

std::shared_ptr<haylen::audio::Effect> EffectBinding::check(lua_State* L, int index) {
    std::shared_ptr<haylen::audio::Effect> effect = test(L, index);
    if (!effect) {
        luaL_typeerror(L, index, "audio effect");
    }

    return effect;
}

void EffectBinding::push(lua_State* L, const std::shared_ptr<haylen::audio::Effect>& effect) {
    if (auto filter = std::dynamic_pointer_cast<haylen::audio::Filter>(effect)) {
        lua::Stack::push(L, std::move(filter));
        return;
    }

    if (auto delay = std::dynamic_pointer_cast<haylen::audio::Delay>(effect)) {
        lua::Stack::push(L, std::move(delay));
        return;
    }

    if (auto reverb = std::dynamic_pointer_cast<haylen::audio::Reverb>(effect)) {
        lua::Stack::push(L, std::move(reverb));
        return;
    }

    if (auto limiter = std::dynamic_pointer_cast<haylen::audio::Limiter>(effect)) {
        lua::Stack::push(L, std::move(limiter));
        return;
    }

    throw std::invalid_argument("The audio effect has no Lua class.");
}

int EffectBinding::filterKind(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::audio::Filter>(L, 1).getKind());
    return 1;
}

template <typename T> int EffectBinding::attached(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<T>(L, 1).isAttached());
    return 1;
}

template <typename T> int EffectBinding::tail(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<T>(L, 1).getTail());
    return 1;
}

void EffectBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::audio::Filter>(L).property("kind", &filterKind).accessor<&haylen::audio::Filter::getCutoff, &haylen::audio::Filter::setCutoff>("cutoff").accessor<&haylen::audio::Filter::getQ, &haylen::audio::Filter::setQ>("q").accessor<&haylen::audio::Filter::getGain, &haylen::audio::Filter::setGain>("gain").property("attached", &attached<haylen::audio::Filter>).property("tail", &tail<haylen::audio::Filter>).meta("__eq", &lua::Userdata::equal<haylen::audio::Filter>).install();
    lua::ClassBuilder<haylen::audio::Delay>(L).accessor<&haylen::audio::Delay::getTime, &haylen::audio::Delay::setTime>("time").property("maxTime", &lua::Binding::method<&haylen::audio::Delay::getMaxTime>).accessor<&haylen::audio::Delay::getFeedback, &haylen::audio::Delay::setFeedback>("feedback").accessor<&haylen::audio::Delay::getWet, &haylen::audio::Delay::setWet>("wet").accessor<&haylen::audio::Delay::getDry, &haylen::audio::Delay::setDry>("dry").property("attached", &attached<haylen::audio::Delay>).property("tail", &tail<haylen::audio::Delay>).meta("__eq", &lua::Userdata::equal<haylen::audio::Delay>).install();
    lua::ClassBuilder<haylen::audio::Reverb>(L).accessor<&haylen::audio::Reverb::getRoomSize, &haylen::audio::Reverb::setRoomSize>("roomSize").accessor<&haylen::audio::Reverb::getDamping, &haylen::audio::Reverb::setDamping>("damping").accessor<&haylen::audio::Reverb::getWidth, &haylen::audio::Reverb::setWidth>("width").accessor<&haylen::audio::Reverb::getWet, &haylen::audio::Reverb::setWet>("wet").accessor<&haylen::audio::Reverb::getDry, &haylen::audio::Reverb::setDry>("dry").property("attached", &attached<haylen::audio::Reverb>).property("tail", &tail<haylen::audio::Reverb>).meta("__eq", &lua::Userdata::equal<haylen::audio::Reverb>).install();
    lua::ClassBuilder<haylen::audio::Limiter>(L).accessor<&haylen::audio::Limiter::getThreshold, &haylen::audio::Limiter::setThreshold>("threshold").accessor<&haylen::audio::Limiter::getRelease, &haylen::audio::Limiter::setRelease>("release").property("reduction", &reduction).property("attached", &attached<haylen::audio::Limiter>).property("tail", &tail<haylen::audio::Limiter>).meta("__eq", &lua::Userdata::equal<haylen::audio::Limiter>).install();
}

} // namespace haylen::lua::audio
