# 🗺️ Tiled maps

This guide shows how to make levels with the [Tiled](https://www.mapeditor.org) map editor and play them in Haylen. It is for developers and level designers who build 2D levels out of tiles and objects. It covers how to save maps for the engine, which features of Tiled work, and how to load, draw and read a map. It then covers collision, spawning objects, ray casts, maps built in code, automapping and worlds of many maps, and ends with the conventions of the Tiny Island map. The [`haylen.tiled` reference](../lua-api/tiled.md) lists every function, field and error.

## How the parts fit

The module `haylen.tiled` works with a few kinds of values:

- **A map asset**, a `Map`, holds the parsed data of a `.tmj` file. Load it with [`haylen.assets`](../lua-api/assets.md), which caches it by path like any asset.
- **A playable map**, a `MapRenderer`, is what a game uses. Make one from a map asset with [`tiled.newMapRenderer`](../lua-api/tiled.md#tilednewmaprendererasset). It draws the layers, animates the tiles, answers questions about cells, layers, objects and tiles, builds physics collision and spawns entities from objects.
- **A world** is the list of the maps of a `.world` file, with their places.
- **Automapping rules**, an `AutoMapper`, add borders, corners and decorations to a map the way the editor does.

Each playable map owns its own copy of the data. So `map:setTile` never changes the cached asset, and two maps made from the same asset are independent.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local camera = graphics2d.newCamera()

scene.push({
    update = function(self, dt)
        map:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        map:draw(camera)
    end,
})
```

## Authoring maps

### Files

Save maps in the Tiled JSON format, with the `.tmj` extension. Keep external tilesets in `.tsj` files and object templates in `.tj` files. The runtime targets the current JSON format, as Tiled 1.12 writes it. It does not read the XML formats (`.tmx`, `.tsx` and `.tx`), and it has no code for obsolete formats. Every layer needs the `id` that Tiled writes.

Every path inside a map is relative to the file that contains it. So a map, its tilesets, its templates and its images can live in any folders under the `content/` folder of the package. The relative paths that Tiled writes only need to stay inside that folder.

### Tiles painted in high definition

A game can paint its tiles larger than the grid it plays at, so they stay sharp on large screens. Keep the map grid at the size the game plays at. Give the tileset the size of the painted tiles, and set its tile render size to the map grid size in Tiled, which writes `"tilerendersize": "grid"`. Then every tile draws at the size of a cell, with its offset and its collision shapes scaled the same way. Load such a map with the `linear` filter.

The linear filter reads the pixels around each point. So keep a gutter around each tile in the tileset image, made of copies of the edge pixels of the tile. Give the gutter to Tiled as the margin and the spacing of the tileset. Then a tile never shows its neighbor along its edges. The tiles of the Tiled tests of the test project are painted at four times their grid this way.

## Supported features

- **Orientations**: Orthogonal, isometric, staggered and hexagonal maps, with `staggeraxis`, `staggerindex` and `hexsidelength`. Oblique maps with the `skewx` and `skewy` of Tiled 1.12. Cells, objects, bounds and collision all follow the skew. A skew that folds the grid onto a line is refused.
- **Render orders**: `right-down`, `right-up`, `left-down` and `left-up`.
- **Tile data**: JSON arrays or base64, uncompressed or compressed with zlib, gzip or zstd.
- **Infinite maps**: Tile layers stored as chunks, with cells at negative coordinates.
- **Layer types**: Tile layers, object layers, image layers that repeat on x, y or both, and nested group layers.
- **Layer attributes**: Visibility, opacity, offsets, tint colors, parallax factors with the parallax origin of the map, classes and custom properties. Groups pass their offset, parallax, tint, opacity and visibility on to their children, and tints and opacities multiply down the tree. Layer locking is a feature of the editor and does nothing at run time.
- **Blend modes**: Every mode that Tiled offers. The modes `normal`, `add`, `multiply` and `screen` draw each tile with the matching blend of the GPU. The other modes mix each color with the color below it: `overlay`, `darken`, `lighten`, `color-dodge`, `color-burn`, `hard-light`, `soft-light`, `difference` and `exclusion`. Such a layer draws into a [group](../lua-api/graphics2d.md#graphics2dbegingroupoptions-and-graphics2dendgroup) that blends with what the layers under it drew, as Tiled composites the layer. Tiled draws the children of a group with their own modes and never blends a group as a whole. So a group layer with any mode other than `normal` fails to load with an error, instead of being ignored.
- **Tilesets**: Embedded in the map or external. Image tilesets with margin, spacing and tile offsets. Image collection tilesets, whose tiles can use a part of their image and keep the ids of removed tiles unused. Transparent colors and object alignment. The `grid` tile render size with the `preserve-aspect-fit` fill mode, which scales the offsets and collision shapes of the tiles with their images, as Tiled does. The grid that Tiled places tile objects on, the flips and turns that Wang painting may apply, and tile classes, probabilities and properties.
- **Tile flips**: Horizontal, vertical and diagonal, plus the 120-degree rotation flag of hexagonal maps.
- **Animated tiles**: Tile animations play on tile layers and on tile objects, driven by `map:update(dt)`.
- **Wang sets**: Corner, edge and mixed sets, with their colors and tiles, which `map:tilesets()` returns. A Wang set is the data of a terrain brush of the editor. Loading a map never repaints its tiles with it, but `map:paintTerrain` paints with it while the game runs.
- **Objects**: Rectangles, ellipses, capsules, points, polygons, polylines, text and tile objects, with rotation, visibility, opacity, classes and custom properties, and the color of each object layer. Text draws with its size, color, wrapping, alignment, bold, italic, underline and strikeout, in the font that the map renderer holds. So its font family and its kerning switch are data for the app.
- **Templates**: The `.tj` object templates. Their fields and properties are defaults that each instance overrides one by one. A tile template maps its tile to the map's own copy of the tileset of the template.
- **Properties**: The `string`, `int`, `float`, `bool`, `color`, `file`, `object`, `class` and `list` values, with classes inside classes and lists inside lists. Colors become `Color` values, files become paths inside the `content/` folder, object references become object ids, class values become nested tables and lists become sequences. With the `project` option, the classes of the `.tiled-project` file fill in the members that Tiled leaves out because they keep their defaults.
- **Worlds**: `.world` files with listed maps and file name patterns.
- **Automapping**: Rules files and rule maps of the current rule system, applied at run time, as [Automapping](#automapping) describes. The legacy `regions` layers are refused. Tiled converts a rule map away from them when its regions are removed.

## Loading maps

Maps are assets of [`haylen.assets`](../lua-api/assets.md). The asset type `tiled` handles `.tmj` files and `tiledWorld` handles `.world` files, so `assets.load` finds the type from the extension. The type `tiledRules` loads automapping rules. Rules have no extension of their own, so pass that type by name. Loading a map also loads its external tilesets, its templates and its images.

The options of a map set the texture `filter`, `wrap` and `mipmaps` of all its images, and its `project` file. The filter is `'nearest'` by default, which keeps pixel art sharp. Use `'linear'` for art painted in high definition. The [reference](../lua-api/tiled.md#loading-maps-and-worlds) lists the options.

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj', nil, {filter = 'linear'}))
```

A large map loads in the background with `assets.loadAsync`, which parses the map and decodes its images on worker threads and returns a promise. A map can also be part of a preload group. Tiny Island loads its island this way while the boot screen shows progress:

```json
{
    "groups": {
        "menu": [{"path": "maps/island.tmj", "options": {"filter": "linear"}}, {"path": "world/props.json", "type": "atlas", "options": {"filter": "linear"}}]
    }
}
```

The [Lua guide](../core/lua.md) explains promises and coroutines, and the [loading guide](../content/loading.md) explains preload groups.

## Drawing

There are two ways to draw a map:

- [`map:draw(camera, options)`](../lua-api/tiled.md#mapdrawcamera-options) draws every visible layer, in map order, into the active canvas.
- [`map:drawLayer(name, camera, options)`](../lua-api/tiled.md#mapdrawlayername-camera-options) draws one layer, with the children of a group. The layer keeps the offset, parallax, tint and visibility it inherits from the groups above it.

Draw layer by layer to put the sprites of your game between map layers.

The `camera` gives the position for parallax. Culling, which skips what the view cannot show, uses the area that the active canvas shows. The `options` take a [draw order](../lua-api/graphics2d.md#draw-order), and each layer replaces the `blend` with its own blend mode. The options `x` and `y` place the origin of the map in the world, so the maps of a world share one camera.

### How a map draws fast

A tile layer bakes its tiles into static batches on the GPU, in regions of cells, the first time it draws. After that, only the regions that the camera sees are drawn. A change of one cell bakes only its region again. Animated tiles are drawn every frame, with the frame that matches the time of the map. So call [`map:update(dt)`](../lua-api/tiled.md#mapupdatedt) in the `update` of the scene to move that time on.

### Layers in front and behind

Tiny Island draws its ground layers under everything and its clouds over everything, with the sprites of the game in between. Its world canvas sorts by depth. The ground layers go on a low layer of the canvas, with growing depths. The entities go on a middle layer, with their y position as their depth. The clouds go on the highest layer. This code is trimmed from `systems/island.lua` and `systems/game.lua` of the sample:

```lua
local graphics2d = require('haylen.graphics2d')

local layers = {ground = 0, entities = 1, clouds = 2}
local groundLayers = {'foam', 'ground', 'shadow', 'cliffs', 'plateau'}
local island = {}

function island:drawGround(camera)
    for index, name in ipairs(groundLayers) do
        self.map:drawLayer(name, camera, {layer = layers.ground, depth = index})
    end
    self.map:drawLayer('decorations', camera, {layer = layers.ground, depth = #groundLayers + 1})
end

function island:drawClouds(camera)
    self.map:drawLayer('clouds', camera, {layer = layers.clouds})
end

function island:render(camera, player)
    graphics2d.beginWorld(camera, {sort = 'depth'})
    self:drawGround(camera)
    player:draw()
    self:drawClouds(camera)
end

return island
```

A layer whose parallax factor is below 1 moves slower than the camera, like a distant background. A factor above 1 makes a foreground that moves faster than the camera.

### Walking behind trees and walls

Drawing a layer with `{ysort = true}` sorts its rows and its objects by the y they stand on. Then entities in the same layer of a canvas that sorts by `'y'` or by depth walk behind and in front of trees and walls. The [drawing rules](../lua-api/tiled.md#drawing-rules) of the reference say exactly how rows and objects sort.

### Keeping the camera on the map

The property `map.pixelBounds` is the world area that the grid covers, which suits the limits of a camera. Tiny Island sets `camera.limits = map.pixelBounds`, so the camera never shows past the edge of the island. The [rendering guide](../graphics/rendering.md) explains canvases, cameras and batching.

## Reading the map

A playable map answers questions about its structure, so the code of the game can read everything the designer put in Tiled.

| Function | Returns |
| --- | --- |
| [`map:layers()`](../lua-api/tiled.md#maplayers) and `map:layer(name)` | Layer tables with their kind, visibility, opacity, blend, offset, parallax, tint and properties. Tile layers add their size, object layers their objects, image layers their image and group layers their children. |
| [`map:objects(layer)`](../lua-api/tiled.md#mapobjectslayer) | The objects of one object layer, or of every object layer, with their class in `type`, their shape, size, rotation, points and properties. |
| [`map:tile(layer, column, row)`](../lua-api/tiled.md#maptilelayer-column-row) and `map:setTile` | The global tile id of a cell, with its flip flags, and a way to change it. |
| [`map:tileInfo(gid)`](../lua-api/tiled.md#maptileinfogid) | The tileset, class, properties, collision shapes and animation of a tile. |
| [`map:tilesets()`](../lua-api/tiled.md#maptilesets) | Every tileset, with its Wang sets. |
| [`map:cellToWorld(column, row)`](../lua-api/tiled.md#mapcelltoworldcolumn-row) and `map:worldToCell(x, y)` | Conversions between cells and world positions, in every orientation. |
| [`map:objectToWorld(x, y)`](../lua-api/tiled.md#mapobjecttoworldx-y) | The world position of object coordinates of the map file. They differ on isometric maps and on skewed oblique maps. |

A global tile id, or gid, numbers a tile across all the tilesets of a map. Tiled stores the flips of a tile in the high bits of its gid, and [`tiled.tileId`](../lua-api/tiled.md#tiledtileidgid) removes them.

The method `map:objects()` returns the raw coordinates of the map file, without the offsets of the layers. The method `map:spawn` hands out world positions that include the offsets of layers and groups, as [spawning objects](#spawning-objects) describes.

The properties of the map are in `map.properties`, and its class is `map.type`. Tiny Island stores the seed that generated its island as the map property `seed`.

## Collision

The method [`map:buildCollision(world)`](../lua-api/tiled.md#mapbuildcollisionworld) creates static bodies in a [`haylen.physics2d`](../lua-api/physics2d.md) world for the collision of the map, and returns them. A tile layer collides through the collision shapes that its tiles have in the collision editor of the tileset. An object layer collides through its objects of class `collision`. A layer whose `collision` property is `false` adds nothing, so a layer of art does not collide.

On orthogonal maps, the solid shapes of touching tiles merge into chain loops around each solid region. A floor of square tiles, a wall and the slope up to it become one outline. Bodies slide along it without catching on the joints between tiles, which separate shapes per tile cause even when their edges line up. The [physics guide](physics.md#bodies-catch-on-the-joints-between-tiles) explains these ghost collisions.

Tiled properties turn shapes into other kinds of collision. A shape whose `sensor` property is `true` becomes a sensor. A shape whose `oneWay` property is `true` becomes a one-way platform, which characters jump through from below and drop through with `body:dropThrough()` or `mover:dropThrough()`. The layer properties `category` and `mask` set the collision filter of a layer. The [reference](../lua-api/tiled.md#mapbuildcollisionworld) lists every rule.

The collision follows the map as it changes. When `map:setTile` changes a cell, it traces the regions around the cell again, so digging, building and breaking tiles change the collision at once. Chain loops have no inside, so a point query inside a solid region finds nothing. A game that asks what fills a cell reads the tile with `map:tile` instead.

```lua
local assets = require('haylen.assets')
local physics2d = require('haylen.physics2d')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/cave.tmj'))
local world = physics2d.newWorld({gravity = {0, 980}, pixelsPerMeter = 64})
local walls = map:buildCollision(world)
-- Digs one cell out of the ground, and the collision follows.
map:setTile('ground', 12, 9, 0)
```

## Spawning objects

The method [`map:spawn(factories, layer)`](../lua-api/tiled.md#mapspawnfactories-layer) turns objects into the entities of the game. Give it a table that maps an object class to a function, a factory. It calls the factory of each object whose class has one, and collects what the factories make. Each object carries `worldX` and `worldY`, its origin in world coordinates with the offsets of its layer and groups. Objects without a factory stay as data, such as markers and collision shapes for other code.

With the object classes of the Tiny Island map, spawning looks like this:

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local spawned = map:spawn({
    campfire = function(object)
        return {kind = 'campfire', x = object.worldX, y = object.worldY}
    end,
    enemy_spawn = function(object)
        return {kind = 'spawn', x = object.worldX, y = object.worldY}
    end,
    wilds = function(object)
        return {kind = 'wilds', x = object.worldX, y = object.worldY, width = object.width, height = object.height}
    end,
}, 'gameplay')
print(#spawned)
```

Tiny Island itself reads its gameplay layer with `map:objects('gameplay')` and checks `object.type`, because it gathers the points into its own tables instead of creating entities one by one. Both ways read the same data. There the raw coordinates are world coordinates, because the object layers of the island have no offsets and the map is orthogonal.

## Ray casts and outlines

A game does not need a physics world to ask what a line hits on a map:

- [`map:raycastTiles`](../lua-api/tiled.md#mapraycasttileslayer-x1-y1-x2-y2-solid) casts a ray over the cells of a tile layer, such as a bullet that stops at walls. A function can tell which tiles are solid.
- [`map:raycastObjects`](../lua-api/tiled.md#mapraycastobjectslayer-x1-y1-x2-y2) casts a ray against the shapes of object layers.
- [`map:objectOutlines`](../lua-api/tiled.md#mapobjectoutlineslayer) lists the closed outlines of objects, ready to become obstacles of a navigation mesh of [`haylen.navigation2d`](../lua-api/navigation2d.md).

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local wall = map:raycastTiles('cliffs', 100, 100, 900, 100)
local tree = map:raycastObjects('decorations', 100, 300, 900, 300)
print(wall and wall.column, tree and tree.name)
```

## Building maps in code

A game can build a map in code, such as a generated dungeon or the map of a level editor. It can then save the map as Tiled JSON, which both the loader and the Tiled editor read back:

1. [`tiled.newMap`](../lua-api/tiled.md#tilednewmapoptions) creates an empty map, which `tiled.newMapRenderer` makes playable.
2. [`map:addTileset`](../lua-api/tiled.md#mapaddtilesetoptions) adds a tileset cut from an image, with optional Wang sets.
3. [`map:addLayer`](../lua-api/tiled.md#mapaddlayeroptions) adds a tile or object layer.
4. [`map:fill`](../lua-api/tiled.md#mapfilllayer-grid-firstgid-column-row) writes a whole grid of cells at once, from a `CellGrid` of [`haylen.spatial2d`](../lua-api/spatial2d.md). On infinite maps, `map:setChunk` and `map:removeChunk` stream chunks in and out.
5. [`map:paintTerrain`](../lua-api/tiled.md#mappaintterrainlayer-wangset-column-row-color) paints with a Wang set, like the terrain brush of the editor.
6. [`map:toJson`](../lua-api/tiled.md#maptojsonpath) returns the map as a table of Tiled JSON, which the `json` module of Varn turns into text.

Every change of cells bakes its layer again on the next draw, and keeps the collision of the layer in step. For a grid that changes every frame and needs none of the features of Tiled, a [`TileLayer`](../lua-api/graphics2d.md#tilelayer) of `haylen.graphics2d` is lighter, because it bakes only the chunks that change.

```lua
local spatial2d = require('haylen.spatial2d')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(tiled.newMap({width = 40, height = 24, tileWidth = 16, tileHeight = 16}))
local first = map:addTileset({name = 'cave', image = 'maps/images/cave.png', tileWidth = 16, tileHeight = 16})
map:addLayer({name = 'ground'})
local floor = spatial2d.newCellGrid(40, 1, 0)
map:fill('ground', floor, first, 0, 23)
```

## Automapping

Automapping applies rules to a map at run time, the way the Tiled editor does when it automaps. A level painted with plain tiles, or made by code, then gets its borders, corners, decorations and objects. Load the rules as an asset of the type `tiledRules` and apply them with [`map:automap(rules, options)`](../lua-api/tiled.md#mapautomaprules-options).

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local rules = assets.load('maps/rules/rules.txt', 'tiledRules')
local map = tiled.newMapRenderer(assets.load('maps/cave.tmj'))
print(map:automap(rules, {seed = 42}) .. ' matches.')
```

### Rules files

Rules come from a rules file or from one rule map. A rules file is a text file with one entry per line:

- A line names a rule map, or another rules file, relative to the rules file.
- A line such as `[town*]` applies the lines after it only to maps whose file name matches, where `*` stands for any text.
- Comments start with `#` or `//`.

### Rule maps

A rule map is a Tiled map that holds the rules in special layers. A rule is each group of cells that touch, on a side or a corner, and hold a tile of an input or output layer, or the origin of an output object.

- **`input_<layer>` and `inputnot_<layer>`**, optionally with an index such as `input2_<layer>`, hold the tiles one of which the layer of the map must hold at each cell, and the tiles it may not hold. Input layers of the same index and layer are one condition, whose tiles are alternatives at each cell. A rule matches when the conditions of any one index hold. Empty cells add nothing, unless the layer property `AutoEmpty` or `StrictEmpty` asks for empty cells where other input layers have tiles. The properties `IgnoreHorizontalFlip`, `IgnoreVerticalFlip`, `IgnoreDiagonalFlip` and `IgnoreHexRotate120` let a layer match flipped and turned tiles too.
- **`output_<layer>`**, optionally with an index such as `output1_<layer>`, holds what a match writes onto the bottom-most layer of that name. That layer joins the map on top when the map lacks it. The outputs without an index always write. Among the others, one index is picked at random, weighted by the property `Probability` of its top layer. An output object layer places its objects with the rule. The other properties of an output layer join the layer it writes.
- **`rule_options`** holds rectangle objects. Their properties `ModX`, `ModY`, `OffsetX`, `OffsetY`, `Probability`, `Disabled` and `NoOverlappingOutput` apply to the rules inside them.

Some tiles have a special meaning through their property `MatchType`, such as the tiles of the automapping rules tileset of Tiled:

| `MatchType` | Matches |
| --- | --- |
| `Empty` | An empty cell. Where it outputs, it erases. |
| `Ignore` | Anything. |
| `NonEmpty` | Any tile. |
| `Other` | Any tile that the rule does not use on that layer. |
| `Negate` | It turns the conditions of its cell around. |

The properties of the rule map `DeleteTiles`, `MatchOutsideMap`, `OverflowBorder`, `WrapBorder`, `MatchInOrder` and `NoOverlappingOutput`, and rule options set on the map, act as in Tiled. Rules match the map as it was before the rule map. They write in the order of their place in the rule map, top to bottom and left to right. With `MatchInOrder`, each rule sees what the rules before it wrote. Each rule map sees what the rule maps before it wrote. A tile from a tileset that the map lacks brings that tileset to the map.

## Worlds

A `.world` file of Tiled places several maps in one large space. It loads as a plain list with one table per map. Each table has the asset `path` of its map, and its `x`, `y`, `width` and `height` in world pixels. The maps that the file lists keep their order, and the maps that its patterns match follow them. The [reference](../lua-api/tiled.md#loading-maps-and-worlds) describes the patterns.

### Placing the maps of a world

Draw each map of a world through the same camera, with the position of the map as the `x` and `y` options of `map:draw`. Culling skips the maps that the camera does not see.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local tiled = require('haylen.tiled')

local camera = graphics2d.newCamera()
local placed = {}
for _, entry in ipairs(assets.load('maps/world/overworld.world')) do
    placed[#placed + 1] = {map = tiled.newMapRenderer(assets.load(entry.path)), x = entry.x, y = entry.y}
end

scene.push({
    update = function(self, dt)
        camera.x = camera.x + 100 * dt
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        for _, part in ipairs(placed) do
            part.map:draw(camera, {x = part.x, y = part.y})
        end
    end,
})
```

The cells and object positions of a placed map stay in map coordinates. So add the position of the map to what `map:cellToWorld` and `map:objectToWorld` return, and subtract it before `map:worldToCell`.

### Streaming a large world

A world larger than memory keeps only the maps near the camera. This works well for three reasons. The list of a `.world` file costs only a few numbers per map. The function `assets.loadAsync` parses a map and decodes its images on worker threads. A map that nothing refers to any more leaves the cache. So the frame never waits for the disk, and memory follows the camera.

Load a map when its area comes within a margin of the visible area. Forget it once it is farther than a larger margin. Then a camera that moves back and forth along an edge never loads the same map again and again.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')
local scene = require('haylen.scene')
local tiled = require('haylen.tiled')

local camera = graphics2d.newCamera()
local entries = assets.load('maps/world/overworld.world')
local loaded, loading = {}, {}

-- Whether the area of an entry overlaps the visible area grown by a margin.
local function near(entry, margin)
    return m.rect(entry.x, entry.y, entry.width, entry.height):intersects(camera:visibleBounds():expanded(margin))
end

scene.push({
    update = function(self, dt)
        camera.x = camera.x + 300 * dt
        for _, entry in ipairs(entries) do
            if not loaded[entry] and not loading[entry] and near(entry, 512) then
                loading[entry] = true
                scene.spawn(self, function()
                    local data = assets.loadAsync(entry.path):await()
                    loading[entry] = nil
                    loaded[entry] = data and tiled.newMapRenderer(data)
                end)
            elseif loaded[entry] and not near(entry, 1024) then
                loaded[entry] = nil
            end
        end
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        for entry, map in pairs(loaded) do
            map:draw(camera, {x = entry.x, y = entry.y})
        end
    end,
})
```

The stress sample `samples/stress/streamed-map` streams the chunks of one infinite map the same way. It uses the tile layers of `graphics2d.newTileLayer`, and the decorations that `procedural2d.scatterAsync` places off the frame thread.

## The Tiny Island map

The island of the sample Tiny Island lives in `samples/games/tiny-island/content/maps/`, as `island.tmj` and three external tilesets. The tool `samples/games/tiny-island/tools/generate-island-map.py` writes all of them from seeded noise, and they open and edit in Tiled 1.12. The file `samples/games/tiny-island/source/systems/island.lua` loads the map with the `linear` filter, because the art is painted in high definition, and gives the rest of the game what it needs from it.

The map is orthogonal, 56 by 36 cells of 64 pixels. Its background color is the water color `#2fb3c4`, which matches the `clearColor` in `app.json`, so the sea fills the screen around the map.

| Layer | Kind | Content |
| --- | --- | --- |
| `water` | Image | The sea, a water image that repeats on both axes. |
| `foam` | Tile | Animated foam under every coast cell. |
| `sand` | Tile | The beach, corner tiles of the land. |
| `grass` | Tile | The meadow, corner tiles of the land one cell in from the coast. |
| `path` | Tile | Worn dirt paths from a clearing around the fire to three beaches. |
| `shadow` | Tile | The shadow the high ground casts, the high ground shifted down and to the right. |
| `cliffs` | Tile | The stone face of the high ground, the high ground shifted one cell down so only its front shows. |
| `plateau` | Tile | The high ground in the north east, with a straight cliff along its south side. |
| `decorations` | Object | Bushes, rocks, grass, flowers, mushrooms, shells, driftwood and rocks in the water, as tile objects of class `decoration`. |
| `gameplay` | Object | Points and regions that the game reads by class. |
| `collision` | Object | Rectangles that block movement, in a hidden layer of class `collision`. |
| `clouds` | Object | Cloud tile objects of class `cloud`, with opacity 0.45 and parallax 1.25. |

The tilesets use the art in the `content/world/` folder of the sample:

- The file `terrain.tsj` is an image tileset of 64-pixel tiles made for corner layers. These layers sit half a tile right and down, so every tile takes the state of the four cells around its corners. Each material, sand, grass, high ground, path and the stone face, has the fifteen shapes of a corner tile in four copies. Its texture repeats every two tiles, and each copy holds the part of the texture of its position. The tileset holds four corner Wang sets, `Sand`, `Grass`, `High ground` and `Path`, so the island can be painted again by hand with the terrain brush of Tiled. It also holds a row of shadow shapes.
- The file `foam.tsj` holds 128-pixel tiles with a tile offset of `(-32, 32)`, which centers each large tile on its 64-pixel cell. The foam tile animates through four frames.
- The file `decorations.tsj` is an image collection whose tiles are regions of the atlases of the props and the clouds. Its `objectalignment` is `bottom`, so decorations stand on their point.

The game relies on these conventions:

- Every tile layer has the property `collision` set to `false`. So `map:buildCollision` ignores the art and takes collision only from the `collision` layer. Those rectangles are merged from blocked cells, the water, the cliffs and the plateau. They collide with the default category 1, which is `config.category.world` in the game.
- The `gameplay` layer holds one point of class `campfire` and one point of class `player_start`. It holds twelve points of class `enemy_spawn` on walkable coast cells, at twelve bearings around the fire. It holds four rectangles of class `wilds`, one per quarter of the island, where trees grow and sheep graze.
- The `campfire` point sits on a walkable grass cell at or near the middle of the island, and the player starts two cells south of it.
- Trees and sheep stay off the paths, which the game finds with `map:tile` on the `path` layer.

The file `island.lua` uses these conventions in a few lines. It builds the physics collision with `map:buildCollision`. It marks every cell under a `collision` rectangle as blocked in a [navigation grid](../lua-api/navigation2d.md) with `map:worldToCell`. It collects the gameplay points by class, and it scatters trees with Poisson disk sampling inside the wild regions, away from the fire.

```lua
local assets = require('haylen.assets')
local navigation2d = require('haylen.navigation2d')
local physics2d = require('haylen.physics2d')
local tiled = require('haylen.tiled')

local island = {spawns = {}}
island.map = tiled.newMapRenderer(assets.load('maps/island.tmj', nil, {filter = 'linear'}))
island.world = physics2d.newWorld({gravity = {0, 0}, pixelsPerMeter = 64})
island.walls = island.map:buildCollision(island.world)

island.grid = navigation2d.newGrid(island.map.width, island.map.height)
for _, wall in ipairs(island.map:objects('collision')) do
    local left, top = island.map:worldToCell(wall.x, wall.y)
    local right, bottom = island.map:worldToCell(wall.x + wall.width - 1, wall.y + wall.height - 1)
    for row = top, bottom do
        for column = left, right do
            island.grid:setWalkable(column, row, false)
        end
    end
end

for _, object in ipairs(island.map:objects('gameplay')) do
    if object.type == 'campfire' then
        island.fire = {x = object.x, y = object.y}
    elseif object.type == 'enemy_spawn' then
        island.spawns[#island.spawns + 1] = {x = object.x, y = object.y}
    end
end
```

### Regenerating the map

```sh
python3 samples/games/tiny-island/tools/generate-island-map.py
```

The tool of the sample writes `island.tmj`, `terrain.tsj`, `foam.tsj` and `decorations.tsj` into the `content/maps` folder of the sample that holds it, with the default seed `20260927`. The option `--package` names another package to write into. The tilesets use the images and atlases in the `content/world/` folder of the package, and the tool stops with a message when one is missing. The same seed always gives the same island. Another island comes from a seed of its own:

```sh
python3 samples/games/tiny-island/tools/generate-island-map.py --seed 7
```

Regenerating overwrites the four files, with any edit made to them in Tiled. The game reads whatever `island.tmj` holds. So an island edited by hand works, as long as it keeps the layer names and object classes above.

## From C++

The runtime is plain C++ under `haylen/2d/tiled/`, in the namespace `haylen::tiled`:

- The class `tiled::Map`, in `Map.hpp`, holds the parsed data. Each part of the model has a header of its own, such as `Layer`, `Object`, `Tileset`, `Tile`, `WangSet` and `Properties`.
- The class `tiled::World`, in `World.hpp`, reads `.world` files.
- The class `tiled::MapRenderer`, in `MapRenderer.hpp`, is the class behind the Lua `MapRenderer`. It draws a map through a `graphics2d::Renderer`, visits objects with `forEachObject` and builds collision into a `physics2d::World`.
- The class `tiled::TileCollision`, in `TileCollision.hpp`, keeps the collision of one tile layer on one body. It merges the solid shapes of touching cells into chain loops, and traces the regions around a changed cell again.
- The class `tiled::ObjectFactories`, in `ObjectFactories.hpp`, registers one factory per object class and spawns them, like `map:spawn` does in Lua.
- The class `tiled::AutoMapper`, in `AutoMapper.hpp`, applies automapping rules with `AutoMapper::apply`, like `map:automap` does in Lua.
- With a `.tiled-project` file, `tiled::PropertyTypes` and `Map::applyPropertyTypes` fill in the members of classes that a map leaves at their defaults.
