#include "core/ClassLua.hpp"

#include <lua.hpp>

#include <string>

#include "haylen/lua/Runtime.hpp"

namespace haylen::core {

// Instances take their class as metatable and classes take a metatable that reaches the parent, so methods inherit through __index. Metamethods are looked up without __index, so a class copies those of its parent when it is created.
const std::string_view ClassLua::kSource = R"lua(
local classes = setmetatable({}, {__mode = 'k'})

local function describe(self)
    return string.format('%s: %p', getmetatable(self).name, self)
end

local function is(self, other)
    local class = classes[self] and self or getmetatable(self)
    while class do
        if class == other or class.mixins[other] then
            return true
        end
        class = class.super
    end
    return false
end

local function include(class, mixin)
    if type(mixin) ~= 'table' then
        error("bad argument #1 to 'include' (table expected, got " .. type(mixin) .. ')', 2)
    end
    for key, value in pairs(mixin) do
        if rawget(class, key) == nil and key ~= 'included' then
            class[key] = value
        end
    end
    class.mixins[mixin] = true
    if type(mixin.included) == 'function' then
        mixin.included(class)
    end
    return class
end

local function class(name, parent, ...)
    if type(name) ~= 'string' then
        error("bad argument #1 to 'class' (string expected, got " .. type(name) .. ')', 2)
    end
    if parent ~= nil and not classes[parent] then
        error("bad argument #2 to 'class' (class expected, got " .. type(parent) .. ')', 2)
    end

    local created = {name = name, super = parent, mixins = {}, __tostring = describe}
    created.__index = created
    if parent then
        for key, value in pairs(parent) do
            if type(key) == 'string' and key:sub(1, 2) == '__' and key ~= '__index' then
                created[key] = value
            end
        end
    end
    classes[created] = true

    function created.new(...)
        local instance = setmetatable({}, created)
        if instance.init then
            instance:init(...)
        end
        return instance
    end
    created.is = is
    created.include = include

    setmetatable(created, {
        __index = parent,
        __call = function(self, ...) return self.new(...) end,
        __tostring = function(self) return 'class ' .. self.name end,
    })
    for index = 1, select('#', ...) do
        include(created, (select(index, ...)))
    end
    return created
end

return class
)lua";

void ClassLua::push(lua_State* L) {
    if (lua_getfield(L, LUA_REGISTRYINDEX, kRegistryKey) == LUA_TFUNCTION) {
        return;
    }
    lua_pop(L, 1);
    if (luaL_loadbufferx(L, kSource.data(), kSource.size(), "=haylen.class", "t") != LUA_OK) {
        lua_error(L);
    }
    lua_call(L, 0, 1);
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, kRegistryKey);
}

} // namespace haylen::core
