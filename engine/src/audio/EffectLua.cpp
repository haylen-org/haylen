#include "audio/EffectLua.hpp"

#include <stdexcept>
#include <string>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::audio {

int EffectLua::newEffect(lua_State* L) {
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
    const std::optional<Filter::Kind> filter = lua::EnumNames<Filter::Kind>::fromName(kind);
    if (!filter) {
        throw std::invalid_argument("The audio effect must be lowpass, highpass, bandpass, notch, peak, lowShelf, highShelf, delay or reverb, not '" + std::string(kind) + "'.");
    }
    newFilter(L, *filter);
    return 1;
}

void EffectLua::newFilter(lua_State* L, Filter::Kind kind) {
    Filter::Settings settings;
    if (lua_istable(L, 2)) {
        const bool gain = kind == Filter::Kind::Peak || kind == Filter::Kind::LowShelf || kind == Filter::Kind::HighShelf;
        lua::Table::checkFields(L, 2, {gain ? lua::Table::FieldNames(kGainFilterFields) : lua::Table::FieldNames(kFilterFields)});
        lua::Table::readField(L, 2, "cutoff", settings.cutoff);
        lua::Table::readField(L, 2, "q", settings.q);
        lua::Table::readField(L, 2, "gain", settings.gain);
    }
    lua::Userdata::emplace<Filter>(L, std::make_shared<Filter>(kind, settings));
}

void EffectLua::newDelay(lua_State* L) {
    Delay::Settings settings;
    if (lua_istable(L, 2)) {
        lua::Table::checkFields(L, 2, {kDelayFields});
        lua::Table::readField(L, 2, "time", settings.time);
        lua::Table::readField(L, 2, "maxTime", settings.maxTime);
        lua::Table::readField(L, 2, "feedback", settings.feedback);
        lua::Table::readField(L, 2, "wet", settings.wet);
        lua::Table::readField(L, 2, "dry", settings.dry);
    }
    lua::Userdata::emplace<Delay>(L, std::make_shared<Delay>(settings));
}

void EffectLua::newReverb(lua_State* L) {
    Reverb::Settings settings;
    if (lua_istable(L, 2)) {
        lua::Table::checkFields(L, 2, {kReverbFields});
        lua::Table::readField(L, 2, "roomSize", settings.roomSize);
        lua::Table::readField(L, 2, "damping", settings.damping);
        lua::Table::readField(L, 2, "width", settings.width);
        lua::Table::readField(L, 2, "wet", settings.wet);
        lua::Table::readField(L, 2, "dry", settings.dry);
    }
    lua::Userdata::emplace<Reverb>(L, std::make_shared<Reverb>(settings));
}

std::shared_ptr<Effect> EffectLua::test(lua_State* L, int index) {
    if (lua::Userdata::test<Filter>(L, index) != nullptr) {
        return lua::Userdata::checkShared<Filter>(L, index);
    }
    if (lua::Userdata::test<Delay>(L, index) != nullptr) {
        return lua::Userdata::checkShared<Delay>(L, index);
    }
    if (lua::Userdata::test<Reverb>(L, index) != nullptr) {
        return lua::Userdata::checkShared<Reverb>(L, index);
    }
    return nullptr;
}

std::shared_ptr<Effect> EffectLua::check(lua_State* L, int index) {
    std::shared_ptr<Effect> effect = test(L, index);
    if (!effect) {
        luaL_typeerror(L, index, "audio effect");
    }
    return effect;
}

void EffectLua::push(lua_State* L, const std::shared_ptr<Effect>& effect) {
    if (auto filter = std::dynamic_pointer_cast<Filter>(effect)) {
        lua::Stack::push(L, std::move(filter));
        return;
    }
    if (auto delay = std::dynamic_pointer_cast<Delay>(effect)) {
        lua::Stack::push(L, std::move(delay));
        return;
    }
    if (auto reverb = std::dynamic_pointer_cast<Reverb>(effect)) {
        lua::Stack::push(L, std::move(reverb));
        return;
    }
    throw std::invalid_argument("The audio effect has no Lua class.");
}

int EffectLua::filterKind(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Filter>(L, 1).getKind());
    return 1;
}

template <typename T> int EffectLua::attached(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<T>(L, 1).isAttached());
    return 1;
}

template <typename T> int EffectLua::tail(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<T>(L, 1).getTail());
    return 1;
}

void EffectLua::install(lua_State* L) {
    lua::ClassBuilder<Filter>(L).property("kind", &filterKind).accessor<&Filter::getCutoff, &Filter::setCutoff>("cutoff").accessor<&Filter::getQ, &Filter::setQ>("q").accessor<&Filter::getGain, &Filter::setGain>("gain").property("attached", &attached<Filter>).property("tail", &tail<Filter>).meta("__eq", &lua::Userdata::equal<Filter>).install();
    lua::ClassBuilder<Delay>(L).accessor<&Delay::getTime, &Delay::setTime>("time").property("maxTime", &lua::Binding::method<&Delay::getMaxTime>).accessor<&Delay::getFeedback, &Delay::setFeedback>("feedback").accessor<&Delay::getWet, &Delay::setWet>("wet").accessor<&Delay::getDry, &Delay::setDry>("dry").property("attached", &attached<Delay>).property("tail", &tail<Delay>).meta("__eq", &lua::Userdata::equal<Delay>).install();
    lua::ClassBuilder<Reverb>(L).accessor<&Reverb::getRoomSize, &Reverb::setRoomSize>("roomSize").accessor<&Reverb::getDamping, &Reverb::setDamping>("damping").accessor<&Reverb::getWidth, &Reverb::setWidth>("width").accessor<&Reverb::getWet, &Reverb::setWet>("wet").accessor<&Reverb::getDry, &Reverb::setDry>("dry").property("attached", &attached<Reverb>).property("tail", &tail<Reverb>).meta("__eq", &lua::Userdata::equal<Reverb>).install();
}

} // namespace haylen::audio
