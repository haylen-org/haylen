#include "2d/animation/SkeletonBinding.hpp"

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
#include "haylen/plugins/Animation2DPlugin.hpp"
#include "runtime/Owners.hpp"

namespace haylen::lua::animation2d {

void SkeletonBinding::pushData(lua_State* L, std::shared_ptr<haylen::animation2d::SkeletonData> data) {
    lua::Userdata::emplace<haylen::animation2d::SkeletonData>(L, std::move(data));
}

int SkeletonBinding::newData(lua_State* L) {
    const haylen::core::Json description = lua::JsonConverter::read(L, 1);
    const std::shared_ptr<haylen::animation2d::SpriteAtlas>& atlas = lua::Userdata::checkShared<haylen::animation2d::SpriteAtlas>(L, 2);
    pushData(L, std::make_shared<haylen::animation2d::SkeletonData>(haylen::animation2d::SkeletonData::parse(description, atlas)));
    return 1;
}

int SkeletonBinding::newSkeleton(lua_State* L) {
    auto skeleton = std::make_shared<haylen::animation2d::Skeleton>(lua::Userdata::checkShared<haylen::animation2d::SkeletonData>(L, 1));
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
    lua::Runtime::getEngine(L).getPlugin<haylen::plugins::Animation2DPlugin>().track(skeleton);
    lua::Userdata::emplace<haylen::animation2d::Skeleton>(L, std::move(skeleton));
    return 1;
}

haylen::animation2d::Skeleton& SkeletonBinding::check(lua_State* L) {
    return lua::Userdata::check<haylen::animation2d::Skeleton>(L, 1);
}

std::size_t SkeletonBinding::readTrack(lua_State* L, int index) {
    const lua_Integer track = luaL_optinteger(L, index, 1);
    luaL_argcheck(L, track >= 1, index, "tracks are numbered from one");
    return static_cast<std::size_t>(track - 1);
}

// Tracks count from one in Lua, as in the options table `{track = 1, loop = true, fade = 0}`.
haylen::animation2d::Skeleton::PlayOptions SkeletonBinding::readPlayOptions(lua_State* L, int index) {
    haylen::animation2d::Skeleton::PlayOptions options;
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

int SkeletonBinding::dataNames(lua_State* L, int kind) {
    const haylen::animation2d::SkeletonData& data = lua::Userdata::check<haylen::animation2d::SkeletonData>(L, 1);
    std::vector<std::string> names;
    switch (kind) {
    case 0:
        for (const haylen::animation2d::SkeletonData::Bone& entry : data.getBones()) {
            names.push_back(entry.name);
        }

        break;
    case 1:
        for (const haylen::animation2d::SkeletonData::Slot& entry : data.getSlots()) {
            names.push_back(entry.name);
        }

        break;
    case 2:
        for (const haylen::animation2d::SkeletonData::Skin& entry : data.getSkins()) {
            names.push_back(entry.name);
        }

        break;
    default:
        for (const haylen::animation2d::SkeletonAnimation& entry : data.getAnimations()) {
            names.push_back(entry.name);
        }

        break;
    }

    lua::Stack::push(L, names);
    return 1;
}

int SkeletonBinding::dataBones(lua_State* L) {
    return dataNames(L, 0);
}

int SkeletonBinding::dataSlots(lua_State* L) {
    return dataNames(L, 1);
}

int SkeletonBinding::dataSkins(lua_State* L) {
    return dataNames(L, 2);
}

int SkeletonBinding::dataAnimations(lua_State* L) {
    return dataNames(L, 3);
}

int SkeletonBinding::dataEvents(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::animation2d::SkeletonData>(L, 1).getEvents());
    return 1;
}

int SkeletonBinding::dataDuration(lua_State* L) {
    const std::string_view name = lua::Stack::read<std::string_view>(L, 2);
    const haylen::animation2d::SkeletonAnimation* animation = lua::Userdata::check<haylen::animation2d::SkeletonData>(L, 1).findAnimation(name);
    if (animation == nullptr) {
        return luaL_error(L, "The skeleton has no animation \"%s\".", std::string(name).c_str());
    }

    lua::Stack::push(L, animation->duration);
    return 1;
}

int SkeletonBinding::play(lua_State* L) {
    check(L).play(lua::Stack::read<std::string_view>(L, 2), readPlayOptions(L, 3));
    return 0;
}

// Crossfades with `crossFade(name, seconds, {track, loop})`.
int SkeletonBinding::crossFade(lua_State* L) {
    haylen::animation2d::Skeleton::PlayOptions options = readPlayOptions(L, 4);
    options.fade = lua::Stack::read<float>(L, 3);
    check(L).play(lua::Stack::read<std::string_view>(L, 2), options);
    return 0;
}

int SkeletonBinding::stop(lua_State* L) {
    check(L).stop(readTrack(L, 2));
    return 0;
}

int SkeletonBinding::setTrackWeight(lua_State* L) {
    check(L).setTrackWeight(readTrack(L, 2), lua::Stack::read<float>(L, 3));
    return 0;
}

int SkeletonBinding::trackAnimation(lua_State* L) {
    const std::string_view name = check(L).getTrackAnimation(readTrack(L, 2));
    if (name.empty()) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, name);
    return 1;
}

int SkeletonBinding::trackTime(lua_State* L) {
    lua::Stack::push(L, check(L).getTrackTime(readTrack(L, 2)));
    return 1;
}

int SkeletonBinding::getSkin(lua_State* L) {
    lua::Stack::push(L, check(L).getSkin());
    return 1;
}

int SkeletonBinding::setSkin(lua_State* L) {
    check(L).setSkin(lua::Stack::read<std::string_view>(L, 3));
    return 0;
}

int SkeletonBinding::setAttachment(lua_State* L) {
    check(L).setAttachment(lua::Stack::read<std::string_view>(L, 2), lua_isnoneornil(L, 3) ? std::string_view() : lua::Stack::read<std::string_view>(L, 3));
    return 0;
}

// Describes a bone with its local pose and its place, rotation and scale in the world, as of the last pose.
int SkeletonBinding::bone(lua_State* L) {
    haylen::animation2d::Skeleton& skeleton = check(L);
    const std::string_view name = lua::Stack::read<std::string_view>(L, 2);
    const std::optional<std::size_t> found = skeleton.findBone(name);
    if (!found) {
        return luaL_error(L, "The skeleton has no bone \"%s\".", std::string(name).c_str());
    }

    const haylen::math::Transform2D& world = skeleton.getBoneWorld(*found);
    const haylen::animation2d::Skeleton::BonePose& local = skeleton.getBonePose(*found);
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
        {"worldScaleX", haylen::math::Vec2{world.a, world.b}.getLength()},
    }};

    for (const auto& [key, value] : fields) {
        lua::Stack::push(L, value);
        lua_setfield(L, -2, key);
    }

    return 1;
}

int SkeletonBinding::setIkTarget(lua_State* L) {
    check(L).setIkTarget(lua::Stack::read<std::string_view>(L, 2), {lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)});
    return 0;
}

int SkeletonBinding::clearIkTarget(lua_State* L) {
    check(L).clearIkTarget(lua::Stack::read<std::string_view>(L, 2));
    return 0;
}

int SkeletonBinding::setIkMix(lua_State* L) {
    check(L).setIkMix(lua::Stack::read<std::string_view>(L, 2), lua::Stack::read<float>(L, 3));
    return 0;
}

int SkeletonBinding::update(lua_State* L) {
    haylen::animation2d::Skeleton& skeleton = check(L);
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

int SkeletonBinding::pose(lua_State* L) {
    check(L).pose();
    return 0;
}

int SkeletonBinding::point(lua_State* L) {
    const std::optional<haylen::math::Vec2> found = check(L).getPoint(lua::Stack::read<std::string_view>(L, 2));
    if (!found) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, found->x);
    lua::Stack::push(L, found->y);
    return 2;
}

int SkeletonBinding::bounds(lua_State* L) {
    lua::Stack::push(L, check(L).getBounds());
    return 1;
}

int SkeletonBinding::draw(lua_State* L) {
    check(L).draw(lua::Runtime::getEngine(L).getRenderer2D(), lua::TypeConverter::readDrawOrder(L, 2));
    return 0;
}

int SkeletonBinding::data(lua_State* L) {
    pushData(L, std::const_pointer_cast<haylen::animation2d::SkeletonData>(lua::Userdata::checkShared<haylen::animation2d::Skeleton>(L, 1)->getDataPointer()));
    return 1;
}

// A skeleton that fires events while Lua updates it calls the listener on that call, so an error reaches its caller, and otherwise the main thread reports the error.
void SkeletonBinding::notifyEvent(const lua::Owners::Function& function, lua_State* main, const haylen::animation2d::Skeleton::Event& fired) {
    if (updating != nullptr) {
        callEvent(function, updating, fired, false);
        return;
    }

    lua::Runtime::runReporting(main, [&] { callEvent(function, main, fired, true); });
}

void SkeletonBinding::callEvent(const lua::Owners::Function& function, lua_State* state, const haylen::animation2d::Skeleton::Event& fired, bool reported) {
    if (!function.push(state)) {
        return;
    }

    lua_createtable(state, 0, 5);
    lua::Stack::push(state, fired.name);
    lua_setfield(state, -2, "name");
    lua::Stack::push(state, fired.track + 1);
    lua_setfield(state, -2, "track");
    lua::Stack::push(state, fired.animation);
    lua_setfield(state, -2, "animation");
    lua::Stack::push(state, fired.text);
    lua_setfield(state, -2, "text");
    lua::Stack::push(state, fired.number);
    lua_setfield(state, -2, "number");

    if (reported) {
        lua::Runtime::protectedCall(state, 1, 0);
        return;
    }

    lua_call(state, 1, 0);
}

// Listens with `on('event', function, {owner = owner})`, called with the name, the track counted from one, the animation, the text and the number of every event the tracks pass. A listener ends with its owner, and without one it takes the default owner.
int SkeletonBinding::on(lua_State* L) {
    haylen::animation2d::Skeleton& skeleton = check(L);
    const std::string_view event = lua::Stack::read<std::string_view>(L, 2);
    if (event != "event") {
        return luaL_error(L, "Unknown skeleton event \"%s\". Skeletons report \"event\".", std::string(event).c_str());
    }

    const lua::Owners::Listener listener = lua::Owners::readListener(L, 3, 4);
    lua_State* main = lua::Runtime::getMainThread(L);

    // clang-format off
    haylen::core::Connection connection = skeleton.eventFired.connect([function = listener.function, main](const haylen::animation2d::Skeleton::Event& fired) {
        notifyEvent(*function, main, fired);
    }, {.owner = listener.lifetime});
    // clang-format on
    lua::Owners::pushConnection(L, listener, std::move(connection));
    return 1;
}

void SkeletonBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::animation2d::SkeletonData>(L).property("bones", &lua::Binding::native<&dataBones>).property("slots", &lua::Binding::native<&dataSlots>).property("skins", &lua::Binding::native<&dataSkins>).property("animations", &lua::Binding::native<&dataAnimations>).property("events", &lua::Binding::native<&dataEvents>).function("duration", &lua::Binding::native<&dataDuration>).meta("__eq", &lua::Userdata::equal<haylen::animation2d::SkeletonData>).install();

    lua::ClassBuilder<haylen::animation2d::Skeleton>(L).nestedField<&haylen::animation2d::Skeleton::position, &haylen::math::Vec2::x>("x").nestedField<&haylen::animation2d::Skeleton::position, &haylen::math::Vec2::y>("y").field<&haylen::animation2d::Skeleton::rotation>("rotation").nestedField<&haylen::animation2d::Skeleton::scale, &haylen::math::Vec2::x>("scaleX").nestedField<&haylen::animation2d::Skeleton::scale, &haylen::math::Vec2::y>("scaleY").field<&haylen::animation2d::Skeleton::color>("color").property("skin", &lua::Binding::native<&getSkin>, &lua::Binding::native<&setSkin>).property("data", &lua::Binding::native<&data>).function("play", &lua::Binding::native<&play>).function("crossFade", &lua::Binding::native<&crossFade>).function("stop", &lua::Binding::native<&stop>).function("setTrackWeight", &lua::Binding::native<&setTrackWeight>).function("trackAnimation", &lua::Binding::native<&trackAnimation>).function("trackTime", &lua::Binding::native<&trackTime>).function("setAttachment", &lua::Binding::native<&setAttachment>).function("bone", &lua::Binding::native<&bone>).function("setIkTarget", &lua::Binding::native<&setIkTarget>).function("clearIkTarget", &lua::Binding::native<&clearIkTarget>).function("setIkMix", &lua::Binding::native<&setIkMix>).function("update", &lua::Binding::native<&update>).function("pose", &lua::Binding::native<&pose>).function("point", &lua::Binding::native<&point>).function("bounds", &lua::Binding::native<&bounds>).function("draw", &lua::Binding::native<&draw>).function("on", &lua::Binding::native<&on>).install();
}

} // namespace haylen::lua::animation2d
