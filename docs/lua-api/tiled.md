# haylen.tiled

`haylen.tiled` turns maps made with the [Tiled](https://www.mapeditor.org) editor into playable levels. A loaded map draws its tile, object and image layers with culling, parallax, tints and blend modes, animates its tiles, answers questions about cells, layers, objects and tiles, spawns app entities from objects through factories, builds physics collision from tile and object shapes, casts rays against tiles and objects without a physics world and lists object outlines for navigation meshes. Use it whenever a level is designed in Tiled.

```lua
local tiled = require('haylen.tiled')
```

## Loading maps and worlds

Maps and worlds are assets of [haylen.assets](assets.md), with the asset types `tiled` for `.tmj` files and `tiledWorld` for `.world` files, so `assets.load` finds the type from the extension. Save maps in the Tiled JSON format with the `.tmj` extension, and keep external tilesets and templates in JSON too (`.tsj` and `.tj`). The XML formats (`.tmx`, `.tsx` and `.tx`) are not read. Every path inside a map is resolved relative to the file that contains it, so the map, its tilesets, templates and images can live in any folder of the package `content/` directory.

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local data = assets.load('maps/island.tmj')
local map = tiled.newMapRenderer(data)
```

`assets.load(path, nil, options)` and `assets.loadAsync(path, nil, options)` accept the texture options `filter` (`'nearest'` or `'linear'`, default `'nearest'`) and `wrap` (`'clamp'`, `'repeat'` or `'mirror'`, default `'clamp'`) for every image of the map. Map images share the texture cache with `assets.texture`, except images with a transparent color, which get a private color-keyed texture. Loading with `loadAsync` parses the map and decodes its images on worker threads:

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

The loaded asset is a `TiledMap` value that holds the parsed data. Its only member is the read-only `path` property, the map path inside the content folder. `tiled.newMapRenderer` makes a playable `MapRenderer` from it.

A Tiled world file with the `.world` extension loads as a plain list with one table per map, each with `path` (the map path inside the content folder), `x`, `y`, `width` and `height` (the map area in world pixels). Maps listed in `maps` keep their order, followed by the maps that `patterns` match. A pattern's `regexp` is matched against the files in the world's folder, its first two capture groups are multiplied by `multiplierX` and `multiplierY` and moved by `offsetX` and `offsetY`, and `mapWidth` and `mapHeight` give the size. World loading accepts no options, and any key raises `Unknown key 'name' in Tiled world options.`

```lua
local assets = require('haylen.assets')

local world = assets.load('maps/world/overworld.world')
for _, entry in ipairs(world) do
    print(entry.path, entry.x, entry.y, entry.width, entry.height)
end
```

## Supported Tiled features

The runtime targets the current Tiled JSON format as written by Tiled 1.12.

- Orientations: orthogonal, isometric, staggered and hexagonal (with `staggeraxis`, `staggerindex` and `hexsidelength`), and oblique maps with `skewx` and `skewy`. A skew that folds the grid onto a line raises `The skew of the oblique map path folds its grid onto a line.`
- Render orders: `right-down`, `right-up`, `left-down` and `left-up`.
- Tile layer data as JSON arrays or base64, uncompressed or compressed with zlib, gzip or zstd.
- Infinite maps, whose tile layers are stored as chunks. Cells may have negative coordinates.
- Layer types: tile layers, object layers, image layers and group layers, with visibility, opacity, offsets, tint colors, parallax factors, the map parallax origin, classes and custom properties. Groups pass their offset, parallax, tint, opacity and visibility on to their children.
- Layer blend modes: `normal`, `add`, `multiply` and `screen`. Every other Tiled mode, such as `overlay`, `darken` or `lighten`, is rejected when the map loads with `The Tiled layer 'name' uses the blend mode 'mode', which Haylen cannot draw. Layers can use normal, add, multiply or screen.` Tiled draws every layer of a group with that layer's own mode and never blends a group as a whole, so a group layer with a mode other than `normal` is rejected with `The Tiled group layer 'name' uses the blend mode 'mode', which Tiled does not apply to the layers inside it. Set the blend mode on those layers.`
- Tilesets embedded in the map or stored in external files, image tilesets with margin, spacing and tile offsets, image collection tilesets whose tiles can use a sub-rectangle of their image and keep the ids of removed tiles unused, transparent colors, object alignment, tile render size `grid` with the fill mode `preserve-aspect-fit`, tile classes, properties, animations, collision shapes and Wang sets.
- Tile flip flags: horizontal, vertical and diagonal, plus the 120-degree rotation flag of hexagonal maps.
- Objects: rectangles, ellipses, capsules, points, polygons, polylines, text and tile objects, with rotation, visibility, opacity, classes and custom properties.
- Object templates, whose fields and properties are defaults that each instance overrides one by one. Tile templates map their tile to the map's own copy of the template's tileset, and a template whose tileset the map does not list raises `A tile template uses a tileset the map does not list: path`.
- Custom property types `string`, `int`, `float`, `bool`, `color`, `file`, `object`, `class` and `list` (Tiled 1.12), including lists nested in lists.
- Worlds with listed maps and patterns.
- Object factories through `map:spawn`.

Every layer needs the `id` that Tiled writes. Other load errors: `Unknown Tiled map orientation: name`, `Unknown Tiled render order: name`, `Unknown Tiled layer type: name`, `The Tiled map path has a negative size.`, `The Tiled map path needs a positive tile size.`, `The Tiled tile layer 'name' has a negative size.`, `The Tiled tile layer 'name' has more cells than a layer can hold.`, `Unknown tile layer compression: name`, `Tile layer data is not valid base64.`, `Tile layer data could not be decompressed.`, `Tile layer data does not match the layer size.` and `Invalid Tiled color: text`.

## Drawing rules

- Tile layers are baked into static batches in regions of 32 by 32 cells the first time they draw. When a camera is given, only the regions inside the area the active canvas shows are drawn, which is the visible area around the camera on a world canvas and the target size around it on a render target canvas. `map:setTile` bakes only the region of its cell again, on the next draw. Animated tiles are drawn every frame with the frame that matches the map time.
- Object layers draw their visible tile objects and text objects. Other shapes are data for the app and for collision. Objects sort by their y position unless the layer draw order is `index`. Tile objects are placed by the tileset object alignment, which defaults to bottom-left, or bottom-center on isometric maps. Text objects use the engine's default font with their size, color, wrapping, alignment, rotation and opacity. Their font family, bold and italic settings are not applied.
- Image layers draw their image at the layer offset. Layers that repeat on an axis cover the camera's visible area and need a camera, otherwise drawing raises `Repeated image layers need the visible area of the view.`
- A layer with a parallax factor other than 1 shifts by the distance between the camera position and the map parallax origin, times one minus the factor. On a map drawn with an offset, the parallax origin moves with the map.
- Every layer draws with the draw order given to `map:draw` or `map:drawLayer`, with its blend replaced by the layer's blend mode. Later map layers cover earlier ones because they are drawn later.
- With the `ysort` option, tile layers bake and draw row by row, each row being the cells that stand on the same y, which is a row of an orthogonal map and a diagonal of an isometric one, and object layers draw object by object. Every row and object takes as its depth the y it stands on, the bottom of its cells or of its object, and sorts by that y in canvases that sort by `'y'` too, so entities drawn in the same layer of a canvas that sorts by y or by depth pass behind and in front of trees, walls and other map objects.
- The `x` and `y` draw options place the map origin in the world, so several maps share one camera. Culling, parallax and repeated images follow the placed map.

## Functions

### tiled.newMapRenderer(asset)

Creates a `MapRenderer` from a loaded `TiledMap`. The playable map owns its own copy of the data, so changes such as `map:setTile` never touch the cached asset, and several maps made from one asset are independent.

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

## MapRenderer

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `path` | string | read | Map path inside the content folder. |
| `width`, `height` | integer | read | Map size in cells. |
| `tileWidth`, `tileHeight` | number | read | Grid cell size in pixels. |
| `pixelBounds` | Rect | read | World area the grid covers. It starts left of or above 0 on oblique maps with a negative skew. |
| `orientation` | string | read | `'orthogonal'`, `'isometric'`, `'staggered'`, `'hexagonal'` or `'oblique'`. |
| `skewX`, `skewY` | number | read | Skew of oblique maps in pixels. |
| `backgroundColor` | Color or nil | read | Map background color, or `nil` when the map has none. |
| `infinite` | boolean | read | True for infinite maps, whose tile layers are stored in chunks. |
| `renderOrder` | string | read | `'right-down'`, `'right-up'`, `'left-down'` or `'left-up'`. |
| `hexSideLength` | integer | read | Length of the flat side of hexagonal tiles, 0 on other maps. |
| `staggerX` | boolean | read | True when staggered and hexagonal maps shift every other column, false when they shift every other row. |
| `staggerEven` | boolean | read | True when the even columns or rows are shifted, false when the odd ones are. |
| `parallaxOrigin` | Vec2 | read | Point of the map that parallax layers are measured from. |
| `type` | string | read | Map class. |
| `properties` | table | read | Custom properties of the map. |
| `propertyTypes` | table | read | Custom property type names of the map properties. |

Custom properties become a table from name to value. Strings, numbers and booleans keep their type. Colors become `Color` values, or `nil` when the color is unset. Files become paths inside the content folder. Object references become object ids. Class values become nested tables of their members, whose colors stay `#AARRGGBB` text because Tiled stores class members without their types. Lists become sequences of their items, converted by the same rules, so a list of colors holds `Color` values and a list of lists holds sequences.

Every table that has `properties` also has `propertyTypes`, a table from property name to the name of its custom property type, such as the class of a class property or the enum of an enum property. Properties with a plain type are absent from it.

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
print(map.infinite, map.renderOrder, map.hexSideLength, map.staggerX, map.staggerEven, map.parallaxOrigin.x)
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

### map:draw(camera, options)

Draws every visible layer in map order into the active canvas. `camera` is optional. When it is given, parallax layers follow its position, and culling and repeated images use the area the active canvas shows. Without it nothing is culled and parallax layers shift as if the camera stood at the world origin. `options` is optional and takes the [draw order](graphics2d.md#draw-order) keys `layer`, `depth`, `sortOffset`, `visibility` and `blend` from haylen.graphics2d, where `blend` is replaced by each layer's blend mode, `x` and `y`, the world position of the map origin, which default to 0, and `ysort`, which draws tile and object layers sorted by the y they stand on, as [Drawing rules](#drawing-rules) describes. Other keys raise `Unknown option 'name'.`

### map:drawLayer(name, camera, options)

Draws one layer, including the children of a group layer, with the offset, parallax, tint and visibility it inherits from the groups above it. `camera` and `options` work as in `map:draw`. Drawing layers one by one lets the app place its own sprites between them. An unknown name raises `Unknown layer: name`.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local camera = graphics2d.newCamera()
local hero = graphics2d.newSprite(assets.texture('tiny_swords/units/blue/warrior/warrior_idle.png'), {source = {0, 0, 192, 192}, x = 800, y = 600, layer = 1})

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
local hero = graphics2d.newSprite(assets.texture('tiny_swords/units/blue/warrior/warrior_idle.png'), {source = {0, 0, 192, 192}, x = 800, y = 600, pivotY = 0.7, sortOffset = 50, layer = 1})

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

Advances the time of animated tiles by `dt` seconds.

### map:tile(layer, column, row)

Returns the global tile id at a cell of the tile layer `layer`, with its flip flags, or 0 for an empty cell or a cell outside the layer. Columns and rows count from 0. A name that is not a tile layer raises `Unknown tile layer: name`.

### map:setTile(layer, column, row, gid)

Replaces the tile at a cell. `gid` is a global tile id, optionally with flip flags added, and 0 empties the cell. A gid that no tileset holds raises an error such as `No tileset holds the tile 5000`, and a cell outside the layer raises `The cell is outside the tile layer.` or, on infinite maps, `The cell is outside every chunk of the infinite tile layer.`

The flip flags are the module constants described in [Flip flags](#flip-flags). Combine them with `|`.

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

### map:setLayerVisible(name, visible)

Shows or hides a layer, which also hides the children of a group. An unknown name raises `Unknown layer: name`.

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
map:setLayerVisible('shadow', false)
print(map:layer('shadow').visible)
```

### map:cellToWorld(column, row)

Returns the world position `x, y` of a cell: its top-left corner, or the top corner of the diamond on isometric maps.

### map:worldToCell(x, y)

Returns the `column, row` of the cell under a world position. Staggered and hexagonal maps pick the cell whose center is nearest.

### map:objectToWorld(x, y)

Converts object coordinates from the map file to a world position and returns `x, y`. The two differ on isometric maps, whose object coordinates run along the diamond edges, and on skewed oblique maps.

```lua
local assets = require('haylen.assets')
local input = require('haylen.input')
local graphics2d = require('haylen.graphics2d')
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

        local start = map:objects('gameplay')[1]
        local spawnX, spawnY = map:objectToWorld(start.x, start.y)
        graphics2d.drawCircle(spawnX, spawnY, 6, '#FFFF4040', {layer = 1})
    end,
})
```

### map:layer(name)

Returns a table that describes the layer `name`, searching group layers depth first. The table is a snapshot, so changing it does not change the map. An unknown name raises `Unknown layer: name`.

### map:layers()

Returns the list of top-level layers as layer tables. Group layers hold their children in `layers`.

Every layer table has these fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | integer | Layer id. |
| `name` | string | Layer name. |
| `type` | string | Layer class. |
| `kind` | string | `'tile'`, `'object'`, `'image'` or `'group'`. |
| `visible` | boolean | Visibility, including changes made with `map:setLayerVisible`. |
| `opacity` | number | Opacity from 0 to 1. |
| `blend` | string | Blend mode: `'alpha'`, `'additive'`, `'multiply'` or `'screen'`. |
| `offsetX`, `offsetY` | number | Layer offset in pixels. |
| `parallaxX`, `parallaxY` | number | Parallax factors. |
| `tint` | Color | Tint color. |
| `properties`, `propertyTypes` | table | Custom properties and their type names. |
| `width`, `height` | integer | Tile layers only. Layer size in cells. |
| `chunks` | table | Tile layers only. The chunks of an infinite map, each with `x` and `y` (its first cell), `width`, `height` and `gids`, the global tile ids row by row with their flip flags. Empty on other maps. |
| `objects` | table | Object layers only. List of object tables. |
| `indexDrawOrder` | boolean | Object layers only. True when objects draw in list order, false when they sort by their y position. |
| `image`, `repeatX`, `repeatY` | string, boolean, boolean | Image layers only. Image path inside the content folder and repetition. |
| `imageSize` | Vec2 | Image layers only. Image size written in the map. |
| `transparentColor` | Color or nil | Image layers only. Color drawn as transparent, or absent. |
| `layers` | table | Group layers only. List of child layer tables. |

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
for _, layer in ipairs(map:layers()) do
    print(layer.name, layer.kind, layer.blend, layer.opacity)
end
local ground = map:layer('ground')
print(ground.width, ground.height, ground.properties.collision)
for _, chunk in ipairs(ground.chunks) do
    print(chunk.x, chunk.y, chunk.width, chunk.height, #chunk.gids)
end
```

### map:objects(layer)

Returns the objects of the object layer `layer`, or of every object layer in map order when `layer` is omitted. Coordinates are the raw values of the map file, without layer offsets. A name that is not an object layer raises `The map has no object layer named 'name'.`

Every object table has these fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | integer | Object id. |
| `name` | string | Object name. |
| `type` | string | Object class. |
| `x`, `y` | number | Object position in map coordinates. Tile objects are anchored at the point their tileset's object alignment names, bottom-left by default, and other objects hang from their top-left corner. |
| `width`, `height` | number | Object size. |
| `rotation` | number | Rotation in radians. |
| `visible` | boolean | Visibility. |
| `opacity` | number | Opacity from 0 to 1. |
| `shape` | string | `'rectangle'`, `'ellipse'`, `'capsule'`, `'point'`, `'polygon'`, `'polyline'`, `'text'` or `'tile'`. |
| `gid` | integer | Global tile id of tile objects, with flip flags, or 0. |
| `template` | string | Template path inside the content folder, or an empty string. |
| `points` | table | Polygon and polyline points as `{x, y}` tables relative to the object position. Empty for other shapes. |
| `properties`, `propertyTypes` | table | Custom properties, merged with the template's, and their type names. |
| `text` | table | Text objects only. Fields `text`, `fontFamily`, `pixelSize`, `wrap`, `color` (a Color), `bold`, `italic`, `horizontalAlign` and `verticalAlign`. |

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

Creates app entities from objects. `factories` is a table from object class to function. For every object whose class has a factory, in the object layer `layer` or in every object layer when `layer` is omitted, the factory is called with the object table, which also carries `worldX` and `worldY`: the object origin in world coordinates, including the offsets of its layer and groups. Objects without a factory are skipped, which leaves markers and collision shapes for other code. `spawn` returns the list of non-nil values the factories returned, in map order. Hidden objects and hidden layers are spawned too. The factories run after the map has listed the objects, so a factory may change the map. A factory that is not a function raises `The factory for the Tiled class 'name' is not a function.` and a name that is not an object layer raises `The map has no object layer named 'name'.`

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local warrior = assets.texture('tiny_swords/units/blue/warrior/warrior_idle.png')

local entities = map:spawn({
    player_start = function(object)
        return {kind = 'player', sprite = graphics2d.newSprite(warrior, {x = object.worldX, y = object.worldY, source = {0, 0, 192, 192}})}
    end,
    campfire = function(object)
        return {kind = 'campfire', x = object.worldX, y = object.worldY, radius = object.properties.radius or 160}
    end,
})
local regions = map:spawn({tree_region = function(object) return {object.worldX, object.worldY, object.width, object.height} end}, 'gameplay')
print(#entities, #regions)
```

### map:tileInfo(gid)

Describes a global tile id, or returns `nil` for 0 and for ids no tileset holds. The table has these fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | integer | Tile id inside its tileset. |
| `tileset` | string | Tileset name. |
| `flippedX`, `flippedY`, `flippedDiagonally` | boolean | Flip flags of the gid. |
| `source` | Rect | Tile rectangle inside its image. |
| `type` | string | Tile class, or an empty string. |
| `image` | string | Image path of a tile from an image collection, or an empty string. |
| `probability` | number | Chance of the tile in Tiled's terrain and random tools, 1 by default. |
| `properties`, `propertyTypes` | table | Custom properties of the tile and their type names. |
| `collision` | table | Collision shapes as object tables, relative to the tile. |
| `animation` | table | Animation frames as `{tileId = id, duration = seconds}` tables. Empty for still tiles. |

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

Returns the tilesets of the map in first gid order. Every tileset table has these fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `name` | string | Tileset name. |
| `type` | string | Tileset class. |
| `path` | string | Path of an external tileset file inside the content folder, or an empty string for a tileset embedded in the map. |
| `firstGid` | integer | Global id of the first tile. |
| `tileWidth`, `tileHeight` | number | Tile size in pixels. |
| `columns`, `tileCount` | integer | Columns of the tileset image and number of tiles. |
| `margin`, `spacing` | integer | Pixels around the tiles and between them in the image. |
| `tileOffset` | Vec2 | Offset applied when the tiles are drawn. |
| `objectAlignment` | string | Anchor of tile objects, such as `'unspecified'`, `'bottomleft'`, `'bottom'` or `'center'`. |
| `renderGridSize` | boolean | True when tiles are drawn at the map grid size instead of their image size. |
| `preserveAspect` | boolean | True when grid sized tiles keep their aspect ratio. |
| `image` | string | Image path inside the content folder, or an empty string for image collections. |
| `imageSize` | Vec2 | Image size written in the tileset. |
| `transparentColor` | Color or nil | Color drawn as transparent, or absent. |
| `texture` | Texture or nil | The loaded tileset image, absent for image collections. |
| `properties`, `propertyTypes` | table | Custom properties and their type names. |
| `wangSets` | table | Wang sets, described below. |

Each Wang set has `name`, `type` (its class), `kind` (`'corner'`, `'edge'` or `'mixed'`), `tile` (the tile that represents it, or -1), `properties`, `propertyTypes`, `colors` and `tiles`. Each color has `name`, `type` (its class), `color`, `tile`, `probability`, `properties` and `propertyTypes`, and each tile has `tileId` and an eight-number `wangId`.

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

### map:buildCollision(world)

Creates static bodies in the [haylen.physics2d](physics2d.md) world `world` for the collision shapes of the map and returns them. Each layer that has shapes becomes one body, and layer visibility does not matter.

- Tile layers add the collision shapes of their tiles, placed and flipped with each tile, unless the layer has a `collision` property set to false. On orthogonal maps, tiles whose collision is one rectangle covering the whole cell merge into one box per horizontal run of cells.
- Object layers whose class is `collision` add every object, and other object layers add the objects whose class is `collision`. Points and text objects add nothing, polylines become segments, ellipses and capsules become polygons, and tile objects cover their image where it draws, placed by the object alignment of their tileset.
- Objects and tile shapes with a `sensor` property set to true become sensors.
- The layer properties `category` and `mask` set the collision filter of the layer's shapes as integers of at least 0. The category defaults to 1 and the mask to every bit. Other values raise `The collision property 'name' of the Tiled layer 'layer' needs an integer of at least 0.`

```lua
local assets = require('haylen.assets')
local physics2d = require('haylen.physics2d')
local scene = require('haylen.scene')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local world = physics2d.newWorld({gravity = {0, 0}})
local walls = map:buildCollision(world)
print(#walls .. ' collision bodies')

scene.push({
    fixedUpdate = function(self, step)
        world:step(step)
    end,
})
```

### map:raycastTiles(layer, x1, y1, x2, y2, solid)

Casts a ray from `x1, y1` to `x2, y2` in map world coordinates over the cells of the tile layer named `layer`, without a physics world, and returns the hit on the first solid cell or `nil`. The hit is a table like those of [haylen.math](math.md#ray-casts) with the extra fields `column`, `row` and `gid`, and its normal points out of the side of the cell the ray entered. Every tile is solid unless the optional function `solid` receives the gid of each tile along the ray and returns false for the ones the ray passes through. The ray only travels over the cells the layer holds, so a far end costs no more than a walk across the layer, and `solid` runs once the ray has gathered the tiles it crosses, so it may change the map. Positions include the offsets of the layer and its groups. Orthogonal, isometric and oblique maps take tile casts, and other orientations raise `Tile ray casts need an orthogonal, isometric or oblique map.` An unknown layer raises `The map has no tile layer named 'name'.`

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

Casts a ray from `x1, y1` to `x2, y2` against the objects of the object layer named `layer`, or of every object layer when `layer` is `nil`, and returns the closest hit or `nil`. The hit is a table like those of [haylen.math](math.md#ray-casts) with the extra fields `id`, `name` and `type` of the object. Rectangles, ellipses, capsules, polygons and tile objects, where their image draws, are solid, polylines are hit from both sides, and points and text are never hit. An unknown layer raises `The map has no object layer named 'name'.`

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local hit = map:raycastObjects(nil, 0, 200, 2000, 200)
if hit then
    print('the arrow hits ' .. hit.name .. ' of class ' .. hit.type .. ' at ' .. hit.x)
end
```

### map:objectOutlines(layer)

Returns the closed world outlines of the objects of the object layer named `layer`, or of every object layer when `layer` is omitted, in map order, as a list of lists of `Vec2`. Rectangles give their corners, tile objects the corners of their image where it draws, polygons their points, and ellipses and capsules many-sided outlines, while points, text and polylines give nothing. The outlines are ready to become obstacles of a navigation mesh of [haylen.navigation2d](navigation2d.md). A name that is not an object layer raises `The map has no object layer named 'name'.`, and a tile object whose tile no tileset holds raises `A tile object uses a tile that no tileset holds: id`, here and in `map:draw`, `map:buildCollision` and `map:raycastObjects`.

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

## Placing the maps of a world

The `x` and `y` options of `map:draw` and `map:drawLayer` place a map origin in the world, so every map of a `.world` file draws at its world position in one canvas through one camera, with culling that skips the maps the camera does not see:

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

Cells and object positions of a placed map stay in map coordinates, so add the map position to what `map:cellToWorld` and `map:objectToWorld` return, and subtract it before `map:worldToCell`.

## Flip flags

The module has the constants of the flags that Tiled stores in the high bits of a global tile id, and a function that removes them.

| Constant | Value | Meaning |
| --- | --- | --- |
| `tiled.flipHorizontal` | `0x80000000` | The tile is mirrored horizontally. |
| `tiled.flipVertical` | `0x40000000` | The tile is mirrored vertically. |
| `tiled.flipDiagonal` | `0x20000000` | The tile is mirrored across its diagonal, which with the other flips rotates it by 90 degree steps. |
| `tiled.rotateHexagonal` | `0x10000000` | The tile of a hexagonal map is turned by 120 degrees. |
| `tiled.flagMask` | `0xF0000000` | Every flag bit. |

### tiled.tileId(gid)

Returns `gid` without its flag bits, which is the global id of the tile itself.

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local gid = map:tile('ground', 2, 2)
local flippedX = (gid & tiled.flipHorizontal) ~= 0
print(tiled.tileId(gid), flippedX, gid & tiled.flagMask)
map:setTile('ground', 2, 2, tiled.tileId(gid) | tiled.flipVertical)
```
