#include "2d/animation/SkeletonLua.hpp"

#include <string>
#include <utility>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "lua/Owners.hpp"

namespace haylen::animation2d {

void SkeletonLua::pushData(lua_State* L, std::shared_ptr<SkeletonData> data) {
    lua::Userdata::emplace<SkeletonData>(L, std::move(data));
}

int SkeletonLua::newData(lua_State* L) {
    const core::Json description = lua::JsonConverter::read(L, 1);
    const std::shared_ptr<SpriteAtlas>& atlas = lua::Userdata::checkShared<SpriteAtlas>(L, 2);
    pushData(L, std::make_shared<SkeletonData>(SkeletonData::parse(description, atlas)));
    return 1;
}

int SkeletonLua::newSkeleton(lua_State* L) {
    auto skeleton = std::make_shared<Skeleton>(lua::Userdata::checkShared<SkeletonData>(L, 1));
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kSkeletonFields});
        std::string skin;
        lua::Table::readField(L, 2, "skin", skin);
        if (!skin.empty()) {
            skeleton->setSkin(skin);
        }
        lua::Table::readField(L, 2, "x", skeleton->position.x);
        lua::Table::readField(L, 2, "y", skeleton->position.y);
        lua::Table::readField(L, 2, "rotation", skeleton->rotation);
        lua::Table::readField(L, 2, "scaleX", skeleton->scale.x);
        lua::Table::readField(L, 2, "scaleY", skeleton->scale.y);
        lua::Table::readField(L, 2, "color", skeleton->color);
    }
    skeleton->pose();
    lua::Userdata::emplace<Skeleton>(L, std::move(skeleton));
    return 1;
}

Skeleton& SkeletonLua::check(lua_State* L) {
    return lua::Userdata::check<Skeleton>(L, 1);
}

std::size_t SkeletonLua::readTrack(lua_State* L, int index) {
    const lua_Integer track = luaL_optinteger(L, index, 1);
    luaL_argcheck(L, track >= 1, index, "tracks are numbered from one");
    return static_cast<std::size_t>(track - 1);
}

// Tracks count from one in Lua, as in the options table `{track = 1, loop = true, fade = 0}`.
Skeleton::PlayOptions SkeletonLua::readPlayOptions(lua_State* L, int index) {
    Skeleton::PlayOptions options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kPlayFields});
    lua_Integer track = 1;
    lua::Table::readField(L, index, "track", track);
    luaL_argcheck(L, track >= 1, index, "tracks are numbered from one");
    options.track = static_cast<std::size_t>(track - 1);
    lua::Table::readField(L, index, "loop", options.loop);
    lua::Table::readField(L, index, "fade", options.fade);
    return options;
}

int SkeletonLua::dataNames(lua_State* L, int kind) {
    const SkeletonData& data = lua::Userdata::check<SkeletonData>(L, 1);
    std::vector<std::string> names;
    switch (kind) {
    case 0:
        for (const SkeletonData::Bone& entry : data.getBones()) {
            names.push_back(entry.name);
        }
        break;
    case 1:
        for (const SkeletonData::Slot& entry : data.getSlots()) {
            names.push_back(entry.name);
        }
        break;
    case 2:
        for (const SkeletonData::Skin& entry : data.getSkins()) {
            names.push_back(entry.name);
        }
        break;
    default:
        for (const SkeletonAnimation& entry : data.getAnimations()) {
            names.push_back(entry.name);
        }
        break;
    }
    lua::Stack::push(L, names);
    return 1;
}

int SkeletonLua::dataBones(lua_State* L) {
    return dataNames(L, 0);
}

int SkeletonLua::dataSlots(lua_State* L) {
    return dataNames(L, 1);
}

int SkeletonLua::dataSkins(lua_State* L) {
    return dataNames(L, 2);
}

int SkeletonLua::dataAnimations(lua_State* L) {
    return dataNames(L, 3);
}

int SkeletonLua::dataEvents(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<SkeletonData>(L, 1).getEvents());
    return 1;
}

int SkeletonLua::dataDuration(lua_State* L) {
    const std::string_view name = lua::Stack::read<std::string_view>(L, 2);
    const SkeletonAnimation* animation = lua::Userdata::check<SkeletonData>(L, 1).findAnimation(name);
    if (animation == nullptr) {
        return luaL_error(L, "The skeleton has no animation \"%s\".", std::string(name).c_str());
    }
    lua::Stack::push(L, animation->duration);
    return 1;
}

int SkeletonLua::play(lua_State* L) {
    check(L).play(lua::Stack::read<std::string_view>(L, 2), readPlayOptions(L, 3));
    return 0;
}

// Crossfades with `crossFade(name, seconds, {track, loop})`.
int SkeletonLua::crossFade(lua_State* L) {
    Skeleton::PlayOptions options = readPlayOptions(L, 4);
    options.fade = lua::Stack::read<float>(L, 3);
    check(L).play(lua::Stack::read<std::string_view>(L, 2), options);
    return 0;
}

int SkeletonLua::stop(lua_State* L) {
    check(L).stop(readTrack(L, 2));
    return 0;
}

int SkeletonLua::setTrackWeight(lua_State* L) {
    check(L).setTrackWeight(readTrack(L, 2), lua::Stack::read<float>(L, 3));
    return 0;
}

int SkeletonLua::trackAnimation(lua_State* L) {
    const std::string_view name = check(L).getTrackAnimation(readTrack(L, 2));
    if (name.empty()) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, name);
    return 1;
}

int SkeletonLua::trackTime(lua_State* L) {
    lua::Stack::push(L, check(L).getTrackTime(readTrack(L, 2)));
    return 1;
}

int SkeletonLua::getSkin(lua_State* L) {
    lua::Stack::push(L, check(L).getSkin());
    return 1;
}

int SkeletonLua::setSkin(lua_State* L) {
    check(L).setSkin(lua::Stack::read<std::string_view>(L, 3));
    return 0;
}

int SkeletonLua::setAttachment(lua_State* L) {
    check(L).setAttachment(lua::Stack::read<std::string_view>(L, 2), lua_isnoneornil(L, 3) ? std::string_view() : lua::Stack::read<std::string_view>(L, 3));
    return 0;
}

// Describes a bone with its local pose and its place, rotation and scale in the world, as of the last pose.
int SkeletonLua::bone(lua_State* L) {
    Skeleton& skeleton = check(L);
    const std::string_view name = lua::Stack::read<std::string_view>(L, 2);
    const std::optional<std::size_t> found = skeleton.findBone(name);
    if (!found) {
        return luaL_error(L, "The skeleton has no bone \"%s\".", std::string(name).c_str());
    }
    const math::Transform2D& world = skeleton.getBoneWorld(*found);
    const Skeleton::BonePose& local = skeleton.getBonePose(*found);
    lua_createtable(L, 0, 9);
    const std::array<std::pair<const char*, float>, 9> fields{{
        {"x", local.position.x},
        {"y", local.position.y},
        {"rotation", local.rotation},
        {"scaleX", local.scale.x},
        {"scaleY", local.scale.y},
        {"worldX", world.tx},
        {"worldY", world.ty},
        {"worldRotation", std::atan2(world.b, world.a)},
        {"worldScaleX", math::Vec2{world.a, world.b}.getLength()},
    }};
    for (const auto& [key, value] : fields) {
        lua::Stack::push(L, value);
        lua_setfield(L, -2, key);
    }
    return 1;
}

int SkeletonLua::setIkTarget(lua_State* L) {
    check(L).setIkTarget(lua::Stack::read<std::string_view>(L, 2), {lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)});
    return 0;
}

int SkeletonLua::clearIkTarget(lua_State* L) {
    check(L).clearIkTarget(lua::Stack::read<std::string_view>(L, 2));
    return 0;
}

int SkeletonLua::setIkMix(lua_State* L) {
    check(L).setIkMix(lua::Stack::read<std::string_view>(L, 2), lua::Stack::read<float>(L, 3));
    return 0;
}

int SkeletonLua::update(lua_State* L) {
    Skeleton& skeleton = check(L);
    const float delta = lua::Stack::read<float>(L, 2);
    struct Restore {
        lua_State* previous;
        ~Restore() {
            updating = previous;
        }
    } restore{updating};
    updating = L;
    skeleton.update(delta);
    return 0;
}

int SkeletonLua::pose(lua_State* L) {
    check(L).pose();
    return 0;
}

int SkeletonLua::point(lua_State* L) {
    const std::optional<math::Vec2> found = check(L).getPoint(lua::Stack::read<std::string_view>(L, 2));
    if (!found) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, found->x);
    lua::Stack::push(L, found->y);
    return 2;
}

int SkeletonLua::bounds(lua_State* L) {
    lua::Stack::push(L, check(L).getBounds());
    return 1;
}

int SkeletonLua::draw(lua_State* L) {
    check(L).draw(lua::Runtime::getEngine(L).getRenderer2D(), lua::TypeConverter::readDrawOrder(L, 2));
    return 0;
}

int SkeletonLua::data(lua_State* L) {
    pushData(L, std::const_pointer_cast<SkeletonData>(lua::Userdata::checkShared<Skeleton>(L, 1)->getDataPointer()));
    return 1;
}

// Listens with `on('event', function, {owner = owner})`, called with the name, the track counted from one, the animation, the text and the number of every event the tracks pass. A listener ends with its owner, and without one it takes the default owner.
int SkeletonLua::on(lua_State* L) {
    Skeleton& skeleton = check(L);
    const std::string_view event = lua::Stack::read<std::string_view>(L, 2);
    if (event != "event") {
        return luaL_error(L, "Unknown skeleton event \"%s\". Skeletons report \"event\".", std::string(event).c_str());
    }
    luaL_checktype(L, 3, LUA_TFUNCTION);
    int owner = 0;
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        lua::Table::checkFields(L, 4, {kListenerFields});
        if (lua_getfield(L, 4, "owner") != LUA_TNIL) {
            lua::Owners::checkOwner(L, -1);
            owner = lua_gettop(L);
        }
    }
    if (owner == 0) {
        owner = lua::Owners::pushDefault(L);
    }
    const std::weak_ptr<const void> lifetime = owner != 0 ? lua::Owners::getLifetime(L, owner) : std::weak_ptr<const void>();
    auto function = std::make_shared<lua::Owners::Function>(L, 3, owner);
    lua_State* main = lua::Runtime::getMainThread(L);

    // clang-format off
    core::Connection connection = skeleton.eventFired.connect([function, main](const Skeleton::Event& fired) {
        const auto call = [&](lua_State* state, bool reported) {
            if (!function->push(state)) {
                return;
            }
            lua::Stack::push(state, fired.name);
            lua::Stack::push(state, fired.track + 1);
            lua::Stack::push(state, fired.animation);
            lua::Stack::push(state, fired.text);
            lua::Stack::push(state, fired.number);
            if (reported) {
                lua::Runtime::protectedCall(state, 5, 0);
            } else {
                lua_call(state, 5, 0);
            }
        };
        if (updating != nullptr) {
            call(updating, false);
            return;
        }
        lua::Runtime::runReporting(main, [&] { call(main, true); });
    }, {.owner = lifetime});
    // clang-format on
    if (owner != 0) {
        lua::Owners::add(L, owner, connection);
    }
    lua::Userdata::emplace<core::Connection>(L, std::move(connection));
    return 1;
}

void SkeletonLua::install(lua_State* L) {
    lua::ClassBuilder<SkeletonData>(L).property("bones", &lua::Binding::native<&dataBones>).property("slots", &lua::Binding::native<&dataSlots>).property("skins", &lua::Binding::native<&dataSkins>).property("animations", &lua::Binding::native<&dataAnimations>).property("events", &lua::Binding::native<&dataEvents>).function("duration", &lua::Binding::native<&dataDuration>).meta("__eq", &lua::Userdata::equal<SkeletonData>).install();

    lua::ClassBuilder<Skeleton>(L).nestedField<&Skeleton::position, &math::Vec2::x>("x").nestedField<&Skeleton::position, &math::Vec2::y>("y").field<&Skeleton::rotation>("rotation").nestedField<&Skeleton::scale, &math::Vec2::x>("scaleX").nestedField<&Skeleton::scale, &math::Vec2::y>("scaleY").field<&Skeleton::color>("color").property("skin", &lua::Binding::native<&getSkin>, &lua::Binding::native<&setSkin>).property("data", &lua::Binding::native<&data>).function("play", &lua::Binding::native<&play>).function("crossFade", &lua::Binding::native<&crossFade>).function("stop", &lua::Binding::native<&stop>).function("setTrackWeight", &lua::Binding::native<&setTrackWeight>).function("trackAnimation", &lua::Binding::native<&trackAnimation>).function("trackTime", &lua::Binding::native<&trackTime>).function("setAttachment", &lua::Binding::native<&setAttachment>).function("bone", &lua::Binding::native<&bone>).function("setIkTarget", &lua::Binding::native<&setIkTarget>).function("clearIkTarget", &lua::Binding::native<&clearIkTarget>).function("setIkMix", &lua::Binding::native<&setIkMix>).function("update", &lua::Binding::native<&update>).function("pose", &lua::Binding::native<&pose>).function("point", &lua::Binding::native<&point>).function("bounds", &lua::Binding::native<&bounds>).function("draw", &lua::Binding::native<&draw>).function("on", &lua::Binding::native<&on>).install();
}

} // namespace haylen::animation2d
