#include <gtest/gtest.h>

#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <zlib.h>
#include <zstd.h>

#include "haylen/2d/tiled/Map.hpp"
#include "haylen/2d/tiled/MapRenderer.hpp"
#include "support/LuaFixture.hpp"
#include "support/TestFiles.hpp"
#include "support/TiledMaps.hpp"

namespace haylen::lua::tiled {

// Builds the test maps with their tilesets and templates, and the package that holds them.
TEST(TiledLuaTest, HandsDeeplyNestedGroupsToLua) {
    haylen::core::Json document = test::TiledMaps::orthogonalMap();
    haylen::core::Json group = haylen::core::Json::parse(R"({"id": 100, "name": "deepest", "type": "group", "layers": []})");
    for (int level = 1; level <= 60; ++level) {
        group = haylen::core::Json{{"id", 100 + level}, {"name", "level"}, {"type", "group"}, {"layers", haylen::core::Json::array({group})}};
    }

    document["layers"].push_back(group);
    std::map<std::string, std::string> files = test::TiledMaps::packageFiles();
    files["content/maps/island.tmj"] = document.dump();

    test::LuaFixture fixture(files);
    EXPECT_EQ(fixture.lua("local layers = require('haylen.tiled').newMapRenderer(require('haylen.assets').load('maps/island.tmj')):layers() local layer = layers[#layers] for level = 1, 60 do layer = layer.layers[1] end return layer.name"), "deepest");
}

TEST(TiledLuaTest, UsesMapsFromLua) {
    test::LuaFixture fixture(test::TiledMaps::packageFiles());
    // clang-format off
    fixture.runLua(R"(
        tiled = require('haylen.tiled')
        assets = require('haylen.assets')
        graphics2d = require('haylen.graphics2d')
        physics2d = require('haylen.physics2d')
        data = assets.load('maps/island.tmj')
        map = tiled.newMapRenderer(data)
        camera = graphics2d.newCamera()
        require('haylen.scene').push({render = function()
            graphics2d.beginWorld(camera)
            map:draw(camera, {layer = 1})
            map:drawLayer('inner', nil, {layer = 2})
        end})
    )");
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getError(), nullptr) << fixture.engine().getError()->what();

    EXPECT_EQ(fixture.lua("return data.path .. ' ' .. map.path .. ' ' .. tostring(data == assets.load('maps/island.tmj'))"), "maps/island.tmj maps/island.tmj true");
    EXPECT_EQ(fixture.lua("return map.width .. 'x' .. map.height .. ' ' .. map.tileWidth .. ' ' .. map.tileHeight .. ' ' .. map.pixelBounds.width .. ' ' .. map.pixelBounds.height .. ' ' .. map.skewX"), "4x3 16.0 16.0 64.0 48.0 0.0");
    EXPECT_EQ(fixture.lua("return map.orientation .. ' ' .. map.type .. ' ' .. map.backgroundColor:toHex()"), "orthogonal level #FF102030");
    EXPECT_EQ(fixture.lua("local p = map.properties return p.title .. p.level .. p.gravity .. tostring(p.safe) .. p.sky:toHex() .. tostring(p.unset) .. p.music .. p.boss .. p.spawn.kind"), "Island39.5true#FF0000FFnilaudio/theme.ogg7goblin");
    EXPECT_EQ(fixture.lua("return map:tile('ground', 1, 0) .. ' ' .. #map:layers() .. ' ' .. map:layer('group').layers[1].name .. ' ' .. map:layer('ground').width"), "2 7 inner 4");
    EXPECT_EQ(fixture.lua("local layer = map:layer('things') return layer.kind .. ' ' .. layer.type .. ' ' .. #layer.objects .. ' ' .. layer.objects[1].properties.hp"), "object entities 9 3");
    EXPECT_EQ(fixture.lua("local sky = map:layer('sky') return sky.kind .. ' ' .. sky.image .. ' ' .. tostring(sky.repeatX) .. ' ' .. sky.parallaxX"), "image maps/backgrounds/sky.png true 0.5");
    EXPECT_EQ(fixture.lua("local o = map:objects('things')[6] return o.shape .. ' ' .. o.text.text .. ' ' .. o.text.horizontalAlign .. ' ' .. o.text.color:toHex()"), "text Hi center #FF00FF00");
    EXPECT_EQ(fixture.lua("local o = map:objects()[4] return o.shape .. ' ' .. #o.points .. ' ' .. o.points[2].x .. ' ' .. o.opacity .. ' ' .. map:layer('ground').blend"), "polygon 3 10.0 1.0 alpha");
    EXPECT_EQ(fixture.lua("local t = map:tileInfo(2) return t.id .. ' ' .. t.tileset .. ' ' .. t.type .. ' ' .. tostring(t.properties.deep) .. ' ' .. #t.collision .. ' ' .. t.source.x"), "1 terrain water true 1 16.0");
    EXPECT_EQ(fixture.lua("local t = map:tileInfo(3 | 0x80000000) return tostring(t.flipHorizontal) .. tostring(t.flipVertical) .. tostring(t.flipDiagonal) .. ' ' .. #t.animation"), "truefalsefalse 2");
    EXPECT_EQ(fixture.lua("return tostring(map:tileInfo(0))"), "nil");
    EXPECT_EQ(fixture.lua("local sets = map:tilesets() return #sets .. ' ' .. sets[1].name .. ' ' .. sets[2].firstGid .. ' ' .. sets[1].wangSets[1].name .. ' ' .. sets[1].wangSets[1].colors[1].name .. ' ' .. sets[1].wangSets[1].tiles[1].wangId[2]"), "2 terrain 100 coast sand 1");
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        local spawned = map:spawn({
            player = function(object) return object.name .. '@' .. object.worldX .. ',' .. object.worldY end,
            rock = function(object) return object.properties.size end,
            collision = function() end,
        })
        return table.concat(spawned, ' ') .. ' ' .. #map:spawn({player = function() return 1 end}, 'things')
    )"), "spawn@10.0,20.0 1 3 1");
    // clang-format on
    // A factory that removes the layer of the objects still to come leaves them whole.
    EXPECT_EQ(fixture.lua("local copy = tiled.newMapRenderer(assets.load('maps/island.tmj')) local names = {} copy:spawn({player = function() copy:removeLayer('things') end, collision = function(object) names[#names + 1] = object.name end}) return table.concat(names, ' ')"), "zone pond wall fence");
    EXPECT_NE(fixture.lua("map:spawn({rock = 5})").find("The factory for the Tiled class \"rock\" is not a function."), std::string::npos);
    EXPECT_NE(fixture.lua("map:spawn({}, 'ground')").find("The map has no object layer named \"ground\"."), std::string::npos);
    EXPECT_EQ(fixture.lua("local x, y = map:cellToWorld(2, 1) local c, r = map:worldToCell(40, 20) local ox, oy = map:objectToWorld(3, 4) return x .. ',' .. y .. ' ' .. c .. ',' .. r .. ' ' .. ox .. ',' .. oy"), "32.0,16.0 2,1 3.0,4.0");

    fixture.runLua("map:setTile('ground', 0, 0, 3) map:setLayerVisible('decor', false) map:update(0.1)");
    EXPECT_EQ(fixture.lua("return map:tile('ground', 0, 0) .. ' ' .. tostring(map:layer('decor').visible)"), "3 false");
    EXPECT_EQ(fixture.lua("local world = physics2d.newWorld({gravity = {0, 0}}) local bodies = map:buildCollision(world) return #bodies .. ' ' .. bodies[1].type .. ' ' .. tostring(bodies[1].world == world)"), "5 static true");
    EXPECT_EQ(fixture.lua("local w = assets.load('maps/world/level.world') return #w .. ' ' .. w[1].path .. ' ' .. w[3].x .. ' ' .. w[3].width"), "3 maps/island.tmj 64.0 64.0");

    EXPECT_NE(fixture.lua("map:layer('missing')").find("The map has no layer named \"missing\"."), std::string::npos);
    EXPECT_NE(fixture.lua("map:draw(camera, {layer = 1, z = 2})").find("Unknown option \"z\""), std::string::npos);
    EXPECT_NE(fixture.lua("map:objects('ground')").find("The map has no object layer named \"ground\"."), std::string::npos);
    EXPECT_NE(fixture.lua("map:setTile('things', 0, 0, 1)").find("The map has no tile layer named \"things\"."), std::string::npos);
    EXPECT_NE(fixture.lua("tiled.newMapRenderer('map')").find("error: "), std::string::npos);

    // The Lua types carry the names of the C++ classes they wrap.
    EXPECT_NE(fixture.lua("local draw = map.draw draw(assets.load('maps/island.tmj'))").find("haylen.MapRenderer expected, got haylen.Map"), std::string::npos);
}

TEST(TiledLuaTest, CullsWithTheActiveCanvasAndPlacesWorldMaps) {
    test::LuaFixture fixture(test::TiledMaps::packageFiles());
    // clang-format off
    fixture.runLua(R"(
        tiled = require('haylen.tiled')
        graphics = require('haylen.graphics')
        graphics2d = require('haylen.graphics2d')
        map = tiled.newMapRenderer(require('haylen.assets').load('maps/island.tmj'))
        camera = graphics2d.newCamera()
        target = graphics.newRenderTarget(64, 64)
        require('haylen.scene').push({render = function()
            if canvas == 'target' then
                graphics2d.beginTarget(target, camera)
            else
                graphics2d.beginWorld(camera)
            end
            map:drawLayer('ground', camera, {x = offsetX, ysort = ysort})
        end})
    )");
    const auto groundSprites = [&](const std::string& setup) {
        fixture.runLua(setup);
        fixture.frames(1);
        EXPECT_EQ(fixture.engine().getError(), nullptr) << fixture.engine().getError()->what();
        return fixture.lua("return graphics2d.stats().sprites");
    };
    // clang-format on

    // The render target shows 64 units around the camera, so the ground far to its left is culled, while the wide world canvas still sees it.
    EXPECT_EQ(groundSprites("canvas = 'target' offsetX = 0 camera.position = {600, 24}"), "0");
    EXPECT_EQ(groundSprites("canvas = 'world'"), "11");
    EXPECT_EQ(groundSprites("canvas = 'target' offsetX = 600 camera.position = {632, 24}"), "11");
    EXPECT_EQ(groundSprites("canvas = 'world' offsetX = 3000"), "0");
    EXPECT_EQ(groundSprites("canvas = 'world' offsetX = 0 ysort = true"), "11");
    EXPECT_NE(fixture.lua("map:drawLayer('ground', camera, {ysort = 1})").find("The option \"ysort\""), std::string::npos);
}

TEST(TiledLuaTest, ExposesMapTilesetLayerAndPropertyDetails) {
    test::LuaFixture fixture(test::TiledMaps::packageFiles());
    fixture.runLua("tiled = require('haylen.tiled') assets = require('haylen.assets') map = tiled.newMapRenderer(assets.load('maps/island.tmj')) endless = tiled.newMapRenderer(assets.load('maps/infinite.tmj'))");

    EXPECT_EQ(fixture.lua("return tostring(map.infinite) .. ' ' .. map.renderOrder .. ' ' .. map.hexSideLength .. ' ' .. tostring(map.staggerX) .. ' ' .. tostring(map.staggerEven) .. ' ' .. map.parallaxOrigin.x .. ' ' .. tostring(endless.infinite)"), "false right-down 0 false false 0.0 true");
    EXPECT_EQ(fixture.lua("local types = map.propertyTypes return types.spawn .. ' ' .. tostring(types.title)"), "Spawn nil");
    EXPECT_EQ(fixture.lua("local loot = map.properties.loot return #loot .. ' ' .. loot[1] .. ' ' .. loot[2]:toHex() .. ' ' .. loot[3] .. ' ' .. tostring(loot[4][1]) .. ' ' .. loot[4][2] .. ' ' .. loot[5].kind"), "5 3 #FF00FF00 audio/coin.ogg true maps/gem.png bat");

    EXPECT_EQ(fixture.lua("local t = map:tilesets()[1] return t.path .. ' ' .. t.margin .. ' ' .. t.spacing .. ' ' .. t.tileOffset.x .. ' ' .. t.objectAlignment .. ' ' .. t.imageSize.x .. 'x' .. t.imageSize.y .. ' ' .. tostring(t.transparentColor) .. ' ' .. tostring(t.renderGridSize) .. ' ' .. tostring(t.preserveAspect) .. ' ' .. tostring(t.texture == assets.texture('maps/images/terrain.png'))"), "maps/tiles/terrain.tsj 0 0 0.0 unspecified 64.0x32.0 nil false false true");
    EXPECT_EQ(fixture.lua("local t = map:tilesets()[2] return t.path .. '|' .. t.objectAlignment .. ' ' .. t.image .. '|' .. tostring(t.texture)"), "|bottom |nil");
    EXPECT_EQ(fixture.lua("local set = map:tilesets()[1].wangSets[1] local color = set.colors[1] return set.type .. ' ' .. set.tile .. ' ' .. tostring(set.properties.wet) .. ' ' .. color.type .. ' ' .. color.probability .. ' ' .. string.format('%.1f', color.properties.speed) .. ' ' .. tostring(next(color.propertyTypes))"), "shore 0 true ground 0.5 0.8 nil");
    EXPECT_EQ(fixture.lua("local t = map:tileInfo(100) local rock = map:tileInfo(2) return t.image .. ' ' .. t.probability .. ' ' .. rock.image .. '|' .. rock.probability"), "maps/props/tree.png 1.0 |1.0");

    EXPECT_EQ(fixture.lua("return tostring(map:layer('things').indexDrawOrder) .. ' ' .. #map:layer('ground').chunks .. ' ' .. map:layer('sky').imageSize.x .. ' ' .. map:layer('sky').transparentColor:toHex() .. ' ' .. tostring(map:layer('things').objects[1].propertyTypes.hp)"), "false 0 8.0 #FFFF00FF nil");
    EXPECT_EQ(fixture.lua("local chunks = endless:layer('ground').chunks return #chunks .. ' ' .. chunks[1].x .. ',' .. chunks[1].y .. ' ' .. chunks[1].width .. 'x' .. chunks[1].height .. ' ' .. table.concat(chunks[1].gids, ',') .. ' ' .. chunks[2].gids[4]"), "2 -2,0 2x2 1,2,3,4 8");
    // A column beyond the cells of a map is an error instead of the column it wraps to.
    EXPECT_NE(fixture.lua("endless:removeChunk('ground', -2 + 2^32, 0)").find("integer out of range"), std::string::npos);
    EXPECT_EQ(fixture.lua("return #endless:layer('ground').chunks"), "2");
    // An infinite layer without chunks holds no cells, though its file gave it a size.
    EXPECT_EQ(fixture.lua("endless:removeChunk('ground', -2, 0) endless:removeChunk('ground', 0, 0) endless:draw() return endless:tile('ground', 1, 1) .. ' ' .. #endless:layer('ground').chunks .. ' ' .. tostring(endless:layer('ground').infinite) .. ' ' .. tostring(map:layer('ground').infinite)"), "0 0 true false");
    EXPECT_NE(fixture.lua("endless:setTile('ground', 1, 1, 2)").find("outside every chunk"), std::string::npos);

    EXPECT_EQ(fixture.lua("local t = map:tilesets()[1] return t.grid.orientation .. ' ' .. t.grid.width .. 'x' .. t.grid.height .. ' ' .. tostring(t.transformations.flipHorizontal) .. ' ' .. tostring(t.transformations.flipVertical) .. ' ' .. tostring(t.transformations.rotate) .. ' ' .. tostring(t.transformations.preferUntransformed)"), "isometric 32.0x16.0 true false true true");
    EXPECT_EQ(fixture.lua("local layer = map:layer('things') local sign for _, object in ipairs(layer.objects) do if object.name == 'sign' then sign = object end end return layer.color:toHex() .. ' ' .. tostring(sign.text.underline) .. ' ' .. tostring(sign.text.strikeout) .. ' ' .. tostring(sign.text.kerning) .. ' ' .. tostring(map:layer('ground').color)"), "#FFA0C040 true true false nil");
    EXPECT_EQ(fixture.lua("return tostring(map:tileInfo(1 | tiled.rotateHexagonal).rotateHexagonal) .. ' ' .. tostring(map:tileInfo(1).rotateHexagonal)"), "true false");
    EXPECT_EQ(fixture.lua("local filled = tiled.newMapRenderer(assets.load('maps/island.tmj', nil, {project = 'maps/game.tiled-project'})) return filled.properties.difficulty .. ' ' .. filled.properties.spawn.reward.coins .. ' ' .. tostring(map.properties.difficulty)"), "1 5 nil");
    EXPECT_NE(fixture.lua("assets.load('maps/island.tmj', nil, {project = 3})").find("The \"project\" of a Tiled map is the path of its \".tiled-project\" file."), std::string::npos);
    // Automapping rules load as an asset of their own and write the map, whose new layer draws with the terrain of the island.
    fixture.runLua("rules = assets.load('maps/rules.txt', 'tiledRules') shaded = tiled.newMapRenderer(assets.load('maps/island.tmj'))");
    EXPECT_EQ(fixture.lua("local grounds = 0 for row = 0, shaded.height - 1 do for column = 0, shaded.width - 1 do if shaded:tile('ground', column, row) ~= 0 then grounds = grounds + 1 end end end return tostring(shaded:automap(rules, {seed = 3}) == grounds) .. ' ' .. shaded:tile('shade', 0, 0) .. ' ' .. rules.ruleMaps[1].path .. ' ' .. rules.ruleMaps[1].filter .. '|'"), "true 3 maps/automap.tmj |");
    EXPECT_NE(fixture.lua("shaded:automap(rules, {seeds = 1})").find("Unknown option \"seeds\"."), std::string::npos);
    EXPECT_EQ(fixture.lua("return string.format('%X %X %X %X %X', tiled.flipHorizontal, tiled.flipVertical, tiled.flipDiagonal, tiled.rotateHexagonal, tiled.flagMask)"), "80000000 40000000 20000000 10000000 F0000000");
    EXPECT_EQ(fixture.lua("return tiled.tileId(5 | tiled.flipHorizontal | tiled.flipDiagonal) .. ' ' .. tiled.tileId(7)"), "5 7");
}

TEST(TiledLuaTest, BuildsMapsAtRunTime) {
    test::LuaFixture fixture(test::TiledMaps::packageFiles());
    // clang-format off
    fixture.runLua(R"(
        tiled = require('haylen.tiled')
        spatial2d = require('haylen.spatial2d')
        graphics2d = require('haylen.graphics2d')
        camera = graphics2d.newCamera()
        camera.anchor = 'topLeft'
        map = tiled.newMapRenderer(tiled.newMap({width = 8, height = 4, tileWidth = 16, tileHeight = 16, backgroundColor = '#FF203040'}))
        first = map:addTileset({name = 'terrain', image = 'maps/images/terrain.png', tileWidth = 16, tileHeight = 16, wangSets = {
            {name = 'grass', type = 'corner', colors = {'grass'}, tiles = {
                {tile = 0, wangId = {0, 1, 0, 1, 0, 1, 0, 1}},
                {tile = 1, wangId = {0, 0, 0, 1, 0, 0, 0, 0}},
                {tile = 2, wangId = {0, 0, 0, 0, 0, 1, 0, 0}},
                {tile = 3, wangId = {0, 1, 0, 0, 0, 0, 0, 0}},
                {tile = 4, wangId = {0, 0, 0, 0, 0, 0, 0, 1}},
            }},
        }})
        map:addLayer({name = 'ground'})
        map:addLayer({name = 'things', kind = 'object'})
        grid = spatial2d.newCellGrid(2, 2, 3)
        grid:set(0, 0, -1)
        map:fill('ground', grid, first, 5, 2)
        require('haylen.scene').push({render = function()
            graphics2d.beginWorld(camera)
            map:draw(camera)
        end})
    )");
    // clang-format on
    fixture.frames(1);
    ASSERT_EQ(fixture.engine().getError(), nullptr) << fixture.engine().getError()->what();
    EXPECT_EQ(fixture.lua("return first .. ' ' .. map:tile('ground', 5, 2) .. ' ' .. map:tile('ground', 6, 2) .. ' ' .. map:tile('ground', 6, 3) .. ' ' .. #map:layers() .. ' ' .. map.width"), "1 0 4 4 2 8");
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().sprites, 3U);

    // Painting a corner terrain gives the cell the full tile and the neighbours the tiles of the corners they share, and erasing empties them.
    fixture.runLua("map:paintTerrain('ground', 'grass', 2, 1, 1)");
    EXPECT_EQ(fixture.lua("return map:tile('ground', 2, 1) .. ' ' .. map:tile('ground', 1, 0) .. ' ' .. map:tile('ground', 3, 0) .. ' ' .. map:tile('ground', 1, 2) .. ' ' .. map:tile('ground', 3, 2)"), "1 2 3 4 5");
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().sprites, 12U);
    fixture.runLua("map:paintTerrain('ground', 'grass', 2, 1, 0)");
    EXPECT_EQ(fixture.lua("return map:tile('ground', 2, 1) .. ' ' .. map:tile('ground', 1, 0) .. ' ' .. map:tile('ground', 3, 2)"), "0 0 0");
    EXPECT_EQ(fixture.lua("map:paintTerrain('ground', 'grass', 0, 0, 1) local painted = map:tile('ground', 0, 0) map:paintTerrain('ground', 'grass', 0, 0, 0) return painted .. ' ' .. map:tile('ground', 1, 1)"), "1 0");

    // The JSON of the map holds the tileset with its path relative to the file and the cells row by row.
    EXPECT_EQ(fixture.lua("local j = map:toJson('maps/built.tmj') return j.tilesets[1].image .. ' ' .. j.layers[1].name .. ' ' .. j.layers[1].data[23] .. ' ' .. j.layers[2].type .. ' ' .. j.width"), "images/terrain.png ground 4 objectgroup 8");

    // A Wang color is a whole number from 0 to 255, never cut to its low bits.
    EXPECT_EQ(fixture.lua("local ok, message = pcall(map.addTileset, map, {name = 'wide', image = 'maps/images/terrain.png', tileWidth = 16, tileHeight = 16, wangSets = {{name = 'cliff', tiles = {{tile = 2, wangId = {0, 256, 0, 1, 0, 1, 0, 1}}}}}}) return message:match('The Wang.*')"), "The Wang set \"cliff\" gives the tile 2 the color 256, which is not a whole number from 0 to 255.");

    // A tileset painted larger than the grid draws its tiles at the size of the grid, as Tiled calls it.
    EXPECT_EQ(fixture.lua("map:addTileset({name = 'painted', image = 'maps/images/terrain.png', tileWidth = 32, tileHeight = 32, renderGridSize = true, preserveAspect = true}) local t, j = map:tilesets()[2], map:toJson('maps/built.tmj') return tostring(t.renderGridSize) .. ' ' .. tostring(t.preserveAspect) .. ' ' .. j.tilesets[2].tilerendersize .. ' ' .. j.tilesets[2].fillmode"), "true true grid preserve-aspect-fit");

    // An infinite map takes and drops chunks of cells.
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        local open = tiled.newMapRenderer(tiled.newMap({infinite = true, tileWidth = 16, tileHeight = 16}))
        local gid = open:addTileset({name = 'terrain', texture = require('haylen.assets').texture('maps/images/terrain.png'), tileWidth = 16, tileHeight = 16})
        open:addLayer({name = 'ground'})
        open:setChunk('ground', -16, 0, grid, gid)
        local before = open:tile('ground', -15, 1)
        open:removeChunk('ground', -16, 0)
        return before .. ' ' .. open:tile('ground', -15, 1) .. ' ' .. open:toJson().tilesets[1].image
    )"), "4 0 maps/images/terrain.png");
    EXPECT_NE(fixture.lua(R"(
        local open = tiled.newMapRenderer(tiled.newMap({infinite = true, tileWidth = 16, tileHeight = 16}))
        open:addLayer({name = 'ground'})
        open:setChunk('ground', 2147483640, 0, grid)
    )").find("The chunk at 2147483640, 0 reaches farther than 16777216 cells from the origin."), std::string::npos);
    // clang-format on

    EXPECT_NE(fixture.lua("tiled.newMap({width = 0, height = 4, tileWidth = 16, tileHeight = 16})").find("A map needs a positive tile size, and a finite map a positive width and height in cells."), std::string::npos);
    EXPECT_NE(fixture.lua("tiled.newMap({width = 4, height = 4, tileWidth = 16, tileHeight = 16, orientation = 'round'})").find("A map has the orientation"), std::string::npos);
    EXPECT_NE(fixture.lua("tiled.newMap({width = 4, height = 4, tileWidth = 16, tileHeight = 16, depth = 2})").find("Unknown option \"depth\"."), std::string::npos);
    EXPECT_NE(fixture.lua("map:addTileset({name = 'none', tileWidth = 16, tileHeight = 16})").find("A tileset needs the package path of its \"image\" or a \"texture\"."), std::string::npos);
    EXPECT_NE(fixture.lua("map:addTileset({image = 'maps/images/terrain.png', tileWidth = 16, tileHeight = 16, wangSets = {{name = 'w', type = 'round'}}})").find("A Wang set has the type \"corner\", \"edge\" or \"mixed\"."), std::string::npos);
    EXPECT_NE(fixture.lua("map:addLayer({name = 'ground'})").find("A layer of a map needs a name of its own, and \"ground\" is empty or taken."), std::string::npos);
    EXPECT_NE(fixture.lua("map:addLayer({name = 'sky', kind = 'image'})").find("A layer built in code has the kind \"tile\" or \"object\"."), std::string::npos);
    EXPECT_NE(fixture.lua("map:fill('ground', spatial2d.newCellGrid(1, 1, 100), first)").find("The tile \"101\" belongs to no tileset of the map."), std::string::npos);
    EXPECT_NE(fixture.lua("map:setChunk('ground', 0, 0, grid, first)").find("Only the tile layers of an infinite map have chunks."), std::string::npos);
    EXPECT_NE(fixture.lua("map:paintTerrain('ground', 'sand', 0, 0, 1)").find("No tileset of the map has the Wang set \"sand\"."), std::string::npos);
    EXPECT_NE(fixture.lua("map:paintTerrain('ground', 'grass', 0, 0, 2)").find("The Wang set \"grass\" has the colors 1 to 1, and 0 erases."), std::string::npos);
    EXPECT_NE(fixture.lua("map:fill('things', grid, first)").find("The map has no tile layer named \"things\"."), std::string::npos);
    EXPECT_EQ(fixture.lua("map:removeLayer('things') return #map:layers()"), "1");
    EXPECT_NE(fixture.lua("map:removeLayer('things')").find("The map has no layer named \"things\" at its top level."), std::string::npos);
}

} // namespace haylen::lua::tiled
