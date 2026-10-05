#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "haylen/2d/graphics/PartColors.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/2d/graphics/SpriteLayout.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Rect.hpp"

struct lua_State;

namespace haylen::graphics2d {

class SpriteBatch;
class StaticSpriteBatch;

// Installs the `SpriteBatch` and `StaticSpriteBatch` classes of `haylen.graphics2d`. Sprites of a batch are numbered from one.
class SpriteBatchLua final {
  public:
    // The key of a sprite table that lists the fields a float buffer holds, as `drawBatch` reads it.
    static constexpr std::array<std::string_view, 1> kLayoutFields{"fields"};

    // The key of a sprite table with the part colors that recolor the sprite in a draw with a part mask.
    static constexpr std::array<std::string_view, 1> kPartColorFields{"partColors"};

    static void install(lua_State* L);

    // Reads a list of field names at index, such as `{'x', 'y', 'rotation'}`, into a layout over the template sprite.
    [[nodiscard]] static SpriteLayout readLayout(lua_State* L, int index, const SpriteInstance& sprite = {});

  private:
    static void pushSpriteInstance(lua_State* L, const SpriteInstance& sprite, const PartColors& colors);

    // Gives the sprite at an index of the batch the part colors of a sprite table that has them.
    static void readPartColors(lua_State* L, int table, SpriteBatch& batch, std::size_t index);
    [[nodiscard]] static std::size_t checkIndex(lua_State* L, int index, const SpriteBatch& batch);
    [[nodiscard]] static std::size_t checkFirst(lua_State* L, int index);

    static int add(lua_State* L);
    static int set(lua_State* L);
    static int get(lua_State* L);
    static int reserve(lua_State* L);
    static int resize(lua_State* L);
    static int writeFields(lua_State* L);
    static int readFields(lua_State* L);
    static int remove(lua_State* L);
    static int clear(lua_State* L);
    static int size(lua_State* L);
    static int draw(lua_State* L);
    static int bake(lua_State* L);

    [[nodiscard]] static std::size_t staticSize(const StaticSpriteBatch& batch);
    [[nodiscard]] static math::Rect staticBounds(const StaticSpriteBatch& batch);
    [[nodiscard]] static graphics::Texture staticTexture(const StaticSpriteBatch& batch);
};

} // namespace haylen::graphics2d
