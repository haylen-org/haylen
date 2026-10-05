#include "lua/ModuleClosures.hpp"

#include <algorithm>
#include <string>

#include "lua/ModuleSnapshot.hpp"

namespace haylen::lua {

ModuleClosures::ModuleClosures(lua_State* L, std::string_view path, std::span<const int> roots) : source("@" + std::string(path)) {
    std::vector<int> indices;
    indices.reserve(roots.size());
    for (const int root : roots) {
        indices.push_back(lua_absindex(L, root));
    }
    lua_newtable(L);
    anchor = Reference(L, -1);
    lua_pop(L, 1);
    for (const int root : indices) {
        visit(L, root, 0);
    }
}

// Only mergeable tables lead further, so the functions of instances and engine objects, which a reload keeps as they are, stay out.
void ModuleClosures::visit(lua_State* L, int index, int depth) {
    luaL_checkstack(L, 6, "The functions of a module are nested too deeply.");
    if (depth > kMaxDepth) {
        return;
    }
    const int type = lua_type(L, index);
    if (type == LUA_TFUNCTION) {
        if (lua_iscfunction(L, index) != 0 || !seen.insert(lua_topointer(L, index)).second) {
            return;
        }
        lua_Debug info{};
        lua_pushvalue(L, index);
        lua_getinfo(L, ">S", &info);
        if (source == info.source) {
            addClosure(L, index, depth);
        }
        return;
    }
    if (type != LUA_TTABLE || !ModuleSnapshot::isMergeable(L, index) || !seen.insert(lua_topointer(L, index)).second) {
        return;
    }
    if (lua_getmetatable(L, index) != 0) {
        visit(L, lua_gettop(L), depth + 1);
        lua_pop(L, 1);
    }
    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        visit(L, lua_gettop(L), depth + 1);
        lua_pop(L, 1);
    }
}

void ModuleClosures::addClosure(lua_State* L, int index, int depth) {
    ++count;
    anchor.push(L);
    lua_pushvalue(L, index);
    lua_rawseti(L, -2, count);
    lua_pop(L, 1);

    const int closure = count;
    for (int upvalue = 1;; ++upvalue) {
        const char* name = lua_getupvalue(L, index, upvalue);
        if (name == nullptr) {
            break;
        }
        if (std::string_view(name) != "_ENV") {
            cells[std::string(name)].push_back({.closure = closure, .upvalue = upvalue, .id = lua_upvalueid(L, index, upvalue)});
        }
        visit(L, lua_gettop(L), depth + 1);
        lua_pop(L, 1);
    }
}

std::string ModuleClosures::findAmbiguous() const {
    for (const auto& [name, found] : cells) {
        const void* id = found.front().id;
        if (std::ranges::any_of(found, [id](const Cell& cell) { return cell.id != id; })) {
            return name;
        }
    }
    return {};
}

void ModuleClosures::pushClosure(lua_State* L, const Cell& cell) const {
    anchor.push(L);
    lua_rawgeti(L, -1, cell.closure);
    lua_remove(L, -2);
}

void ModuleClosures::pushValues(lua_State* L) const {
    lua_createtable(L, 0, static_cast<int>(cells.size()));
    for (const auto& [name, found] : cells) {
        pushClosure(L, found.front());
        lua_getupvalue(L, -1, found.front().upvalue);
        lua_remove(L, -2);
        lua_setfield(L, -2, name.c_str());
    }
}

} // namespace haylen::lua
