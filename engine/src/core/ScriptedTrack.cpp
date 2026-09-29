#include "core/ScriptedTrack.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::core {

ScriptedTrack::ScriptedTrack(lua_State* L, int value) : state(lua::Runtime::getMainThread(L)), target(L, value) {}

std::string ScriptedTrack::join(const Path& path) {
    std::string name;
    for (const std::string& part : path) {
        name += name.empty() ? part : "." + part;
    }
    return name;
}

std::vector<ScriptedTrack::Path> ScriptedTrack::split(const std::vector<std::string>& names) {
    std::vector<Path> paths;
    for (const std::string& name : names) {
        Path path;
        std::size_t start = 0;
        while (true) {
            const std::size_t dot = name.find('.', start);
            path.push_back(name.substr(start, dot == std::string::npos ? std::string::npos : dot - start));
            if (path.back().empty()) {
                throw std::invalid_argument("The tween field '" + name + "' has an empty part.");
            }
            if (dot == std::string::npos) {
                break;
            }
            start = dot + 1;
        }
        paths.push_back(std::move(path));
    }
    return paths;
}

void ScriptedTrack::add(const std::vector<std::string>& names, TweenProperty property) {
    fields.push_back({.names = names, .paths = split(names), .property = std::move(property)});
}

void ScriptedTrack::checkIndexable(lua_State* L, int index, const Path& path) {
    const int type = lua_type(L, index);
    if (type != LUA_TTABLE && type != LUA_TUSERDATA) {
        luaL_error(L, "Cannot tween the field '%s' because part of its path is %s, not a table or an object.", join(path).c_str(), luaL_typename(L, index));
    }
}

void ScriptedTrack::pushPath(lua_State* L, int object, const Path& path) {
    lua_pushvalue(L, object);
    for (const std::string& part : path) {
        checkIndexable(L, -1, path);
        lua_getfield(L, -1, part.c_str());
        lua_remove(L, -2);
    }
}

TweenValue ScriptedTrack::toValue(lua_State* L, int index, const Path& path) {
    if (lua_type(L, index) == LUA_TNUMBER) {
        return lua_tonumber(L, index);
    }
    if (const math::Vec2* vector = lua::Userdata::test<math::Vec2>(L, index)) {
        return *vector;
    }
    if (const math::Color* color = lua::Userdata::test<math::Color>(L, index)) {
        return *color;
    }
    if (lua_type(L, index) == LUA_TSTRING) {
        return lua::Stack::read<std::string>(L, index);
    }
    luaL_error(L, "Cannot tween the field '%s' because it is not a number, a Vec2, a Color or a text.", join(path).c_str());
    return {};
}

TweenValue ScriptedTrack::read(lua_State* L, int object, const std::vector<Path>& paths) {
    const int index = lua_absindex(L, object);
    if (paths.size() == 1) {
        pushPath(L, index, paths.front());
        TweenValue value = toValue(L, -1, paths.front());
        lua_pop(L, 1);
        return value;
    }

    // Two paths hold the components of one Vec2, such as x and y.
    math::Vec2 components;
    for (std::size_t axis = 0; axis < 2; ++axis) {
        pushPath(L, index, paths[axis]);
        if (lua_type(L, -1) != LUA_TNUMBER) {
            luaL_error(L, "Cannot tween the field '%s' as part of a Vec2 because it is not a number.", join(paths[axis]).c_str());
        }
        (axis == 0 ? components.x : components.y) = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    return components;
}

void ScriptedTrack::pushValue(lua_State* L, const TweenValue& value) {
    switch (value.getKind()) {
    case TweenValue::Kind::Number:
        lua_pushnumber(L, value.getNumber());
        return;
    case TweenValue::Kind::Vector:
        lua::Stack::push(L, value.getVector());
        return;
    case TweenValue::Kind::Color:
        lua::Stack::push(L, value.getColor());
        return;
    case TweenValue::Kind::Text:
        lua::Stack::push(L, value.getText());
        return;
    }
}

// A path that ends inside a Vec2 or a Color, such as position.x, writes a changed copy back to the field that holds it, since those values are copies in Lua and changing one in place would change every field that shares it.
void ScriptedTrack::write(lua_State* L, int object, const Path& path, const TweenValue& value) {
    if (path.size() == 1) {
        pushValue(L, value);
        lua_setfield(L, object, path.front().c_str());
        return;
    }

    lua_pushvalue(L, object);
    for (std::size_t part = 0; part + 2 < path.size(); ++part) {
        checkIndexable(L, -1, path);
        lua_getfield(L, -1, path[part].c_str());
        lua_remove(L, -2);
    }
    checkIndexable(L, -1, path);
    const int holder = lua_gettop(L);
    const std::string& container = path[path.size() - 2];
    const std::string& component = path.back();
    lua_getfield(L, holder, container.c_str());

    const auto amount = static_cast<float>(value.getKind() == TweenValue::Kind::Number ? value.getNumber() : 0.0);
    if (const math::Vec2* vector = lua::Userdata::test<math::Vec2>(L, -1)) {
        math::Vec2 copy = *vector;
        if (component != "x" && component != "y") {
            luaL_error(L, "Cannot tween the field '%s' because a Vec2 has only x and y.", join(path).c_str());
        }
        (component == "x" ? copy.x : copy.y) = amount;
        lua_pop(L, 1);
        lua::Stack::push(L, copy);
        lua_setfield(L, holder, container.c_str());
    } else if (const math::Color* color = lua::Userdata::test<math::Color>(L, -1)) {
        math::Color copy = *color;
        float* channel = component == "r" ? &copy.r : component == "g" ? &copy.g : component == "b" ? &copy.b : component == "a" ? &copy.a : nullptr;
        if (channel == nullptr) {
            luaL_error(L, "Cannot tween the field '%s' because a Color has only r, g, b and a.", join(path).c_str());
        }
        *channel = amount;
        lua_pop(L, 1);
        lua::Stack::push(L, copy);
        lua_setfield(L, holder, container.c_str());
    } else {
        checkIndexable(L, -1, path);
        pushValue(L, value);
        lua_setfield(L, -2, component.c_str());
        lua_pop(L, 1);
    }
    lua_settop(L, holder - 1);
}

void ScriptedTrack::begin() {
    // clang-format off
    lua::Runtime::protectedRun(state, [this](lua_State* L) {
        if (!target.push(L)) {
            return;
        }
        const int index = lua_gettop(L);
        for (Field& field : fields) {
            field.property.begin(read(L, index, field.paths));
        }
        lua_pop(L, 1);
    });
    // clang-format on
}

void ScriptedTrack::render(float progress, int loops) {
    // clang-format off
    lua::Runtime::protectedRun(state, [this, progress, loops](lua_State* L) {
        if (!target.push(L)) {
            return;
        }
        const int index = lua_gettop(L);
        for (const Field& field : fields) {
            if (field.released) {
                continue;
            }
            const TweenValue value = field.property.evaluate(progress, loops);
            if (field.paths.size() == 1) {
                write(L, index, field.paths.front(), value);
                continue;
            }
            const math::Vec2 components = value.getVector();
            write(L, index, field.paths[0], static_cast<double>(components.x));
            write(L, index, field.paths[1], static_cast<double>(components.y));
        }
        lua_pop(L, 1);
    });
    // clang-format on
}

bool ScriptedTrack::isAlive() const {
    return target.isAlive();
}

float ScriptedTrack::getDistance() const {
    float distance = 0.0F;
    for (const Field& field : fields) {
        if (!field.released) {
            distance = std::max(distance, field.property.getDistance());
        }
    }
    return distance;
}

std::vector<std::string> ScriptedTrack::getFields() const {
    std::vector<std::string> names;
    for (const Field& field : fields) {
        if (!field.released) {
            names.insert(names.end(), field.names.begin(), field.names.end());
        }
    }
    return names;
}

bool ScriptedTrack::release(std::string_view name) {
    bool remaining = false;
    for (Field& field : fields) {
        if (std::find(field.names.begin(), field.names.end(), name) != field.names.end()) {
            field.released = true;
        }
        remaining = remaining || !field.released;
    }
    return remaining;
}

} // namespace haylen::core
