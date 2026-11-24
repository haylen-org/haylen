#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "support/LuaFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::lua::tiled {

TEST(MapQueryLuaTest, CastsRaysOverMapsFromLua) {
    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(32, 16, 0xFFFFFFFFU);
    // clang-format off
    const std::string map = R"({
        "type": "map", "orientation": "orthogonal", "renderorder": "right-down", "width": 8, "height": 6, "tilewidth": 16, "tileheight": 16, "infinite": false,
        "tilesets": [{"firstgid": 1, "name": "tiles", "tilewidth": 16, "tileheight": 16, "tilecount": 2, "columns": 2, "image": "tiles.png", "imagewidth": 32, "imageheight": 16}],
        "layers": [
            {"id": 1, "name": "walls", "type": "tilelayer", "width": 8, "height": 6, "data": [0,0,0,0,0,1,0,0, 0,0,0,0,0,1,0,0, 0,0,0,0,0,1,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,2,0,0, 0,0,0,0,0,1,0,0]},
            {"id": 2, "name": "rocks", "type": "objectgroup", "offsetx": 4, "offsety": 2, "objects": [
                {"id": 1, "name": "crate", "type": "block", "x": 20, "y": 20, "width": 10, "height": 10},
                {"id": 2, "name": "fence", "x": 90, "y": 10, "polyline": [{"x": 0, "y": 0}, {"x": 0, "y": 30}]}
            ]}
        ]
    })";
    test::LuaFixture fixture({{"content/maps/query.tmj", map}, {"content/maps/tiles.png", std::string(image.begin(), image.end())}});
    fixture.runLua(R"(
        tiled = require('haylen.tiled')
        map = tiled.newMapRenderer(require('haylen.assets').load('maps/query.tmj'))
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("local hit = map:raycastTiles('walls', 8, 8, 200, 8) return hit.column .. ':' .. hit.row .. ' ' .. hit.gid .. ' ' .. hit.x .. ' ' .. hit.normalX .. ' ' .. hit.distance"), "5:0 1 80.0 -1.0 72.0");
    EXPECT_EQ(fixture.lua("return tostring(map:raycastTiles('walls', 8, 56, 200, 56)) .. ' ' .. tostring(map:raycastTiles('walls', 8, 72, 200, 72, function(gid) return gid ~= 2 end))"), "nil nil");
    EXPECT_EQ(fixture.lua("local hit = map:raycastObjects('rocks', 0, 27, 200, 27) return hit.name .. ' ' .. hit.id .. ' ' .. hit.type .. ' ' .. hit.x"), "crate 1 block 24.0");
    EXPECT_EQ(fixture.lua("return map:raycastObjects(nil, 60, 27, 200, 27).name .. ' ' .. tostring(map:raycastObjects('rocks', 0, 100, 200, 100))"), "fence nil");
    EXPECT_EQ(fixture.lua("local outlines = map:objectOutlines('rocks') return #outlines .. ' ' .. #outlines[1] .. ' ' .. outlines[1][1].x .. ' ' .. #map:objectOutlines()"), "1 4 24.0 1");
    EXPECT_NE(fixture.lua("map:raycastTiles('rocks', 0, 0, 1, 0)").find("no tile layer named \"rocks\""), std::string::npos);

    // The `solid` callback runs once the ray has gathered its tiles, so a tile it places behind the wall does not stop the ray.
    EXPECT_EQ(fixture.lua("local hit = map:raycastTiles('walls', 8, 8, 200, 8, function(gid) map:setTile('walls', 6, 0, 2) return gid == 2 end) return tostring(hit) .. ' ' .. map:tile('walls', 6, 0)"), "nil 2");
}

} // namespace haylen::lua::tiled
