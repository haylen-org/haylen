#include "2d/graphics/MaterialBinding.hpp"

#include <lua.hpp>

#include <string>
#include <string_view>
#include <vector>

#include "haylen/2d/graphics/Material.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::graphics2d {

void MaterialBinding::setValue(lua_State* L, haylen::graphics2d::Material& material, int nameIndex, int valueIndex) {
    using Type = haylen::graphics::Shader::UniformType;
    const auto name = lua::Stack::read<std::string_view>(L, nameIndex);
    const haylen::graphics::Shader& shader = material.getShader();
    if (shader.findTexture(name) != nullptr) {
        material.setTexture(name, lua_isnil(L, valueIndex) ? haylen::graphics::Texture{} : lua::Stack::read<haylen::graphics::Texture>(L, valueIndex));
        return;
    }

    if (lua_type(L, valueIndex) == LUA_TNUMBER) {
        material.set(name, lua::Stack::read<float>(L, valueIndex));
    } else if (const haylen::math::Transform2D* transform = lua::Userdata::test<haylen::math::Transform2D>(L, valueIndex)) {
        material.set(name, *transform);
    } else if (lua::Userdata::test<haylen::math::Vec2>(L, valueIndex) != nullptr) {
        material.set(name, lua::Stack::read<haylen::math::Vec2>(L, valueIndex));
    } else if (lua_type(L, valueIndex) == LUA_TTABLE) {
        // A list of numbers fills every number of the uniform, and named fields read as a vector or a color by the type of the uniform.
        lua_rawgeti(L, valueIndex, 1);
        const bool numbers = lua_type(L, -1) == LUA_TNUMBER;
        lua_pop(L, 1);
        const haylen::graphics::Shader::Uniform* uniform = shader.findUniform(name);
        if (numbers || uniform == nullptr) {
            lua::Stack::readInto(L, valueIndex, valueScratch);
            material.set(name, valueScratch);
        } else if (uniform->type == Type::Vec2 || uniform->type == Type::IVec2) {
            material.set(name, lua::Stack::read<haylen::math::Vec2>(L, valueIndex));
        } else {
            material.set(name, lua::Stack::read<haylen::math::Color>(L, valueIndex));
        }
    } else {
        material.set(name, lua::Stack::read<haylen::math::Color>(L, valueIndex));
    }
}

int MaterialBinding::newMaterial(lua_State* L) {
    haylen::graphics2d::Material material(lua::Stack::read<haylen::graphics::Shader>(L, 1));
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua_pushnil(L);
        while (lua_next(L, 2) != 0) {
            luaL_argcheck(L, lua_type(L, -2) == LUA_TSTRING, 2, "uniform names must be strings");
            setValue(L, material, lua_gettop(L) - 1, lua_gettop(L));
            lua_pop(L, 1);
        }
    }

    lua::Userdata::emplace<haylen::graphics2d::Material>(L, std::move(material));
    return 1;
}

// Sets a uniform or texture with `material:set(name, value)`.
int MaterialBinding::set(lua_State* L) {
    setValue(L, lua::Userdata::check<haylen::graphics2d::Material>(L, 1), 2, 3);
    return 0;
}

// Returns a texture, or a number, `Vec2` or `Color` for single `float`, `int`, `vec2` and `vec4` uniforms, and a list of numbers for the others.
int MaterialBinding::get(lua_State* L) {
    using Type = haylen::graphics::Shader::UniformType;
    const haylen::graphics2d::Material& material = lua::Userdata::check<haylen::graphics2d::Material>(L, 1);
    const std::string name = lua::Stack::read<std::string>(L, 2);
    if (material.getShader().findTexture(name) != nullptr) {
        const haylen::graphics::Texture texture = material.getTexture(name);
        if (!texture.isValid()) {
            lua_pushnil(L);
            return 1;
        }

        lua::Stack::push(L, texture);
        return 1;
    }

    const std::vector<float> values = material.get(name);
    const haylen::graphics::Shader::Uniform& uniform = *material.getShader().findUniform(name);
    if (uniform.count > 1) {
        lua::Stack::push(L, values);
        return 1;
    }

    switch (uniform.type) {
    case Type::Float:
        lua::Stack::push(L, values[0]);
        return 1;
    case Type::Int:
        lua::Stack::push(L, static_cast<lua_Integer>(values[0]));
        return 1;
    case Type::Vec2:
    case Type::IVec2:
        lua::Stack::push(L, haylen::math::Vec2{values[0], values[1]});
        return 1;
    case Type::Vec4:
        lua::Stack::push(L, haylen::math::Color{values[0], values[1], values[2], values[3]});
        return 1;
    case Type::Vec3:
    case Type::IVec3:
    case Type::IVec4:
    case Type::Mat4:
        break;
    }

    lua::Stack::push(L, values);
    return 1;
}

int MaterialBinding::shader(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics2d::Material>(L, 1).getShader());
    return 1;
}

void MaterialBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::graphics2d::Material>(L).function("set", &lua::Binding::native<&set>).function("get", &lua::Binding::native<&get>).property("shader", &shader).meta("__eq", &lua::Userdata::equal<haylen::graphics2d::Material>).install();
}

} // namespace haylen::lua::graphics2d
