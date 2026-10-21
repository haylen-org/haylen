# 📖 `haylen.tiled`

The module `haylen.tiled` plays maps made with the [Tiled](https://www.mapeditor.org) map editor. Use it whenever a level is designed in Tiled, or when a game builds a Tiled map in code. The [Tiled guide](../2d/tiled.md) explains how to author maps, which features of Tiled work, and how drawing, collision, spawning, automapping and worlds fit together.

```lua
local tiled = require('haylen.tiled')
```

## Loading maps and worlds

Maps, worlds and automapping rules are assets of [`haylen.assets`](assets.md):

| Asset type | Files | What it loads |
| --- | --- | --- |
| `tiled` | `.tmj` | A [`Map`](#map), the data of one map with its tilesets, templates and images. |
| `tiledWorld` | `.world` | A plain list of the maps of a world. |
| `tiledRules` | None, so pass the type by name | An [`AutoMapper`](#automapper), the rules of [automapping](#automapping). |

The functions `assets.load(path, nil, options)` and `assets.loadAsync(path, nil, options)` take these options for a map. An unknown key raises `Unknown option "<key>".`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `filter` | string | `'nearest'` | The texture filter of every image of the map, `'nearest'` or `'linear'`. |
| `wrap` | string | `'clamp'` | The texture wrap of every image of the map, `'clamp'`, `'repeat'` or `'mirror'`. |
| `mipmaps` | boolean | `false` | Whether every image of the map keeps smaller copies of itself, as for [`assets.texture`](assets.md#assetstexturepath-options). |
| `project` | string | none | The path of the `.tiled-project` file of the map inside the content folder, as [Tiled projects](#tiled-projects) describes. |

The images of a map share the texture cache with `assets.texture`. The one exception is an image with a transparent color, which gets a private texture with that color keyed out. The function `assets.loadAsync` parses the map and decodes its images on worker threads, and returns a promise.

```lua
local assets = require('haylen.assets')
local async = require('async')
local log = require('haylen.log')
local tiled = require('haylen.tiled')

local map = nil

async.spawn(function()
    local data, failure = assets.loadAsync('maps/island.tmj', nil, {filter = 'linear'}):await()
    if not data then
        log.error(failure)
        return
    end
    map = tiled.newMapRenderer(data)
end)
```

### Tiled projects

Tiled writes only the members of a custom class that differ from their defaults. This holds for class values, and for the properties of a map, layer, tileset, tile, Wang set or object whose class it is. The option `project` names the `.tiled-project` file of the map inside the content folder. Its classes fill in the missing members with their defaults, in nested classes and in the class items of lists too. So `map.properties.spawn.reward.coins` holds the default that the project gives it.

These errors can happen:

- A `project` that is not a path raises `The "project" of a Tiled map is the path of its ".tiled-project" file.`.
- A class that holds itself raises `The class "<name>" of the Tiled project holds itself.`.
- A class without a name raises `A class of the Tiled project has no name.`, and a member without a name raises `A member of the class "<name>" of the Tiled project has no name.`.

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj', nil, {project = 'maps/game.tiled-project', filter = 'linear'}))
print(map.properties.difficulty)
```

### World files

A `.world` file loads as a plain list with one table per map. Each table has these fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `path` | string | The path of the map inside the content folder. |
| `x`, `y` | number | The place of the map in world pixels. |
| `width`, `height` | number | The size of the map in world pixels. |

The maps that the file lists in `maps` keep their order, and the maps that `patterns` match follow them. The `regexp` of a pattern is matched against the files in the folder of the world. Its first two capture groups are multiplied by `multiplierX` and `multiplierY` and moved by `offsetX` and `offsetY`, which gives the place of the map. The fields `mapWidth` and `mapHeight` give its size. A world takes no options, and any key raises `Unknown option "<key>".`. The [guide](../2d/tiled.md#worlds) shows how to draw and stream the maps of a world.

```lua
local assets = require('haylen.assets')

local world = assets.load('maps/world/overworld.world')
for _, entry in ipairs(world) do
    print(entry.path, entry.x, entry.y, entry.width, entry.height)
end
```

## Functions

### tiled.newMapRenderer(asset)

Creates a playable [`MapRenderer`](#maprenderer) from a loaded [`Map`](#map). The playable map owns its own copy of the data. So changes such as `map:setTile` never touch the cached asset, and several maps made from one asset are independent.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local camera = graphics2d.newCamera()
camera.limits = map.pixelBounds
camera:snapTo(map.pixelBounds.width / 2, map.pixelBounds.height / 2)

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

### tiled.newMap(options)

Creates an empty [`Map`](#map) for a map that code builds, such as a generated dungeon. The function `tiled.newMapRenderer` makes it playable, and the methods of [Building maps at run time](#building-maps-at-run-time) give it tilesets, layers and cells. The table `options` is required, and an unknown key raises `Unknown option "<key>".`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `width`, `height` | integer | `0` | Size of a finite map in cells, which must be positive. |
| `tileWidth`, `tileHeight` | number | `0` | Size of a cell in pixels, which must be positive. |
| `orientation` | string | `'orthogonal'` | One of `'orthogonal'`, `'isometric'`, `'staggered'`, `'hexagonal'` and `'oblique'`. |
| `renderOrder` | string | `'right-down'` | One of `'right-down'`, `'right-up'`, `'left-down'` and `'left-up'`. |
| `infinite` | boolean | `false` | Makes tile layers hold chunks, which `map:setChunk` adds, instead of one grid of the size of the map. |
| `staggerAxis`, `staggerIndex` | string | `'y'`, `'odd'` | The axis, `'x'` or `'y'`, and the cells, `'odd'` or `'even'`, that staggered and hexagonal maps shift. |
| `hexSideLength` | number | `0` | Length of the flat side of a hexagonal cell. |
| `backgroundColor` | Color | none | The background color of the map. |

These errors can happen:

- A finite map without a positive size, or a tile size that is not positive, raises `A map needs a positive tile size, and a finite map a positive width and height in cells.`.
- An unknown orientation or render order raises `A map has the orientation "orthogonal", "isometric", "staggered", "hexagonal" or "oblique" and the render order "right-down", "right-up", "left-down" or "left-up".`.
- An unknown stagger axis or index raises `A map staggers along the axis "x" or "y" from the "odd" or "even" cells.`.

```lua
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(tiled.newMap({width = 64, height = 40, tileWidth = 16, tileHeight = 16}))
print(map.width, map.pixelBounds.width) -- 64 1024.0
```

### tiled.tileId(gid)

Returns the global tile id `gid` without its [flip flags](#flip-flags), which is the global id of the tile itself.

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local gid = map:tile('ground', 2, 2)
local flipHorizontal = (gid & tiled.flipHorizontal) ~= 0
print(tiled.tileId(gid), flipHorizontal, gid & tiled.flagMask)
map:setTile('ground', 2, 2, tiled.tileId(gid) | tiled.flipVertical)
```

## Flip flags

Tiled stores the flips of a tile in the high bits of its global tile id. The module has a constant for each flag. Combine them with `|`, test them with `&`, and remove them with [`tiled.tileId`](#tiledtileidgid).

| Constant | Value | Meaning |
| --- | --- | --- |
| `tiled.flipHorizontal` | `0x80000000` | The tile is mirrored horizontally. |
| `tiled.flipVertical` | `0x40000000` | The tile is mirrored vertically. |
| `tiled.flipDiagonal` | `0x20000000` | The tile is mirrored across its diagonal, which with the other flips turns it in steps of 90 degrees. |
| `tiled.rotateHexagonal` | `0x10000000` | The tile of a hexagonal map is turned by 120 degrees. |
| `tiled.flagMask` | `0xF0000000` | Every flag bit. |

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local gid = map:tile('ground', 4, 4)
-- Turns the tile by 90 degrees clockwise: a diagonal flip, then a horizontal one.
map:setTile('ground', 4, 4, tiled.tileId(gid) | tiled.flipDiagonal | tiled.flipHorizontal)
print(tiled.rotateHexagonal == 0x10000000)
```

## Map

A `Map`, named `haylen.Map` in messages, is the data of a map. Loading a `.tmj` file gives one, and so does `tiled.newMap`. Make it playable with [`tiled.newMapRenderer`](#tilednewmaprendererasset). It is cached by path like any asset, so two loads of the same map compare equal with `==`.

### Map properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `path` | string | read | The path of the map inside the content folder, or an empty string for a map made with `tiled.newMap`. |

```lua
local assets = require('haylen.assets')

local data = assets.load('maps/island.tmj')
print(data.path, data == assets.load('maps/island.tmj')) -- maps/island.tmj true
```

## AutoMapper

An `AutoMapper`, named `haylen.AutoMapper` in messages, holds loaded automapping rules. Load one as an asset of the type `tiledRules`, from a rules file or from one rule map, and apply it with [`map:automap`](#mapautomaprules-options). The [guide](../2d/tiled.md#automapping) explains rules files and rule maps. The tileset images of its rule maps load with the texture options `filter`, `wrap` and `mipmaps`.

### AutoMapper properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `ruleMaps` | table | read | A list of the rule maps, in the order they apply. Each item has `path`, the path of the rule map, and `filter`, the map names it applies to, such as `town*`. The filter is an empty string when the rule map applies to every map. |

```lua
local assets = require('haylen.assets')

local rules = assets.load('maps/rules/rules.txt', 'tiledRules')
for _, ruleMap in ipairs(rules.ruleMaps) do
    print(ruleMap.path, ruleMap.filter)
end
```

## MapRenderer

A `MapRenderer`, named `haylen.MapRenderer` in messages, is a playable map. The function [`tiled.newMapRenderer`](#tilednewmaprendererasset) makes one. It has many methods, which the sections after this one describe by topic:

| Topic | Methods |
| --- | --- |
| [Drawing](#drawing) | [`map:draw`](#mapdrawcamera-options), [`map:drawLayer`](#mapdrawlayername-camera-options), [`map:update`](#mapupdatedt) and [`map:setLayerVisible`](#mapsetlayervisiblename-visible) |
| [Cells and tiles](#cells-and-tiles) | [`map:tile`](#maptilelayer-column-row), [`map:setTile`](#mapsettilelayer-column-row-gid), [`map:cellToWorld`](#mapcelltoworldcolumn-row), [`map:worldToCell`](#mapworldtocellx-y), [`map:objectToWorld`](#mapobjecttoworldx-y), [`map:tileInfo`](#maptileinfogid) and [`map:tilesets`](#maptilesets) |
| [Layers and objects](#layers-and-objects) | [`map:layer`](#maplayername), [`map:layers`](#maplayers), [`map:objects`](#mapobjectslayer) and [`map:spawn`](#mapspawnfactories-layer) |
| [Collision, ray casts and outlines](#collision-ray-casts-and-outlines) | [`map:buildCollision`](#mapbuildcollisionworld), [`map:raycastTiles`](#mapraycasttileslayer-x1-y1-x2-y2-solid), [`map:raycastObjects`](#mapraycastobjectslayer-x1-y1-x2-y2) and [`map:objectOutlines`](#mapobjectoutlineslayer) |
| [Building maps at run time](#building-maps-at-run-time) | [`map:addTileset`](#mapaddtilesetoptions), [`map:addLayer`](#mapaddlayeroptions), [`map:removeLayer`](#mapremovelayername), [`map:fill`](#mapfilllayer-grid-firstgid-column-row), [`map:setChunk`](#mapsetchunklayer-column-row-grid-firstgid), [`map:removeChunk`](#mapremovechunklayer-column-row), [`map:paintTerrain`](#mappaintterrainlayer-wangset-column-row-color) and [`map:toJson`](#maptojsonpath) |
| [Automapping](#automapping) | [`map:automap`](#mapautomaprules-options) |

### MapRenderer properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `path` | string | read | The path of the map inside the content folder. |
| `width`, `height` | integer | read | The size of the map in cells. |
| `tileWidth`, `tileHeight` | number | read | The size of a cell of the grid in pixels. |
| `pixelBounds` | Rect | read | The world area that the grid covers. On oblique maps with a negative skew it starts left of or above 0. |
| `orientation` | string | read | `'orthogonal'`, `'isometric'`, `'staggered'`, `'hexagonal'` or `'oblique'`. |
| `skewX`, `skewY` | number | read | The skew of an oblique map in pixels. |
| `backgroundColor` | Color or nil | read | The background color of the map, or `nil` when the map has none. |
| `infinite` | boolean | read | The value is `true` for infinite maps, whose tile layers are stored in chunks. |
| `renderOrder` | string | read | `'right-down'`, `'right-up'`, `'left-down'` or `'left-up'`. |
| `hexSideLength` | integer | read | The length of the flat side of hexagonal tiles, 0 on other maps. |
| `staggerX` | boolean | read | The value is `true` when a staggered or hexagonal map shifts every other column, and `false` when it shifts every other row. |
| `staggerEven` | boolean | read | The value is `true` when the even columns or rows are shifted, and `false` when the odd ones are. |
| `parallaxOrigin` | Vec2 | read | The point of the map that parallax layers are measured from. |
| `type` | string | read | The class of the map. |
| `properties` | table | read | The custom properties of the map, as [custom properties](#custom-properties) describes. |
| `propertyTypes` | table | read | The custom property type names of the map properties. |

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
print(map.path, map.width, map.height, map.tileWidth, map.orientation, map.type)
print(map.infinite, map.renderOrder, map.hexSideLength, map.staggerX, map.staggerEven, map.parallaxOrigin.x, map.skewX, map.skewY)
print(map.backgroundColor, map.pixelBounds.width)
```

### Custom properties

Custom properties become a table from name to value:

- Strings, numbers and booleans keep their type.
- Colors become `Color` values, or `nil` when the color is unset.
- Files become paths inside the content folder.
- Object references become object ids.
- Class values become nested tables of their members. Their colors stay `#AARRGGBB` text, because Tiled stores the members of a class without their types.
- Lists become sequences of their items, converted by the same rules. So a list of colors holds `Color` values, and a list of lists holds sequences.

Every table that has `properties` also has `propertyTypes`. It maps a property name to the name of its custom property type, such as the class of a class property or the enum of an enum property. Properties of a plain type are absent from it.

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
for name, value in pairs(map.properties) do
    print(name, value, map.propertyTypes[name])
end
local loot = map.properties.loot
if loot then
    for index, item in ipairs(loot) do
        print(index, item)
    end
end
```

## Drawing

### Drawing rules

The methods `map:draw` and `map:drawLayer` follow these rules:

- **Tile layers** bake into static batches, in regions of 32 by 32 cells, the first time they draw. When a camera is given, only the regions inside the area that the active canvas shows are drawn. That is the visible area around the camera on a world canvas, and the size of the target around it on a render target canvas. The method `map:setTile` bakes only the region of its cell again, on the next draw. Animated tiles are drawn every frame, with the frame that matches the time of the map.
- **Object layers** draw their visible tile objects and text objects. Other shapes are data for the app and for collision. Objects sort by their y position, unless the draw order of the layer is `index`. Tile objects are placed by the object alignment of their tileset, which is bottom-left by default, or bottom-center on isometric maps.
- **Text objects** use the default font of the engine, with their size, color, wrapping, alignment, rotation, opacity, bold, italic, underline and strikeout. Their font family names a font that the map renderer does not hold, and their kerning stays on.
- **Image layers** draw their image at the offset of the layer. A layer that repeats on an axis covers the visible area of the camera, so it needs a camera. Without one, drawing raises `Repeated image layers need the visible area of the view.`.
- **Parallax**: A layer with a parallax factor other than 1 shifts by the distance between the camera position and the parallax origin of the map, times one minus the factor. On a map drawn with an offset, the parallax origin moves with the map.
- **Order and blends**: Every layer draws with the draw order given to `map:draw` or `map:drawLayer`, with the blend replaced by the blend mode of the layer. Later map layers cover earlier ones, because they draw later. A layer whose blend mode mixes with the colors below it draws into a [group](graphics2d.md#graphics2dbegingroupoptions), which works only in a canvas without ambient light. In a lit canvas, such a layer raises `A group that blends with what lies below draws in a canvas without ambient light.`.
- **Y sorting**: With the option `ysort`, tile layers bake and draw row by row, where a row is the cells that stand on the same y. That is a row of an orthogonal map and a diagonal of an isometric one. Object layers draw object by object. Every row and every object takes as its depth the y it stands on, the bottom of its cells or of its object. It also sorts by that y in canvases that sort by `'y'`. So entities drawn in the same layer of a canvas that sorts by y or by depth pass behind and in front of trees, walls and other map objects.
- **Placement**: The options `x` and `y` place the origin of the map in the world, so several maps share one camera. Culling, parallax and repeated images follow the placed map.

### map:draw(camera, options)

Draws every visible layer of the map, in map order, into the active canvas, as the [drawing rules](#drawing-rules) describe.

The argument `camera` is optional. With it, parallax layers follow its position, and culling and repeated images use the area that the active canvas shows. Without it, nothing is culled, and parallax layers shift as if the camera stood at the world origin.

The table `options` is optional. It takes the [draw order](graphics2d.md#draw-order) keys `layer`, `depth`, `sortOffset`, `visibility` and `blend` of `haylen.graphics2d`, where each layer replaces `blend` with its own blend mode. It also takes `x` and `y`, the world position of the origin of the map, 0 by default, and `ysort`, which draws tile and object layers sorted by the y they stand on. Another key raises `Unknown option "<key>".`.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        map:draw(camera, {layer = 0, x = 0, y = 0})
    end,
})
```

### map:drawLayer(name, camera, options)

Draws one layer, with the children of a group layer. The layer keeps the offset, parallax, tint and visibility that it inherits from the groups above it. The arguments `camera` and `options` work as in `map:draw`. Drawing layers one by one lets the app place its own sprites between them. An unknown name raises `The map has no layer named "<name>".`.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local camera = graphics2d.newCamera()
local hero = graphics2d.newSprite(assets.texture('sprites/knight/idle.png'), {source = {0, 0, 192, 192}, x = 800, y = 600, layer = 1})

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        map:drawLayer('ground', camera, {layer = 0})
        hero:draw()
        map:drawLayer('decorations', camera, {layer = 2})
    end,
})
```

With `ysort`, a layer of trees and walls shares one layer with the entities, and every entity sorts among its rows by its feet:

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local camera = graphics2d.newCamera()
local hero = graphics2d.newSprite(assets.texture('sprites/knight/idle.png'), {source = {0, 0, 192, 192}, x = 800, y = 600, pivotY = 0.7, sortOffset = 50, layer = 1})

scene.push({
    update = function(self, dt)
        hero.y = 600 + math.sin(os.clock()) * 200
    end,
    render = function(self)
        graphics2d.beginWorld(camera, {sort = 'y'})
        map:drawLayer('ground', camera, {layer = 0})
        map:drawLayer('decorations', camera, {layer = 1, ysort = true})
        hero:draw()
    end,
})
```

### map:update(dt)

Moves the time of animated tiles on by `dt` seconds. Call it from the `update` callback of a scene.

```lua
local assets = require('haylen.assets')
local scene = require('haylen.scene')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))

scene.push({
    update = function(self, dt)
        map:update(dt)
    end,
})
```

### map:setLayerVisible(name, visible)

Shows or hides a layer. Hiding a group also hides its children. An unknown name raises `The map has no layer named "<name>".`.

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
map:setLayerVisible('shadow', false)
print(map:layer('shadow').visible) -- false
```

## Cells and tiles

Columns and rows count from 0. A global tile id, or gid, numbers a tile across all the tilesets of the map, with its [flip flags](#flip-flags) in its high bits.

### map:tile(layer, column, row)

Returns the global tile id at a cell of the tile layer `layer`, with its flip flags. It returns 0 for an empty cell and for a cell outside the layer. A name that is not a tile layer raises `The map has no tile layer named "<name>".`.

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local gid = map:tile('path', 10, 12)
print(gid ~= 0 and 'On the path.' or 'Off the path.')
```

### map:setTile(layer, column, row, gid)

Replaces the tile at a cell. The argument `gid` is a global tile id, optionally with flip flags added, and 0 empties the cell. The region of the cell bakes again on the next draw. The collision that [`map:buildCollision`](#mapbuildcollisionworld) built for the layer follows the cell.

These errors can happen:

- A gid that no tileset holds raises `The tile "<gid>" belongs to no tileset of the map.`.
- A cell outside the layer raises `The cell is outside the tile layer.`. On an infinite map, it raises `The cell is outside every chunk of the infinite tile layer.`.
- A name that is not a tile layer raises `The map has no tile layer named "<name>".`.

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local gid = map:tile('ground', 10, 10)
if gid ~= 0 then
    map:setTile('ground', 11, 10, gid)
    map:setTile('ground', 12, 10, gid | tiled.flipHorizontal)
end
map:setTile('ground', 13, 10, 0)
```

### map:cellToWorld(column, row)

Returns the world position `x, y` of a cell. That is its top-left corner, or the top corner of the diamond on isometric maps.

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local x, y = map:cellToWorld(3, 2)
print(x, y) -- 192.0 128.0
```

### map:worldToCell(x, y)

Returns the `column, row` of the cell under a world position. Staggered and hexagonal maps pick the cell whose center is nearest.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        map:draw(camera)
        local worldX, worldY = camera:screenToWorld(input.mousePosition())
        local column, row = map:worldToCell(worldX, worldY)
        local cellX, cellY = map:cellToWorld(column, row)
        graphics2d.drawRectOutline({cellX, cellY, map.tileWidth, map.tileHeight}, 2, '#FFFFFF00', {layer = 1})
    end,
})
```

### map:objectToWorld(x, y)

Converts object coordinates of the map file to a world position, and returns `x, y`. The two differ on isometric maps, whose object coordinates run along the edges of the diamonds, and on skewed oblique maps.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local camera = graphics2d.newCamera()
local start = map:objects('gameplay')[1]

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        map:draw(camera)
        local spawnX, spawnY = map:objectToWorld(start.x, start.y)
        graphics2d.drawCircle(spawnX, spawnY, 6, '#FFFF4040', {layer = 1})
    end,
})
```

### map:tileInfo(gid)

Describes a global tile id. It returns `nil` for 0 and for an id that no tileset holds. The table has these fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | integer | The id of the tile inside its tileset. |
| `tileset` | string | The name of the tileset. |
| `flipHorizontal`, `flipVertical`, `flipDiagonal` | boolean | The flip flags of the gid, named like the flips of a sprite. |
| `rotateHexagonal` | boolean | The 120-degree rotation flag of the gid, which hexagonal maps use. |
| `source` | Rect | The rectangle of the tile inside its image. |
| `type` | string | The class of the tile, or an empty string. |
| `image` | string | The image path of a tile from an image collection, or an empty string. |
| `probability` | number | The chance of the tile in the terrain and random tools of Tiled, 1 by default. |
| `properties`, `propertyTypes` | table | The custom properties of the tile and their type names. |
| `collision` | table | The collision shapes as object tables, relative to the tile, in the pixels of its image. |
| `animation` | table | The animation frames as `{tileId = id, duration = seconds}` tables. It is empty for a still tile. |

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local info = map:tileInfo(map:tile('ground', 3, 3))
if info then
    print(info.tileset, info.id, info.type, info.probability, #info.collision, #info.animation, tostring(info.properties.water))
end
```

### map:tilesets()

Returns the tilesets of the map, in the order of their first gid. Every tileset table has these fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `name` | string | The name of the tileset. |
| `type` | string | The class of the tileset. |
| `path` | string | The path of an external tileset file inside the content folder, or an empty string for a tileset embedded in the map. |
| `firstGid` | integer | The global id of the first tile. |
| `tileWidth`, `tileHeight` | number | The size of a tile in pixels. |
| `columns`, `tileCount` | integer | The columns of the tileset image and the number of tiles. |
| `margin`, `spacing` | integer | The pixels around the tiles and between them in the image. |
| `tileOffset` | Vec2 | The offset applied when the tiles are drawn. |
| `objectAlignment` | string | The anchor of tile objects, such as `'unspecified'`, `'bottomleft'`, `'bottom'` or `'center'`. |
| `renderGridSize` | boolean | The value is `true` when tiles draw at the size of the map grid instead of the size of their image, with their offsets and collision shapes scaled the same way. |
| `preserveAspect` | boolean | The value is `true` when tiles drawn at the size of the grid keep their aspect ratio. |
| `grid` | table | The grid Tiled places tile objects of the tileset on, with `orientation`, `'orthogonal'` or `'isometric'`, and `width` and `height`, which are the tile size unless the tileset sets another. |
| `transformations` | table | How Wang painting may turn the tiles, with `flipHorizontal`, `flipVertical`, `rotate` and `preferUntransformed`, all `false` unless the tileset allows them. |
| `image` | string | The image path inside the content folder, or an empty string for an image collection. |
| `imageSize` | Vec2 | The image size written in the tileset. |
| `transparentColor` | Color or nil | The color drawn as transparent, or absent. |
| `texture` | Texture or nil | The loaded tileset image, absent for an image collection. |
| `properties`, `propertyTypes` | table | The custom properties and their type names. |
| `wangSets` | table | The Wang sets. |

Each Wang set has `name`, `type`, its class, `kind`, which is `'corner'`, `'edge'` or `'mixed'`, `tile`, the tile that stands for it or -1, `properties`, `propertyTypes`, `colors` and `tiles`. Each color has `name`, `type`, its class, `color`, `tile`, `probability`, `properties` and `propertyTypes`. Each tile has `tileId` and an eight-number `wangId`.

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
for _, tileset in ipairs(map:tilesets()) do
    print(tileset.name, tileset.firstGid, tileset.tileCount, tileset.margin, tileset.spacing, tileset.objectAlignment, tileset.imageSize.x, #tileset.wangSets)
    for _, set in ipairs(tileset.wangSets) do
        for _, color in ipairs(set.colors) do
            print(set.name, set.type, color.name, color.type, color.probability)
        end
    end
end
```

## Layers and objects

### map:layer(name)

Returns a table that describes the layer `name`, searching group layers depth first. The table is a copy, so changing it does not change the map. An unknown name raises `The map has no layer named "<name>".`. The table has the fields of [`map:layers`](#maplayers).

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local ground = map:layer('ground')
print(ground.width, ground.height, ground.properties.collision)
for _, chunk in ipairs(ground.chunks) do
    print(chunk.x, chunk.y, chunk.width, chunk.height, #chunk.gids)
end
```

### map:layers()

Returns the list of the top-level layers as layer tables. Group layers hold their children in `layers`. Every layer table has these fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | integer | The id of the layer. |
| `name` | string | The name of the layer. |
| `type` | string | The class of the layer. |
| `kind` | string | `'tile'`, `'object'`, `'image'` or `'group'`. |
| `visible` | boolean | The visibility, with the changes of `map:setLayerVisible`. |
| `opacity` | number | The opacity from 0 to 1. |
| `blend` | string | The blend mode: `'alpha'`, `'additive'`, `'multiply'`, `'screen'`, `'overlay'`, `'darken'`, `'lighten'`, `'colorDodge'`, `'colorBurn'`, `'hardLight'`, `'softLight'`, `'difference'` or `'exclusion'`. |
| `offsetX`, `offsetY` | number | The offset of the layer in pixels. |
| `parallaxX`, `parallaxY` | number | The parallax factors. |
| `tint` | Color | The tint color. |
| `properties`, `propertyTypes` | table | The custom properties and their type names. |
| `width`, `height` | integer | Tile layers only. The size of the layer in cells. |
| `chunks` | table | Tile layers only. The chunks of an infinite map, each with `x` and `y`, its first cell, `width`, `height` and `gids`, the global tile ids row by row with their flip flags. It is empty on other maps. |
| `infinite` | boolean | Tile layers only. Whether the layer keeps its cells in `chunks`, as every tile layer of an infinite map does, even once it has no chunk left. Such a layer has no cells outside its chunks, and its `width` and `height` are the size that its file gave it. |
| `objects` | table | Object layers only. The list of object tables, as [`map:objects`](#mapobjectslayer) describes them. |
| `indexDrawOrder` | boolean | Object layers only. The value is `true` when objects draw in list order, and `false` when they sort by their y position. |
| `color` | Color or nil | Object layers only. The color in which Tiled shows the objects of the layer, for the app's own drawings of the layer, or `nil` when the layer keeps the default. |
| `image`, `repeatX`, `repeatY` | string, boolean, boolean | Image layers only. The image path inside the content folder, and whether it repeats. |
| `imageSize` | Vec2 | Image layers only. The image size written in the map. |
| `transparentColor` | Color or nil | Image layers only. The color drawn as transparent, or absent. |
| `layers` | table | Group layers only. The list of the child layer tables. |

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
for _, layer in ipairs(map:layers()) do
    print(layer.name, layer.kind, layer.blend, layer.opacity)
end
```

### map:objects(layer)

Returns the objects of the object layer `layer`, or of every object layer in map order when `layer` is left out. The coordinates are the raw values of the map file, without the offsets of the layers. A name that is not an object layer raises `The map has no object layer named "<name>".`. Every object table has these fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | integer | The id of the object. |
| `name` | string | The name of the object. |
| `type` | string | The class of the object. |
| `x`, `y` | number | The position of the object in map coordinates. A tile object is anchored at the point that the object alignment of its tileset names, bottom-left by default. Other objects hang from their top-left corner. |
| `width`, `height` | number | The size of the object. |
| `rotation` | number | The rotation in radians. |
| `visible` | boolean | The visibility. |
| `opacity` | number | The opacity from 0 to 1. |
| `shape` | string | `'rectangle'`, `'ellipse'`, `'capsule'`, `'point'`, `'polygon'`, `'polyline'`, `'text'` or `'tile'`. |
| `gid` | integer | The global tile id of a tile object, with its flip flags, or 0. |
| `template` | string | The template path inside the content folder, or an empty string. |
| `points` | table | The points of a polygon or a polyline, as `{x, y}` tables relative to the position of the object. It is empty for other shapes. |
| `properties`, `propertyTypes` | table | The custom properties, merged with those of the template, and their type names. |
| `text` | table | Text objects only. It has the fields `text`, `fontFamily`, `pixelSize`, `wrap`, `color`, a `Color`, `bold`, `italic`, `underline`, `strikeout`, `kerning`, `horizontalAlign` and `verticalAlign`. |

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
for _, object in ipairs(map:objects()) do
    if object.shape == 'polygon' then
        print(object.name, #object.points)
    elseif object.shape == 'text' then
        print(object.name, object.text.text)
    end
end
for _, object in ipairs(map:objects('gameplay')) do
    print(object.type, object.x, object.y)
end
```

### map:spawn(factories, layer)

Creates app entities from objects. The table `factories` maps an object class to a function. The factory of each object whose class has one is called with the object table, in the object layer `layer`, or in every object layer when `layer` is left out. The object table also carries `worldX` and `worldY`: the origin of the object in world coordinates, with the offsets of its layer and groups.

Objects without a factory are skipped, which leaves markers and collision shapes for other code. Hidden objects and hidden layers are spawned too. The method returns the list of the values that the factories returned, without the `nil` ones, in map order. The factories run after the map has listed the objects, so a factory may change the map.

A factory that is not a function raises `The factory for the Tiled class "<name>" is not a function.`. A name that is not an object layer raises `The map has no object layer named "<name>".`.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local warrior = assets.texture('sprites/knight/idle.png')

local entities = map:spawn({
    player_start = function(object)
        return {kind = 'player', sprite = graphics2d.newSprite(warrior, {x = object.worldX, y = object.worldY, source = {0, 0, 192, 192}})}
    end,
    campfire = function(object)
        return {kind = 'campfire', x = object.worldX, y = object.worldY, radius = object.properties.radius or 160}
    end,
})
local regions = map:spawn({wilds = function(object) return {object.worldX, object.worldY, object.width, object.height} end}, 'gameplay')
print(#entities, #regions)
```

## Collision, ray casts and outlines

### map:buildCollision(world)

Creates static bodies in the [`haylen.physics2d`](physics2d.md) world `world` for the collision of the map, and returns them in map order. It makes one body for each tile layer whose `collision` property is not `false`, even when the layer has no collision shapes yet, so the tiles that `map:setTile` places later collide on it. It makes one body for each object layer with collision objects. The visibility of a layer does not matter. The [guide](../2d/tiled.md#collision) explains how the collision of a map works.

- **Tile layers** add the collision shapes of their tiles, placed and flipped with each tile. A tileset that draws its tiles at the size of the grid scales their shapes with them. A tile layer whose `collision` property is `false` adds nothing and gets no body.
- **Merging**: On orthogonal maps, the solid closed shapes of touching tiles merge into chain loops around each solid region, solid from the outside, with loops around the holes inside it. A region whose outline has only three corners, such as a lone triangle, stays a polygon. Sensors, polylines and shapes whose `oneWay` property is `true` stay separate shapes of the body. Isometric, staggered, hexagonal and oblique maps keep one shape for each tile shape.
- **Object layers** whose class is `collision` add every object. Other object layers add only the objects whose class is `collision`. Points and text objects add nothing, polylines become segments, and ellipses and capsules become polygons. Tile objects cover their image where it draws, placed by the object alignment of their tileset.
- **Sensors and platforms**: Objects and tile shapes whose `sensor` property is `true` become sensors. Those whose `oneWay` property is `true` become [one-way platforms](physics2d.md#one-way-platforms) that only block bodies from above, on tile layers and object layers alike.
- **Filters**: The layer properties `category` and `mask` set the collision filter of the shapes of a layer, as integers of at least 0. The category is 1 by default and the mask is every bit. Other values raise `The collision property "<name>" of the Tiled layer "<layer>" needs an integer of at least 0.`.

The method `map:setTile` traces the regions around the cell it changes again, on every body that `buildCollision` made for that layer, in every world the map built collision in. It forgets the bodies whose world is gone. Chain loops have no inside. So point queries such as `world:queryPoint` find nothing inside a solid region of a tile layer, ray casts hit its outline only from outside, and shape casts hit its outline from both sides.

```lua
local assets = require('haylen.assets')
local physics2d = require('haylen.physics2d')
local scene = require('haylen.scene')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local world = physics2d.newWorld({gravity = {0, 0}})
local walls = map:buildCollision(world)
print(#walls .. ' collision bodies.')

-- Digging a cell out of the ground layer traces the collision around it again.
map:setTile('ground', 4, 7, 0)

scene.push({
    fixedUpdate = function(self, step)
        world:step(step)
    end,
})
```

### map:raycastTiles(layer, x1, y1, x2, y2, solid)

Casts a ray from `x1, y1` to `x2, y2`, in the world coordinates of the map, over the cells of the tile layer named `layer`, without a physics world. It returns the hit on the first solid cell, or `nil`. The hit is a table like those of [`haylen.math`](math.md#ray-casts), with the extra fields `column`, `row` and `gid`. Its normal points out of the side of the cell that the ray entered.

Every tile is solid, unless the optional function `solid` receives the gid of each tile along the ray and returns `false` for the tiles the ray passes through. The ray only travels over the cells the layer holds, so a far end costs no more than a walk across the layer. The function `solid` runs once the ray has gathered the tiles it crosses, so it may change the map. Positions include the offsets of the layer and its groups.

Orthogonal, isometric and oblique maps take tile casts. Other orientations raise `Tile ray casts need an orthogonal, isometric or oblique map.`. An unknown layer raises `The map has no tile layer named "<name>".`.

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local water = 1
local hit = map:raycastTiles('ground', 100, 100, 900, 100, function(gid)
    return tiled.tileId(gid) ~= water
end)
if hit then
    print(hit.column, hit.row, hit.gid, hit.x, hit.normalX)
end
```

### map:raycastObjects(layer, x1, y1, x2, y2)

Casts a ray from `x1, y1` to `x2, y2` against the objects of the object layer named `layer`, or of every object layer when `layer` is `nil`. It returns the closest hit, or `nil`. The hit is a table like those of [`haylen.math`](math.md#ray-casts), with the extra fields `id`, `name` and `type` of the object. Rectangles, ellipses, capsules, polygons and tile objects, where their image draws, are solid. Polylines are hit from both sides. Points and text are never hit. An unknown layer raises `The map has no object layer named "<name>".`.

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local hit = map:raycastObjects(nil, 0, 200, 2000, 200)
if hit then
    print('The arrow hits ' .. hit.name .. ' of class ' .. hit.type .. ' at ' .. hit.x .. '.')
end
```

### map:objectOutlines(layer)

Returns the closed world outlines of the objects of the object layer named `layer`, or of every object layer when `layer` is left out, in map order, as a list of lists of `Vec2`. Rectangles give their corners, tile objects the corners of their image where it draws, and polygons their points. Ellipses and capsules give outlines of many sides. Points, text and polylines give nothing. The outlines are ready to become obstacles of a navigation mesh of [`haylen.navigation2d`](navigation2d.md).

A name that is not an object layer raises `The map has no object layer named "<name>".`. A tile object whose tile no tileset holds raises `A tile object uses the tile "<id>", which no tileset of the map holds.`, here and in `map:draw`, `map:buildCollision` and `map:raycastObjects`.

```lua
local assets = require('haylen.assets')
local navigation2d = require('haylen.navigation2d')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local area = map.pixelBounds
local right, bottom = area.x + area.width, area.y + area.height
local mesh = navigation2d.newNavMesh({{area.x, area.y}, {right, area.y}, {right, bottom}, {area.x, bottom}})
for _, outline in ipairs(map:objectOutlines('collision')) do
    mesh:addObstacle(outline)
end
local path = mesh:findPath(area.x + 64, area.y + 64, right - 64, bottom - 64, 12)
print(path and #path)
```

## Building maps at run time

A map made with `tiled.newMap`, or loaded from a file, changes through these methods. So a generator or a level editor builds a map in code and saves it as Tiled JSON, which the loader and the Tiled editor read back. Every method that changes cells drops the baked batches of its layer, which bake again on the next draw. It also keeps the collision that `map:buildCollision` built in step. The [guide](../2d/tiled.md#building-maps-in-code) explains when to build a map and when a `TileLayer` suits better.

### map:addTileset(options)

Adds a tileset after the tilesets of the map, and returns its first global tile id, the gid of its tile 0. The tiles are cut from the image in rows, from the top left, and the image gives the number of columns and tiles. The table `options` is required, and an unknown key raises `Unknown option "<key>".`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `name` | string | `''` | The name of the tileset. |
| `image` | string | none | The package path of the image, loaded through the asset cache like `assets.texture`, with `filter`, `wrap` and `mipmaps`. |
| `texture` | Texture | none | A texture to use instead of `image`. It keeps the path it was loaded from when the map is saved. |
| `filter`, `wrap`, `mipmaps` | string, string, boolean | `'nearest'`, `'clamp'`, `false` | The texture options of `image`. |
| `tileWidth`, `tileHeight` | number | `0` | The size of a tile in pixels, which must be positive. |
| `margin`, `spacing` | integer | `0` | The pixels around the tiles and between them. |
| `tileCount` | integer | from the image | The number of tiles, when the tileset uses fewer than the image holds. |
| `offsetX`, `offsetY` | number | `0` | The pixels that every tile image moves when it draws. |
| `renderGridSize` | boolean | `false` | Draws every tile at the size of the map grid instead of the size of its image, so tiles painted in high definition fill the cells of a smaller grid. Their offsets and collision shapes scale with them. |
| `preserveAspect` | boolean | `false` | Keeps the aspect ratio of tiles drawn at the size of the grid, which then fit inside their cells. |
| `wangSets` | table | none | The terrain sets for `map:paintTerrain`, a list of `{name, type, colors, tiles}`. |

Each Wang set of `wangSets` has these keys:

- The key `name` is the name of the set.
- The key `type` is `'corner'`, `'edge'` or `'mixed'`, and `'corner'` by default.
- The key `colors` lists the names of the colors, counted from 1.
- The key `tiles` lists `{tile, wangId}` tables, with the local tile id and the eight colors of its edges and corners, clockwise from the top edge, as Tiled stores them.

These errors can happen:

- A tile size that is not positive raises `A tileset needs a positive tile width and height.`.
- A tileset without an image or a texture raises `A tileset needs the package path of its "image" or a "texture".`.
- An image too small to hold one tile, or a `tileCount` of 0, raises `A tileset needs a positive tile size and at least one tile.`.
- A Wang set of another type raises `A Wang set has the type "corner", "edge" or "mixed".`.
- A Wang tile without eight colors raises `A tile of a Wang set needs "wangId", the eight colors of its edges and corners from the top clockwise.`.

```lua
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(tiled.newMap({width = 32, height = 32, tileWidth = 16, tileHeight = 16}))
local first = map:addTileset({name = 'cave', image = 'maps/images/cave.png', tileWidth = 16, tileHeight = 16, filter = 'nearest', wangSets = {
    {name = 'rock', type = 'corner', colors = {'rock'}, tiles = {{tile = 0, wangId = {0, 1, 0, 1, 0, 1, 0, 1}}}},
}})
print(first) -- 1
```

### map:addLayer(options)

Adds a layer on top of the others. A tile layer of a finite map takes the size of the map, with every cell empty. A tile layer of an infinite map starts without chunks. The table `options` is required, and an unknown key raises `Unknown option "<key>".`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `name` | string | none | The name of the layer, which must be new. |
| `kind` | string | `'tile'` | `'tile'` or `'object'`. |
| `visible` | boolean | `true` | Whether the layer draws. |
| `opacity` | number | `1` | The opacity from 0 to 1. |
| `offsetX`, `offsetY` | number | `0` | The offset of the layer in pixels. |
| `parallaxX`, `parallaxY` | number | `1` | The parallax factors. |
| `tint` | Color | white | The tint color. |
| `blend` | string | `'alpha'` | The blend mode, one of the names that the `blend` field of [`map:layers`](#maplayers) lists. The blends `'premultiplied'` and `'opaque'` draw too, but `map:toJson` refuses them. |

A name that is empty or taken raises `A layer of a map needs a name of its own, and "<name>" is empty or taken.`. Another kind raises `A layer built in code has the kind "tile" or "object".`.

```lua
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(tiled.newMap({width = 32, height = 32, tileWidth = 16, tileHeight = 16}))
map:addLayer({name = 'ground'})
map:addLayer({name = 'spawns', kind = 'object'})
map:addLayer({name = 'fog', opacity = 0.5, parallaxX = 1.2, parallaxY = 1.2, blend = 'screen'})
print(#map:layers()) -- 3
```

### map:removeLayer(name)

Removes a layer of the top level, with what it baked and the collision built from it. An unknown name raises `The map has no layer named "<name>" at its top level.`.

```lua
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(tiled.newMap({width = 32, height = 32, tileWidth = 16, tileHeight = 16}))
map:addLayer({name = 'scratch'})
map:removeLayer('scratch')
print(#map:layers()) -- 0
```

### map:fill(layer, grid, firstGid, column, row)

Sets the cells of a rectangle of a tile layer from a [`CellGrid`](spatial2d.md) of `haylen.spatial2d`. The top-left cell of the grid goes to `column`, `row`, which are 0 by default. Each value plus `firstGid`, which is 1 by default, becomes the global tile id of its cell. Negative values empty their cells. So a generator writes local tile ids into a grid and fills the layer in one call. On an infinite map, the cells must lie in its chunks. A gid that no tileset holds raises `The tile "<gid>" belongs to no tileset of the map.`.

```lua
local spatial2d = require('haylen.spatial2d')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(tiled.newMap({width = 32, height = 32, tileWidth = 16, tileHeight = 16}))
local first = map:addTileset({name = 'cave', image = 'maps/images/cave.png', tileWidth = 16, tileHeight = 16})
map:addLayer({name = 'ground'})

local grid = spatial2d.newCellGrid(32, 32, -1)
for column = 0, 31 do
    grid:set(column, 31, 0)
end
map:fill('ground', grid, first)
```

### map:setChunk(layer, column, row, grid, firstGid)

Puts a chunk of the size of `grid` at the cell `column`, `row` of a tile layer of an infinite map. It replaces the chunk that starts at that cell. The values of the grid are read like those of `map:fill`, with `firstGid` 1 by default. So a streamed world adds the chunks near the player. A finite map raises `Only the tile layers of an infinite map have chunks.`.

```lua
local spatial2d = require('haylen.spatial2d')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(tiled.newMap({infinite = true, tileWidth = 16, tileHeight = 16}))
local first = map:addTileset({name = 'cave', image = 'maps/images/cave.png', tileWidth = 16, tileHeight = 16})
map:addLayer({name = 'ground'})
local chunk = spatial2d.newCellGrid(16, 16, 0)
map:setChunk('ground', -16, 0, chunk, first)
print(#map:layer('ground').chunks) -- 1
```

### map:removeChunk(layer, column, row)

Removes the chunk that starts at the cell `column`, `row` of a tile layer of an infinite map. So a streamed world drops the chunks far from the player. A finite map raises `Only the tile layers of an infinite map have chunks.`.

```lua
local spatial2d = require('haylen.spatial2d')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(tiled.newMap({infinite = true, tileWidth = 16, tileHeight = 16}))
local first = map:addTileset({name = 'cave', image = 'maps/images/cave.png', tileWidth = 16, tileHeight = 16})
map:addLayer({name = 'ground'})
map:setChunk('ground', 0, 0, spatial2d.newCellGrid(16, 16, 0), first)
map:removeChunk('ground', 0, 0)
print(#map:layer('ground').chunks) -- 0
```

### map:paintTerrain(layer, wangSet, column, row, color)

Paints the color `color` of the Wang set named `wangSet` on a cell. The colors count from 1, and 0 erases. The cell takes the color on all its corners and edges. Its eight neighbors take it on the corners and edges they share with the cell. Each of the nine cells then takes the tile of the set that matches its new colors best. A `'corner'` set matches the corners, an `'edge'` set the edges, and a `'mixed'` set both. A cell left with no color becomes empty, and neighbors outside the layer are left out.

An unknown set raises `No tileset of the map has the Wang set "<name>".`. A color outside the set raises `The Wang set "<name>" has the colors 1 to <count>, and 0 erases.`.

```lua
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(tiled.newMap({width = 32, height = 32, tileWidth = 16, tileHeight = 16}))
map:addTileset({name = 'cave', image = 'maps/images/cave.png', tileWidth = 16, tileHeight = 16, wangSets = {
    {name = 'rock', type = 'corner', colors = {'rock'}, tiles = {{tile = 0, wangId = {0, 1, 0, 1, 0, 1, 0, 1}}}},
}})
map:addLayer({name = 'ground'})
map:paintTerrain('ground', 'rock', 10, 20, 1)
```

### map:toJson(path)

Returns the map as a table of Tiled JSON, for a file at the package path `path`, which defaults to the path the map was loaded from. Tilesets are embedded, templates are resolved into their objects, and every image and file property is written relative to the folder of `path`. The module `json` of Varn turns the table into text, which [`haylen.storage`](storage.md) saves. A layer whose blend has no mode in Tiled, such as `'premultiplied'` or `'opaque'`, raises `A Tiled layer blends with "normal", "add", "multiply", "screen", "overlay", "darken", "lighten", "color-dodge", "color-burn", "hard-light", "soft-light", "difference" or "exclusion", not "<blend>".`.

```lua
local json = require('json')
local spatial2d = require('haylen.spatial2d')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(tiled.newMap({width = 32, height = 32, tileWidth = 16, tileHeight = 16}))
local first = map:addTileset({name = 'cave', image = 'maps/images/cave.png', tileWidth = 16, tileHeight = 16})
map:addLayer({name = 'ground'})
map:fill('ground', spatial2d.newCellGrid(32, 1, 0), first, 0, 31)
print(json.encode(map:toJson('maps/cave.tmj')))
```

## Automapping

Automapping applies the rules of Tiled to a map at run time, the way the editor does when it automaps. Load the rules as an [`AutoMapper`](#automapper), an asset of the type `tiledRules`. The [guide](../2d/tiled.md#automapping) explains rules files, rule maps, their layers and their special tiles.

### map:automap(rules, options)

Applies the rules to the map and returns how many times they matched. The layers they changed bake again on their next draw, and trace their collision again. The table `options` is optional. Its only key, `seed`, makes the random outputs and probabilities the same every time. Without it, they differ from call to call. An unknown key raises `Unknown option "<key>".`.

These errors can happen, some when the rules load and some when they apply:

- A rule map without input or output layers raises `The rule map "<path>" needs an input layer and an output layer.`.
- A rule map with the legacy `regions` layers raises `The rule map "<path>" uses the legacy "regions" layers. Remove them, so its rules follow the tiles of their input and output layers.`.
- An unknown `MatchType` raises `The automapping tile type "<name>" is unknown. It is "Empty", "Ignore", "NonEmpty", "Other" or "Negate".`.
- A rules file that includes itself raises `The automapping rules file "<path>" includes itself.`.
- A rule map that outputs a tile of no tileset raises `A rule map outputs the tile "<id>", which no tileset of the rule map holds.`.

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(tiled.newMap({width = 32, height = 24, tileWidth = 16, tileHeight = 16}))
map:addTileset({name = 'cave', image = 'tiles/cave.png', tileWidth = 16, tileHeight = 16})
map:addLayer({name = 'ground'})
for column = 0, 31 do
    map:setTile('ground', column, 23, 1)
end
print(map:automap(assets.load('maps/rules/cave-rules.tmj', 'tiledRules'), {seed = 1}))
```

## Errors

Loading a map raises these errors:

| Message | Cause |
| --- | --- |
| `The orientation of a Tiled map must be "orthogonal", "isometric", "staggered", "hexagonal" or "oblique", not "<name>".` | The map has another orientation. |
| `The render order of a Tiled map must be "right-down", "right-up", "left-down" or "left-up", not "<name>".` | The map has another render order. |
| `The type of a Tiled layer must be "tilelayer", "objectgroup", "imagelayer" or "group", not "<name>".` | A layer has another type. |
| `The Tiled map "<path>" has a negative size.` | The width or the height of the map is negative. |
| `The Tiled map "<path>" needs a positive tile size.` | The tile size of the map is not positive. |
| `The skew of the oblique map "<path>" folds its grid onto a line.` | The skew of an oblique map leaves the grid without area. |
| `The Tiled tile layer "<name>" has a negative size.` | A tile layer has a negative width or height. |
| `The Tiled tile layer "<name>" has more cells than a layer can hold.` | A tile layer is too large. |
| `The compression of a tile layer must be "zlib", "gzip" or "zstd", not "<name>".` | The tile data uses another compression. |
| `Tile layer data is not valid base64.` | The base64 text of a tile layer is broken. |
| `Tile layer data could not be decompressed.` | The compressed tile data is broken. |
| `Tile layer data does not match the layer size.` | The tile data holds another number of cells than the layer. |
| `The Tiled color "<text>" is not a "#RRGGBB" or "#AARRGGBB" color.` | A color of the map is written another way. |
| `The Tiled layer "<name>" uses the blend mode "<mode>", which Tiled does not offer. Layers can use "normal", "add", "multiply", "screen", "overlay", "darken", "lighten", "color-dodge", "color-burn", "hard-light", "soft-light", "difference" or "exclusion".` | A layer has a blend mode that Tiled does not offer. |
| `The Tiled group layer "<name>" uses the blend mode "<mode>", which Tiled does not apply to the layers inside it. Set the blend mode on those layers.` | A group layer has a blend mode other than `normal`. |
| `A tile template uses the tileset "<path>", which the map does not list.` | A tile template names a tileset that the map lacks. |
| `The "project" of a Tiled map is the path of its ".tiled-project" file.` | The option `project` is not a path. |
| `The class "<name>" of the Tiled project holds itself.` | A class of the project contains itself. |
| `A class of the Tiled project has no name.` | A class of the project has no name. |
| `A member of the class "<name>" of the Tiled project has no name.` | A member of a class of the project has no name. |
| `Unknown option "<key>".` | The options of a map or a world have a key that they do not take. |
| `The texture filter must be "nearest" or "linear", not <value>.` | The option `filter` has another value. |
| `The texture wrap must be "clamp", "repeat" or "mirror", not <value>.` | The option `wrap` has another value. |
| `The texture option "mipmaps" must be true or false, not <value>.` | The option `mipmaps` is not a boolean. |

The functions and methods raise these errors:

| Message | Cause |
| --- | --- |
| `Unknown option "<key>".` | An options table has a key that the call does not take. |
| `The map has no layer named "<name>".` | The methods `map:layer`, `map:drawLayer` or `map:setLayerVisible` got an unknown layer. |
| `The map has no tile layer named "<name>".` | The methods `map:tile`, `map:setTile` or `map:raycastTiles` got a name that is not a tile layer. |
| `The map has no object layer named "<name>".` | The methods `map:objects`, `map:spawn`, `map:raycastObjects` or `map:objectOutlines` got a name that is not an object layer. |
| `The map has no layer named "<name>" at its top level.` | The method `map:removeLayer` got a name that is not a top-level layer. |
| `Repeated image layers need the visible area of the view.` | A repeating image layer was drawn without a camera. |
| `A group that blends with what lies below draws in a canvas without ambient light.` | A layer whose blend mode mixes with the colors below it was drawn in a lit canvas. |
| `The tile "<gid>" belongs to no tileset of the map.` | The method `map:setTile` or `map:fill` got a gid of no tileset. |
| `The cell is outside the tile layer.` | The method `map:setTile` got a cell outside a finite layer. |
| `The cell is outside every chunk of the infinite tile layer.` | The method `map:setTile` or `map:fill` got a cell outside the chunks of an infinite layer. |
| `The factory for the Tiled class "<name>" is not a function.` | The table of `map:spawn` holds something other than a function. |
| `The collision property "<name>" of the Tiled layer "<layer>" needs an integer of at least 0.` | The property `category` or `mask` of a layer is not an integer of at least 0. |
| `Tile ray casts need an orthogonal, isometric or oblique map.` | The method `map:raycastTiles` ran on a staggered or hexagonal map. |
| `A tile object uses the tile "<id>", which no tileset of the map holds.` | A tile object names a tile of no tileset. |
| `A tile layer uses the tile "<id>", which no tileset of the map holds.` | The method `map:draw` or `map:drawLayer` met a cell whose gid no tileset holds. |
| `A tileset needs a positive tile size and at least one tile.` | The method `map:addTileset` got an image too small to hold one tile, or a `tileCount` of 0. |
| `A map needs a positive tile size, and a finite map a positive width and height in cells.` | The function `tiled.newMap` got a bad size. |
| `A map has the orientation "orthogonal", "isometric", "staggered", "hexagonal" or "oblique" and the render order "right-down", "right-up", "left-down" or "left-up".` | The function `tiled.newMap` got another orientation or render order. |
| `A map staggers along the axis "x" or "y" from the "odd" or "even" cells.` | The function `tiled.newMap` got another stagger axis or index. |
| `A tileset needs a positive tile width and height.` | The method `map:addTileset` got a bad tile size. |
| `A tileset needs the package path of its "image" or a "texture".` | The method `map:addTileset` got neither an image nor a texture. |
| `A Wang set has the type "corner", "edge" or "mixed".` | A Wang set of `map:addTileset` has another type. |
| `A tile of a Wang set needs "wangId", the eight colors of its edges and corners from the top clockwise.` | A Wang tile of `map:addTileset` lacks its eight colors. |
| `A layer of a map needs a name of its own, and "<name>" is empty or taken.` | The method `map:addLayer` got an empty or taken name. |
| `A layer built in code has the kind "tile" or "object".` | The method `map:addLayer` got another kind. |
| `Only the tile layers of an infinite map have chunks.` | The method `map:setChunk` or `map:removeChunk` ran on a finite map. |
| `No tileset of the map has the Wang set "<name>".` | The method `map:paintTerrain` got an unknown Wang set. |
| `The Wang set "<name>" has the colors 1 to <count>, and 0 erases.` | The method `map:paintTerrain` got a color outside the set. |
| `A Tiled layer blends with "normal", "add", "multiply", "screen", "overlay", "darken", "lighten", "color-dodge", "color-burn", "hard-light", "soft-light", "difference" or "exclusion", not "<blend>".` | The method `map:toJson` met a layer whose blend has no mode in Tiled. |
| `The rule map "<path>" needs an input layer and an output layer.` | A rule map lacks input or output layers. |
| `The rule map "<path>" uses the legacy "regions" layers. Remove them, so its rules follow the tiles of their input and output layers.` | A rule map uses the legacy `regions` layers. |
| `The automapping tile type "<name>" is unknown. It is "Empty", "Ignore", "NonEmpty", "Other" or "Negate".` | A tile of a rule map has an unknown `MatchType`. |
| `The automapping rules file "<path>" includes itself.` | A rules file includes itself. |
| `A rule map outputs the tile "<id>", which no tileset of the rule map holds.` | A rule map outputs a gid of no tileset. |
