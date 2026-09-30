#include "core/TweenLua.hpp"

#include <lua.hpp>

#include <algorithm>
#include <cmath>
#include <random>
#include <span>
#include <utility>

#include "core/ScriptedTrack.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/PropertyTrack.hpp"
#include "haylen/core/TweenManager.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/NativeProperty.hpp"
#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "lua/ScriptedScene.hpp"
#include "lua/WeakReference.hpp"
#include "varn/async/Promise.h"

namespace haylen::lua {

template <> struct Type<core::Tween> {
    static constexpr const char* name = "haylen.Tween";
    using Storage = std::shared_ptr<core::Tween>;
};

template <> struct Type<core::Timeline> {
    static constexpr const char* name = "haylen.Timeline";
    using Storage = std::shared_ptr<core::Timeline>;
};

template <> struct EnumNames<core::Tween::LoopMode> {
    static constexpr std::array<std::pair<std::string_view, core::Tween::LoopMode>, 3> kModes{{{"restart", core::Tween::LoopMode::Restart}, {"yoyo", core::Tween::LoopMode::Yoyo}, {"incremental", core::Tween::LoopMode::Incremental}}};

    static std::optional<core::Tween::LoopMode> fromName(std::string_view name) {
        for (const auto& [candidate, mode] : kModes) {
            if (candidate == name) {
                return mode;
            }
        }
        return std::nullopt;
    }

    static std::string_view name(core::Tween::LoopMode value) {
        return kModes[static_cast<std::size_t>(value)].first;
    }
};

template <> struct EnumNames<core::Timeline::StaggerOrigin> {
    static constexpr std::array<std::pair<std::string_view, core::Timeline::StaggerOrigin>, 3> kOrigins{{{"start", core::Timeline::StaggerOrigin::Start}, {"end", core::Timeline::StaggerOrigin::End}, {"center", core::Timeline::StaggerOrigin::Center}}};

    static std::optional<core::Timeline::StaggerOrigin> fromName(std::string_view name) {
        for (const auto& [candidate, origin] : kOrigins) {
            if (candidate == name) {
                return origin;
            }
        }
        return std::nullopt;
    }

    static std::string_view name(core::Timeline::StaggerOrigin value) {
        return kOrigins[static_cast<std::size_t>(value)].first;
    }
};

} // namespace haylen::lua

namespace haylen::core {

std::shared_ptr<Tween> TweenLua::check(lua_State* L, int index) {
    if (auto* tween = static_cast<std::shared_ptr<Tween>*>(luaL_testudata(L, index, lua::Type<Tween>::name)); tween != nullptr && *tween) {
        return *tween;
    }
    if (auto* timeline = static_cast<std::shared_ptr<Timeline>*>(luaL_testudata(L, index, lua::Type<Timeline>::name)); timeline != nullptr && *timeline) {
        return *timeline;
    }
    luaL_typeerror(L, index, "haylen.Tween");
    return nullptr;
}

Timeline& TweenLua::checkTimeline(lua_State* L, int index) {
    return lua::Userdata::check<Timeline>(L, index);
}

void TweenLua::pushHandle(lua_State* L, const std::shared_ptr<Tween>& tween) {
    if (auto timeline = std::dynamic_pointer_cast<Timeline>(tween)) {
        lua::Userdata::emplace<Timeline>(L, std::move(timeline));
        return;
    }
    lua::Userdata::emplace<Tween>(L, tween);
}

int TweenLua::readOwner(lua_State* L, int options) {
    if (options == 0) {
        return 0;
    }
    if (lua_getfield(L, options, "owner") == LUA_TNIL) {
        lua_pop(L, 1);
        return 0;
    }
    lua::Owners::checkOwner(L, -1);
    return lua_gettop(L);
}

void TweenLua::call(const lua::Owners::Function& function, lua_State* main) {
    if (function.push(main)) {
        lua::Runtime::protectedCall(main, 0, 0);
    }
}

// Callbacks of a tween with an owner are held through the owner, so a callback that refers to the owner never keeps it alive.
void TweenLua::readCallbacks(lua_State* L, int options, int owner, Tween::Callbacks& callbacks) {
    lua_State* main = lua::Runtime::getMainThread(L);
    // clang-format off
    const auto read = [L, options, owner](const char* name) -> std::shared_ptr<lua::Owners::Function> {
        if (lua_getfield(L, options, name) == LUA_TNIL) {
            lua_pop(L, 1);
            return nullptr;
        }
        if (!lua_isfunction(L, -1)) {
            luaL_error(L, "The tween option \"%s\" must be a function.", name);
        }
        auto function = std::make_shared<lua::Owners::Function>(L, -1, owner);
        lua_pop(L, 1);
        return function;
    };
    // clang-format on
    if (auto function = read("onStart")) {
        callbacks.start = [function, main] { call(*function, main); };
    }
    if (auto function = read("onUpdate")) {
        callbacks.update = [function, main](float progress) { call(*function, main, progress); };
    }
    if (auto function = read("onLoop")) {
        callbacks.loop = [function, main](int loop) { call(*function, main, loop); };
    }
    if (auto function = read("onStep")) {
        callbacks.step = [function, main](int step) { call(*function, main, step + 1); };
    }
    if (auto function = read("onComplete")) {
        callbacks.complete = [function, main] { call(*function, main); };
    }
    if (auto function = read("onKill")) {
        callbacks.kill = [function, main] { call(*function, main); };
    }
}

void TweenLua::configure(lua_State* L, int options, int owner, Tween& tween) {
    if (options == 0) {
        return;
    }
    float delay = tween.getDelay();
    int repeatCount = tween.getRepeatCount();
    Tween::LoopMode loop = tween.getLoopMode();
    float repeatDelay = tween.getRepeatDelay();
    float timeScale = tween.getTimeScale();
    ProcessMode processMode = tween.getProcessMode();
    bool unscaled = tween.isUnscaled();
    bool fixed = tween.isFixedStep();
    bool autoKill = tween.isAutoKill();
    std::string tag;
    lua::Table::readField(L, options, "delay", delay);
    lua::Table::readField(L, options, "repeatCount", repeatCount);
    lua::Table::readField(L, options, "loopMode", loop);
    lua::Table::readField(L, options, "repeatDelay", repeatDelay);
    lua::Table::readField(L, options, "timeScale", timeScale);
    lua::Table::readField(L, options, "processMode", processMode);
    lua::Table::readField(L, options, "unscaled", unscaled);
    lua::Table::readField(L, options, "fixedStep", fixed);
    lua::Table::readField(L, options, "autoKill", autoKill);
    lua::Table::readField(L, options, "tag", tag);

    tween.setDelay(delay);
    tween.setRepeatCount(repeatCount);
    tween.setLoopMode(loop);
    tween.setRepeatDelay(repeatDelay);
    tween.setTimeScale(timeScale);
    tween.setProcessMode(processMode);
    tween.setUnscaled(unscaled);
    tween.setFixedStep(fixed);
    tween.setAutoKill(autoKill);
    tween.setTag(std::move(tag));
    readCallbacks(L, options, owner, tween.getCallbacks());
}

// Hands the tween to the manager, ties it to its owner and pushes its handle. A tween that inherits its process mode follows the one of its owner.
void TweenLua::start(lua_State* L, const std::shared_ptr<Tween>& tween, int options, int owner) {
    Engine& engine = lua::Runtime::getEngine(L);
    if (tween->getProcessMode() == ProcessMode::Inherit && owner != 0) {
        tween->setParentMode(lua::ScriptedScene::followOwnerMode(L, owner));
    }
    engine.getTweens().add(tween);
    if (owner != 0) {
        lua::Owners::add(L, owner, tween->getConnection());
    }
    bool paused = false;
    if (options != 0) {
        lua::Table::readField(L, options, "paused", paused);
    }
    if (paused) {
        tween->pause();
    }
    pushHandle(L, tween);
}

std::shared_ptr<PropertyTween> TweenLua::createTween(lua_State* L, float length, int options, int owner) {
    bool speedBased = false;
    if (options != 0) {
        lua::Table::readField(L, options, "speedBased", speedBased);
    }
    auto tween = std::make_shared<PropertyTween>(length, speedBased);
    configure(L, options, owner, *tween);
    if (options != 0) {
        math::EasingCurve ease;
        lua::Table::readField(L, options, "ease", ease);
        tween->setEase(std::move(ease));
    }
    return tween;
}

bool TweenLua::isListed(lua_State* L, int options, const char* list, std::string_view name) {
    if (options == 0 || lua_getfield(L, options, list) == LUA_TNIL) {
        if (options != 0) {
            lua_pop(L, 1);
        }
        return false;
    }
    if (!lua_istable(L, -1)) {
        luaL_error(L, "The tween option \"%s\" must be a list of field names.", list);
    }
    bool found = false;
    const auto length = static_cast<lua_Integer>(lua_rawlen(L, -1));
    for (lua_Integer index = 1; index <= length && !found; ++index) {
        lua_rawgeti(L, -1, index);
        found = lua_type(L, -1) == LUA_TSTRING && name == lua_tostring(L, -1);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return found;
}

TweenValue::Interpolation TweenLua::getInterpolation(lua_State* L, int options, std::string_view name, TweenValue::Kind kind) {
    if (kind == TweenValue::Kind::Number && isListed(L, options, "angles", name)) {
        return TweenValue::Interpolation::Angle;
    }
    if (kind == TweenValue::Kind::Number && isListed(L, options, "integers", name)) {
        return TweenValue::Interpolation::Integer;
    }
    if (kind == TweenValue::Kind::Color && options != 0) {
        std::string space = "rgb";
        lua::Table::readField(L, options, "colorSpace", space);
        if (space != "rgb" && space != "hsv") {
            luaL_error(L, "The tween option \"colorSpace\" must be \"rgb\" or \"hsv\".");
        }
        return space == "hsv" ? TweenValue::Interpolation::Hsv : TweenValue::Interpolation::Linear;
    }
    return TweenValue::Interpolation::Linear;
}

std::optional<std::vector<TweenLua::NativeField>> TweenLua::findNativeFields(lua_State* L, int target, const std::vector<std::string>& names) {
    if (lua_type(L, target) != LUA_TUSERDATA) {
        return std::nullopt;
    }
    std::vector<NativeField> fields;
    for (const std::string& name : names) {
        const std::size_t dot = name.find('.');
        const std::string_view head = std::string_view(name).substr(0, dot);
        const lua::NativeProperty* property = lua::NativeProperty::find(L, target, head);
        if (property == nullptr) {
            return std::nullopt;
        }

        // One level of components reaches into a `Vec2` or a `Color`, such as `position.x` or `color.a`.
        NativeField field{.storage = lua_touserdata(L, target), .property = property};
        if (dot != std::string::npos) {
            const std::string_view tail = std::string_view(name).substr(dot + 1);
            const std::string_view components = property->kind == lua::NativeProperty::Kind::Vector ? "xy" : property->kind == lua::NativeProperty::Kind::Color ? "rgba" : "";
            if (tail.size() != 1 || components.find(tail.front()) == std::string_view::npos) {
                return std::nullopt;
            }
            field.component = static_cast<int>(components.find(tail.front()));
        }
        const bool number = field.component >= 0 || property->kind == lua::NativeProperty::Kind::Number;
        if (names.size() == 2 && !number) {
            return std::nullopt;
        }
        fields.push_back(field);
    }
    return fields;
}

TweenValue TweenLua::readNative(const std::vector<NativeField>& fields) {
    // clang-format off
    const auto readOne = [](const NativeField& field) -> TweenValue {
        lua::NativeProperty::Values values{};
        (void)field.property->read(field.storage, values);
        if (field.component >= 0) {
            return static_cast<double>(values[static_cast<std::size_t>(field.component)]);
        }
        switch (field.property->kind) {
        case lua::NativeProperty::Kind::Vector:
            return math::Vec2{values[0], values[1]};
        case lua::NativeProperty::Kind::Color:
            return math::Color{values[0], values[1], values[2], values[3]};
        case lua::NativeProperty::Kind::Number:
            break;
        }
        return static_cast<double>(values[0]);
    };
    // clang-format on
    if (fields.size() == 2) {
        return math::Vec2{static_cast<float>(readOne(fields[0]).getNumber()), static_cast<float>(readOne(fields[1]).getNumber())};
    }
    return readOne(fields.front());
}

void TweenLua::writeNative(const std::vector<NativeField>& fields, const TweenValue& value) {
    // clang-format off
    const auto writeNumber = [](const NativeField& field, float number) {
        lua::NativeProperty::Values values{};
        (void)field.property->read(field.storage, values);
        values[static_cast<std::size_t>(std::max(field.component, 0))] = number;
        (void)field.property->write(field.storage, values);
    };
    // clang-format on
    if (fields.size() == 2) {
        const math::Vec2 vector = value.getVector();
        writeNumber(fields[0], vector.x);
        writeNumber(fields[1], vector.y);
        return;
    }

    const NativeField& field = fields.front();
    switch (value.getKind()) {
    case TweenValue::Kind::Number:
        writeNumber(field, static_cast<float>(value.getNumber()));
        return;
    case TweenValue::Kind::Vector: {
        lua::NativeProperty::Values values{};
        lua::NativeProperty::pack(value.getVector(), values);
        (void)field.property->write(field.storage, values);
        return;
    }
    case TweenValue::Kind::Color: {
        lua::NativeProperty::Values values{};
        lua::NativeProperty::pack(value.getColor(), values);
        (void)field.property->write(field.storage, values);
        return;
    }
    case TweenValue::Kind::Text:
        return;
    }
}

TweenValue TweenLua::readCurrent(Builder& builder, const std::vector<std::string>& names) {
    if (const std::optional<std::vector<NativeField>> fields = findNativeFields(builder.state, builder.target, names)) {
        if (lua::NativeProperty::Values values{}; !fields->front().property->read(fields->front().storage, values)) {
            luaL_error(builder.state, "Cannot tween an object that was already released.");
        }
        return readNative(*fields);
    }
    return ScriptedTrack::read(builder.state, builder.target, ScriptedTrack::split(names));
}

TweenValue TweenLua::readLike(lua_State* L, int index, const TweenValue& current, std::string_view name) {
    const std::string field(name);
    switch (current.getKind()) {
    case TweenValue::Kind::Number:
        if (lua_type(L, index) != LUA_TNUMBER) {
            luaL_error(L, "The tween value of \"%s\" must be a number.", field.c_str());
        }
        return lua_tonumber(L, index);
    case TweenValue::Kind::Vector:
        return lua::Stack::read<math::Vec2>(L, index);
    case TweenValue::Kind::Color:
        return lua::Stack::read<math::Color>(L, index);
    case TweenValue::Kind::Text:
        if (lua_type(L, index) != LUA_TSTRING) {
            luaL_error(L, "The tween value of \"%s\" must be a text.", field.c_str());
        }
        return lua::Stack::read<std::string>(L, index);
    }
    return current;
}

void TweenLua::addProperty(Builder& builder, const std::vector<std::string>& names, TweenProperty property) {
    lua_State* L = builder.state;
    if (std::optional<std::vector<NativeField>> fields = findNativeFields(L, builder.target, names)) {
        // A native property is animated without running Lua, and the weak reference only tells whether the object still exists.
        auto reference = std::make_shared<lua::WeakReference>(L, builder.target);
        const NativeField first = fields->front();
        // clang-format off
        builder.tween->addTrack(std::make_unique<PropertyTrack>(lua_topointer(L, builder.target), names,
            [fields = *fields] { return readNative(fields); },
            [fields = *fields](const TweenValue& value) { writeNative(fields, value); },
            std::move(property),
            [reference, first] {
                lua::NativeProperty::Values values{};
                return reference->isAlive() && first.property->read(first.storage, values);
            }));
        // clang-format on
        return;
    }
    if (!builder.scripted) {
        builder.scripted = std::make_unique<ScriptedTrack>(L, builder.target);
    }
    builder.scripted->add(names, std::move(property));
}

void TweenLua::finishTracks(Builder& builder) {
    if (builder.scripted) {
        builder.tween->addTrack(std::move(builder.scripted));
    }
}

// Shared by `to`, `from`, `by` and `fromTo`: `tween.to(target, seconds, values, options)` and `tween.fromTo(target, seconds, from, to, options)`.
void TweenLua::checkTarget(lua_State* L) {
    const int type = lua_type(L, 1);
    if (type != LUA_TTABLE && type != LUA_TUSERDATA) {
        luaL_error(L, "A tween target must be a table or a userdata, not %s.", luaL_typename(L, 1));
    }
}

int TweenLua::startValues(lua_State* L, TweenProperty::Mode mode) {
    checkTarget(L);
    const bool pair = mode == TweenProperty::Mode::FromTo;
    const int values = 3;
    const int ends = pair ? 4 : 0;
    const int optionsArgument = pair ? 5 : 4;
    const auto seconds = lua::Stack::read<float>(L, 2);
    luaL_checktype(L, values, LUA_TTABLE);
    if (pair) {
        luaL_checktype(L, ends, LUA_TTABLE);
    }
    int options = 0;
    if (!lua_isnoneornil(L, optionsArgument)) {
        luaL_checktype(L, optionsArgument, LUA_TTABLE);
        lua::Table::checkFields(L, optionsArgument, {kCommonFields, kTweenFields});
        options = optionsArgument;
    }
    const int owner = readOwner(L, options);
    const std::shared_ptr<PropertyTween> tween = createTween(L, seconds, options, owner);

    Builder builder{.state = L, .target = 1, .options = options, .tween = tween.get()};
    lua_pushnil(L);
    while (lua_next(L, values) != 0) {
        luaL_argcheck(L, lua_type(L, -2) == LUA_TSTRING, values, "tween fields are named by strings");
        const std::string name = lua_tostring(L, -2);
        const std::vector<std::string> names{name};
        const TweenValue current = readCurrent(builder, names);
        const TweenValue given = readLike(L, -1, current, name);
        lua_pop(L, 1);

        TweenProperty property = TweenProperty::to(given);
        if (mode == TweenProperty::Mode::From) {
            property = TweenProperty::from(given);
        } else if (mode == TweenProperty::Mode::By) {
            property = TweenProperty::by(given);
        } else if (pair) {
            if (lua_getfield(L, ends, name.c_str()) == LUA_TNIL) {
                luaL_error(L, "A \"tween.fromTo\" call needs an end value for the field \"%s\".", name.c_str());
            }
            property = TweenProperty::fromTo(given, readLike(L, -1, current, name));
            lua_pop(L, 1);
        }
        property.setInterpolation(getInterpolation(L, options, name, current.getKind()));
        addProperty(builder, names, std::move(property));
    }
    finishTracks(builder);

    // A `from` tween shows its start values right away, even while its delay runs.
    if (mode == TweenProperty::Mode::From) {
        tween->renderStart();
    }
    bool overwrite = false;
    if (options != 0) {
        lua::Table::readField(L, options, "overwrite", overwrite);
    }
    if (overwrite) {
        lua::Runtime::getEngine(L).getTweens().overwrite(*tween);
    }
    start(L, tween, options, owner);
    return 1;
}

int TweenLua::to(lua_State* L) {
    return startValues(L, TweenProperty::Mode::To);
}

int TweenLua::from(lua_State* L) {
    return startValues(L, TweenProperty::Mode::From);
}

int TweenLua::by(lua_State* L) {
    return startValues(L, TweenProperty::Mode::By);
}

int TweenLua::fromTo(lua_State* L) {
    return startValues(L, TweenProperty::Mode::FromTo);
}

std::vector<std::string> TweenLua::readFieldNames(lua_State* L, int options, std::vector<std::string> defaults) {
    if (options == 0 || lua_getfield(L, options, "field") == LUA_TNIL) {
        if (options != 0) {
            lua_pop(L, 1);
        }
        return defaults;
    }
    std::vector<std::string> names;
    if (lua_type(L, -1) == LUA_TSTRING) {
        names.emplace_back(lua_tostring(L, -1));
    } else if (lua_istable(L, -1) && lua_rawlen(L, -1) == 2) {
        for (lua_Integer index = 1; index <= 2; ++index) {
            lua_rawgeti(L, -1, index);
            names.push_back(lua::Stack::read<std::string>(L, -1));
            lua_pop(L, 1);
        }
    } else {
        luaL_error(L, "The tween option \"field\" must be a field name or a list of two number field names.");
    }
    lua_pop(L, 1);
    return names;
}

std::vector<math::Vec2> TweenLua::readPoints(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    return lua::Stack::read<std::vector<math::Vec2>>(L, index);
}

// Shared by the ready-made tweens, which take the target, the seconds, one value of their own and the options. The `make` function turns that value into the property of the fields the tween animates.
int TweenLua::startReadyMade(lua_State* L, std::initializer_list<lua::Table::FieldNames> extra, std::vector<std::string> defaults, const std::function<TweenProperty(Builder&, const TweenValue&)>& make) {
    checkTarget(L);
    const auto seconds = lua::Stack::read<float>(L, 2);
    luaL_checkany(L, 3);
    int options = 0;
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        std::vector<lua::Table::FieldNames> allowed{kCommonFields, kTweenFields, kFieldFields};
        allowed.insert(allowed.end(), extra.begin(), extra.end());
        lua::Table::checkFields(L, 4, std::span<const lua::Table::FieldNames>(allowed));
        options = 4;
    }
    const int owner = readOwner(L, options);
    const std::shared_ptr<PropertyTween> tween = createTween(L, seconds, options, owner);

    Builder builder{.state = L, .target = 1, .options = options, .tween = tween.get()};
    const std::vector<std::string> names = readFieldNames(L, options, std::move(defaults));
    const TweenValue current = readCurrent(builder, names);
    TweenProperty property = make(builder, current);
    if (property.getInterpolation() == TweenValue::Interpolation::Linear) {
        property.setInterpolation(getInterpolation(L, options, names.front(), current.getKind()));
    }
    addProperty(builder, names, std::move(property));
    finishTracks(builder);

    // Values that follow the main one, such as the heading of a path, begin after it.
    for (auto& [pendingNames, pendingProperty] : builder.pending) {
        addProperty(builder, pendingNames, std::move(pendingProperty));
    }
    finishTracks(builder);

    bool overwrite = false;
    if (options != 0) {
        lua::Table::readField(L, options, "overwrite", overwrite);
    }
    if (overwrite) {
        lua::Runtime::getEngine(L).getTweens().overwrite(*tween);
    }
    start(L, tween, options, owner);
    return 1;
}

void TweenLua::requireVector(lua_State* L, const TweenValue& current, const char* kind) {
    if (current.getKind() != TweenValue::Kind::Vector) {
        luaL_error(L, "A %s tween needs a \"Vec2\" field or a pair of number fields.", kind);
    }
}

int TweenLua::move(lua_State* L) {
    return startReadyMade(L, {}, {"x", "y"}, [](Builder& builder, const TweenValue& current) { return TweenProperty::to(readLike(builder.state, 3, current, "position")); });
}

// A single number scales both axes of a `Vec2` or of a pair of fields.
int TweenLua::scale(lua_State* L) {
    // clang-format off
    return startReadyMade(L, {}, {"scaleX", "scaleY"}, [](Builder& builder, const TweenValue& current) {
        lua_State* state = builder.state;
        if (current.getKind() == TweenValue::Kind::Vector && lua_type(state, 3) == LUA_TNUMBER) {
            const auto uniform = static_cast<float>(lua_tonumber(state, 3));
            return TweenProperty::to(math::Vec2{uniform, uniform});
        }
        return TweenProperty::to(readLike(state, 3, current, "scale"));
    });
    // clang-format on
}

int TweenLua::rotate(lua_State* L) {
    // clang-format off
    return startReadyMade(L, {}, {"rotation"}, [](Builder& builder, const TweenValue& current) {
        TweenProperty property = TweenProperty::to(readLike(builder.state, 3, current, "rotation"));
        property.setInterpolation(TweenValue::Interpolation::Angle);
        return property;
    });
    // clang-format on
}

int TweenLua::fade(lua_State* L) {
    return startReadyMade(L, {}, {"color.a"}, [](Builder& builder, const TweenValue& current) { return TweenProperty::to(readLike(builder.state, 3, current, "alpha")); });
}

int TweenLua::tint(lua_State* L) {
    return startReadyMade(L, {}, {"color"}, [](Builder& builder, const TweenValue& current) { return TweenProperty::to(readLike(builder.state, 3, current, "color")); });
}

int TweenLua::jump(lua_State* L) {
    // clang-format off
    return startReadyMade(L, {kJumpFields}, {"x", "y"}, [](Builder& builder, const TweenValue& current) {
        float power = 100.0F;
        int jumps = 1;
        if (builder.options != 0) {
            lua::Table::readField(builder.state, builder.options, "power", power);
            lua::Table::readField(builder.state, builder.options, "jumps", jumps);
        }
        requireVector(builder.state, current, "jump");
        TweenProperty property = TweenProperty::to(readLike(builder.state, 3, current, "position"));
        property.setMotion(TweenMotion::jump(power, jumps));
        return property;
    });
    // clang-format on
}

// The path starts where the target is and ends at the last point. With `orient`, the `rotation` field turns along the path.
int TweenLua::path(lua_State* L) {
    // clang-format off
    return startReadyMade(L, {kPathFields}, {"x", "y"}, [](Builder& builder, const TweenValue& current) {
        lua_State* state = builder.state;
        requireVector(state, current, "path");
        bool curved = true;
        bool closed = false;
        bool orient = false;
        std::string orientField = "rotation";
        if (builder.options != 0) {
            lua::Table::readField(state, builder.options, "curved", curved);
            lua::Table::readField(state, builder.options, "closed", closed);
            lua::Table::readField(state, builder.options, "orient", orient);
            lua::Table::readField(state, builder.options, "orientField", orientField);
        }
        std::vector<math::Vec2> points = readPoints(state, 3);
        if (points.empty()) {
            luaL_error(state, "A path needs at least one point.");
        }
        const math::Vec2 end = closed ? math::Vec2{} : points.back();
        std::shared_ptr<TweenMotion> motion = TweenMotion::path(std::move(points), curved, closed);
        TweenProperty property = TweenProperty::to(end);
        property.setMotion(motion);
        if (orient) {
            const std::vector<std::string> names{orientField};
            TweenProperty heading = TweenProperty::to(readCurrent(builder, names));
            heading.setMotion(TweenMotion::orientation(motion));
            builder.pending.emplace_back(names, std::move(heading));
        }
        return property;
    });
    // clang-format on
}

int TweenLua::bezier(lua_State* L) {
    // clang-format off
    return startReadyMade(L, {}, {"x", "y"}, [](Builder& builder, const TweenValue& current) {
        requireVector(builder.state, current, "Bézier");
        std::vector<math::Vec2> points = readPoints(builder.state, 3);
        if (points.size() < 2 || points.size() > 3) {
            luaL_error(builder.state, "A Bézier tween needs one or two control points followed by the end point.");
        }
        const math::Vec2 end = points.back();
        points.pop_back();
        TweenProperty property = TweenProperty::to(end);
        property.setMotion(TweenMotion::bezier(std::move(points)));
        return property;
    });
    // clang-format on
}

// Blinks `count` times between the current value and a hidden one, 0 for alpha by default, and ends on the current value.
int TweenLua::blink(lua_State* L) {
    // clang-format off
    return startReadyMade(L, {kBlinkFields}, {"color.a"}, [](Builder& builder, const TweenValue& current) {
        lua_State* state = builder.state;
        const auto count = static_cast<int>(luaL_checkinteger(state, 3));
        TweenValue hidden = current.getKind() == TweenValue::Kind::Color ? TweenValue(math::Color::transparent()) : TweenValue(0.0);
        if (builder.options != 0 && lua_getfield(state, builder.options, "hidden") != LUA_TNIL) {
            hidden = readLike(state, -1, current, "hidden");
        } else if (current.getKind() != TweenValue::Kind::Number && current.getKind() != TweenValue::Kind::Color) {
            luaL_error(state, "A blink of a \"Vec2\" or a text needs the \"hidden\" option.");
        }
        if (builder.options != 0) {
            lua_pop(state, 1);
        }
        TweenProperty property = TweenProperty::to(hidden);
        property.setMotion(TweenMotion::blink(count));
        return property;
    });
    // clang-format on
}

// A strength of one number shakes both axes of a `Vec2` by the same amount.
int TweenLua::shake(lua_State* L) {
    // clang-format off
    return startReadyMade(L, {kShakeFields}, {"x", "y"}, [](Builder& builder, const TweenValue& current) {
        lua_State* state = builder.state;
        int vibrato = 10;
        float randomness = 90.0F;
        auto seed = static_cast<std::uint32_t>(std::random_device{}());
        if (builder.options != 0) {
            lua::Table::readField(state, builder.options, "vibrato", vibrato);
            lua::Table::readField(state, builder.options, "randomness", randomness);
            lua::Table::readField(state, builder.options, "seed", seed);
        }
        TweenValue strength = current.getKind() == TweenValue::Kind::Vector && lua_type(state, 3) == LUA_TNUMBER ? TweenValue(math::Vec2{static_cast<float>(lua_tonumber(state, 3)), static_cast<float>(lua_tonumber(state, 3))}) : readLike(state, 3, current, "strength");
        TweenProperty property = TweenProperty::to(std::move(strength));
        property.setMotion(TweenMotion::shake(vibrato, randomness, seed));
        return property;
    });
    // clang-format on
}

int TweenLua::punch(lua_State* L) {
    // clang-format off
    return startReadyMade(L, {kPunchFields}, {"x", "y"}, [](Builder& builder, const TweenValue& current) {
        lua_State* state = builder.state;
        int vibrato = 10;
        float elasticity = 1.0F;
        if (builder.options != 0) {
            lua::Table::readField(state, builder.options, "vibrato", vibrato);
            lua::Table::readField(state, builder.options, "elasticity", elasticity);
        }
        TweenProperty property = TweenProperty::to(readLike(state, 3, current, "offset"));
        property.setMotion(TweenMotion::punch(vibrato, elasticity));
        return property;
    });
    // clang-format on
}

int TweenLua::timeline(lua_State* L) {
    int options = 0;
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {kCommonFields, kTimelineFields});
        options = 1;
    }
    const int owner = readOwner(L, options);
    const auto created = std::make_shared<Timeline>();
    configure(L, options, owner, *created);
    start(L, created, options, owner);
    return 1;
}

// Builds one tween per target with `make(target, index)` and places them in a new timeline, each one starting `each` seconds after the previous one.
int TweenLua::stagger(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    const auto each = lua::Stack::read<float>(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    int options = 0;
    Timeline::StaggerOrigin origin = Timeline::StaggerOrigin::Start;
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        lua::Table::checkFields(L, 4, {kCommonFields, kTimelineFields, kStaggerFields});
        lua::Table::readField(L, 4, "origin", origin);
        options = 4;
    }

    std::vector<std::shared_ptr<Tween>> tweens;
    const auto count = static_cast<lua_Integer>(lua_rawlen(L, 1));
    for (lua_Integer index = 1; index <= count; ++index) {
        lua_pushvalue(L, 3);
        lua_rawgeti(L, 1, index);
        lua_pushinteger(L, index);
        lua_call(L, 2, 1);
        tweens.push_back(check(L, -1));
        lua_pop(L, 1);
    }

    const int owner = readOwner(L, options);
    const auto created = std::make_shared<Timeline>();
    created->stagger(std::move(tweens), each, origin);
    configure(L, options, owner, *created);
    start(L, created, options, owner);
    return 1;
}

int TweenLua::killTag(lua_State* L) {
    lua::Runtime::getEngine(L).getTweens().killTag(lua::Stack::read<std::string_view>(L, 1));
    return 0;
}

int TweenLua::completeTag(lua_State* L) {
    lua::Runtime::getEngine(L).getTweens().completeTag(lua::Stack::read<std::string_view>(L, 1), lua_isnoneornil(L, 2) || lua_toboolean(L, 2) != 0);
    return 0;
}

int TweenLua::pauseTag(lua_State* L) {
    lua::Runtime::getEngine(L).getTweens().pauseTag(lua::Stack::read<std::string_view>(L, 1), true);
    return 0;
}

int TweenLua::resumeTag(lua_State* L) {
    lua::Runtime::getEngine(L).getTweens().pauseTag(lua::Stack::read<std::string_view>(L, 1), false);
    return 0;
}

int TweenLua::setTimeScale(lua_State* L) {
    lua::Runtime::getEngine(L).getTweens().setTimeScale(lua::Stack::read<std::string_view>(L, 1), lua::Stack::read<float>(L, 2));
    return 0;
}

int TweenLua::getTimeScale(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getTweens().getTimeScale(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int TweenLua::killTarget(lua_State* L) {
    luaL_checkany(L, 1);
    lua::Runtime::getEngine(L).getTweens().killTarget(lua_topointer(L, 1));
    return 0;
}

int TweenLua::killAll(lua_State* L) {
    lua::Runtime::getEngine(L).getTweens().killAll();
    return 0;
}

int TweenLua::size(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getTweens().size());
    return 1;
}

int TweenLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"to", &lua::Binding::native<&to>}, {"from", &lua::Binding::native<&from>}, {"by", &lua::Binding::native<&by>}, {"fromTo", &lua::Binding::native<&fromTo>}, {"move", &lua::Binding::native<&move>}, {"scale", &lua::Binding::native<&scale>}, {"rotate", &lua::Binding::native<&rotate>}, {"fade", &lua::Binding::native<&fade>}, {"tint", &lua::Binding::native<&tint>}, {"jump", &lua::Binding::native<&jump>}, {"path", &lua::Binding::native<&path>}, {"bezier", &lua::Binding::native<&bezier>}, {"blink", &lua::Binding::native<&blink>}, {"shake", &lua::Binding::native<&shake>}, {"punch", &lua::Binding::native<&punch>}, {"timeline", &lua::Binding::native<&timeline>}, {"stagger", &lua::Binding::native<&stagger>}, {"killTag", &lua::Binding::native<&killTag>}, {"completeTag", &lua::Binding::native<&completeTag>}, {"pauseTag", &lua::Binding::native<&pauseTag>}, {"resumeTag", &lua::Binding::native<&resumeTag>}, {"setTimeScale", &lua::Binding::native<&setTimeScale>}, {"timeScale", &lua::Binding::native<&getTimeScale>}, {"killTarget", &lua::Binding::native<&killTarget>}, {"killAll", &lua::Binding::native<&killAll>}, {"size", &size}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

int TweenLua::handlePlay(lua_State* L) {
    check(L, 1)->play();
    lua_settop(L, 1);
    return 1;
}

int TweenLua::handlePause(lua_State* L) {
    check(L, 1)->pause();
    lua_settop(L, 1);
    return 1;
}

int TweenLua::handleResume(lua_State* L) {
    check(L, 1)->resume();
    lua_settop(L, 1);
    return 1;
}

int TweenLua::handleRestart(lua_State* L) {
    check(L, 1)->restart();
    lua_settop(L, 1);
    return 1;
}

int TweenLua::handleReverse(lua_State* L) {
    check(L, 1)->reverse();
    lua_settop(L, 1);
    return 1;
}

// Seeks to seconds, or to a label of a timeline.
int TweenLua::handleSeek(lua_State* L) {
    const std::shared_ptr<Tween> tween = check(L, 1);
    if (lua_type(L, 2) == LUA_TSTRING) {
        checkTimeline(L, 1).seekLabel(lua::Stack::read<std::string_view>(L, 2));
    } else {
        tween->seek(lua::Stack::read<float>(L, 2));
    }
    lua_settop(L, 1);
    return 1;
}

int TweenLua::handleComplete(lua_State* L) {
    check(L, 1)->complete(lua_isnoneornil(L, 2) || lua_toboolean(L, 2) != 0);
    lua_settop(L, 1);
    return 1;
}

int TweenLua::handleKill(lua_State* L) {
    check(L, 1)->kill();
    lua_settop(L, 1);
    return 1;
}

// Returns a promise that resolves with `true` when the tween next completes and with `false` when it is killed first.
int TweenLua::handleWait(lua_State* L) {
    const std::shared_ptr<Tween> tween = check(L, 1);
    Engine& engine = lua::Runtime::getEngine(L);
    auto promise = std::make_shared<varn::async::Promise>(engine.getScriptRuntime());
    // clang-format off
    const auto settle = [promise](bool completed) {
        promise->resolveCustom([completed](lua_State* state) { lua_pushboolean(state, completed ? 1 : 0); });
    };
    // clang-format on
    if (!tween->isAlive()) {
        settle(tween->isCompleted());
    } else {
        (void)tween->finished.connect(settle, {.once = true});
    }
    varn::async::Promise::push(L, promise);
    return 1;
}

int TweenLua::handleAlive(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->isAlive());
    return 1;
}

int TweenLua::handlePlaying(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->isPlaying());
    return 1;
}

int TweenLua::handlePaused(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->isPaused());
    return 1;
}

int TweenLua::handleReversed(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->isReversed());
    return 1;
}

int TweenLua::handleCompleted(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->isCompleted());
    return 1;
}

int TweenLua::handleProgress(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->getProgress());
    return 1;
}

int TweenLua::handleSetProgress(lua_State* L) {
    check(L, 1)->setProgress(lua::Stack::read<float>(L, 3));
    return 0;
}

int TweenLua::handleTime(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->getTime());
    return 1;
}

int TweenLua::handleSetTime(lua_State* L) {
    check(L, 1)->seek(lua::Stack::read<float>(L, 3));
    return 0;
}

int TweenLua::handleTimeScale(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->getTimeScale());
    return 1;
}

int TweenLua::handleSetTimeScale(lua_State* L) {
    check(L, 1)->setTimeScale(lua::Stack::read<float>(L, 3));
    return 0;
}

int TweenLua::handleDuration(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->getDuration());
    return 1;
}

int TweenLua::handleTotalDuration(lua_State* L) {
    const float total = check(L, 1)->getTotalDuration();
    lua_pushnumber(L, std::isinf(total) ? HUGE_VAL : static_cast<lua_Number>(total));
    return 1;
}

int TweenLua::handleDelay(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->getDelay());
    return 1;
}

int TweenLua::handleTag(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->getTag());
    return 1;
}

std::function<void()> TweenLua::makeCall(lua_State* L, int index) {
    auto function = std::make_shared<lua::Reference>(L, index);
    // clang-format off
    return [function] {
        lua_State* main = function->getState();
        function->push(main);
        lua::Runtime::protectedCall(main, 0, 0);
    };
    // clang-format on
}

// Places an item at the end of the timeline: a tween or timeline handle, a number of seconds to wait or a function to call.
int TweenLua::timelineAppend(lua_State* L) {
    Timeline& target = checkTimeline(L, 1);
    if (lua_type(L, 2) == LUA_TNUMBER) {
        target.appendInterval(lua::Stack::read<float>(L, 2));
    } else if (lua_isfunction(L, 2)) {
        target.appendCall(makeCall(L, 2));
    } else {
        target.append(check(L, 2));
    }
    lua_settop(L, 1);
    return 1;
}

int TweenLua::timelineJoin(lua_State* L) {
    Timeline& target = checkTimeline(L, 1);
    if (lua_isfunction(L, 2)) {
        target.joinCall(makeCall(L, 2));
    } else {
        target.join(check(L, 2));
    }
    lua_settop(L, 1);
    return 1;
}

// Places an item at seconds or at a label: a tween or timeline handle or a function to call.
int TweenLua::timelineInsert(lua_State* L) {
    Timeline& target = checkTimeline(L, 1);
    const float time = lua_type(L, 2) == LUA_TSTRING ? target.getLabelTime(lua::Stack::read<std::string_view>(L, 2)) : lua::Stack::read<float>(L, 2);
    if (lua_isfunction(L, 3)) {
        target.insertCall(time, makeCall(L, 3));
    } else {
        target.insert(time, check(L, 3));
    }
    lua_settop(L, 1);
    return 1;
}

int TweenLua::timelineAddLabel(lua_State* L) {
    Timeline& target = checkTimeline(L, 1);
    std::string name = lua::Stack::read<std::string>(L, 2);
    if (lua_isnoneornil(L, 3)) {
        target.addLabel(std::move(name));
    } else {
        target.addLabel(std::move(name), lua::Stack::read<float>(L, 3));
    }
    lua_settop(L, 1);
    return 1;
}

int TweenLua::timelineSize(lua_State* L) {
    lua::Stack::push(L, checkTimeline(L, 1).size());
    return 1;
}

template <typename T> void TweenLua::installHandle(lua_State* L, bool withTimeline) {
    lua::ClassBuilder<T> builder(L);
    builder.function("play", &lua::Binding::native<&handlePlay>).function("pause", &lua::Binding::native<&handlePause>).function("resume", &lua::Binding::native<&handleResume>).function("restart", &lua::Binding::native<&handleRestart>).function("reverse", &lua::Binding::native<&handleReverse>).function("seek", &lua::Binding::native<&handleSeek>).function("complete", &lua::Binding::native<&handleComplete>).function("kill", &lua::Binding::native<&handleKill>).function("wait", &lua::Binding::native<&handleWait>);
    builder.property("alive", &handleAlive).property("playing", &handlePlaying).property("paused", &handlePaused).property("reversed", &handleReversed).property("completed", &handleCompleted).property("progress", &handleProgress, &lua::Binding::native<&handleSetProgress>).property("time", &handleTime, &lua::Binding::native<&handleSetTime>).property("timeScale", &handleTimeScale, &lua::Binding::native<&handleSetTimeScale>).property("duration", &handleDuration).property("totalDuration", &handleTotalDuration).property("delay", &handleDelay).property("tag", &handleTag);
    if (withTimeline) {
        builder.function("append", &lua::Binding::native<&timelineAppend>).function("join", &lua::Binding::native<&timelineJoin>).function("insert", &lua::Binding::native<&timelineInsert>).function("addLabel", &lua::Binding::native<&timelineAddLabel>).property("size", &timelineSize);
    }
    builder.install();
}

void TweenLua::install(lua_State* L) {
    installHandle<Tween>(L, false);
    installHandle<Timeline>(L, true);
    lua::Binding::preload(L, "haylen.tween", &open);
}

} // namespace haylen::core
