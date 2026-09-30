# Tiled maps

Haylen loads maps made with the [Tiled](https://www.mapeditor.org) editor and turns them into playable levels. A map draws its tile, object and image layers with culling, parallax, tints and blend modes, animates its tiles, answers questions about cells, layers, objects and tiles, spawns app entities from objects through factories and builds physics collision from tile and object shapes. This guide covers how to author maps for the engine, how the runtime uses them and the conventions of the Tiny Island map. The [`haylen.tiled` reference](lua-api/tiled.md) lists every function, field and error.

## Authoring maps

Save maps in the Tiled JSON format with the `.tmj` extension, and keep external tilesets in `.tsj` files and object templates in `.tj` files. The runtime targets the current JSON format as Tiled 1.12 writes it and does not read the XML formats (`.tmx`, `.tsx` and `.tx`) or add compatibility code for obsolete ones.

Every path inside a map is resolved relative to the file that contains it, so a map, its tilesets, its templates and its images can live in any folders under the package `content/` directory, as long as the relative paths Tiled writes stay inside it.

## Supported features

- **Orientations**: Orthogonal, isometric, staggered and hexagonal maps (with `staggeraxis`, `staggerindex` and `hexsidelength`), and oblique maps with the `skewx` and `skewy` of Tiled 1.12. Cells, objects, bounds and collision all follow the skew. A skew that folds the grid onto a line is rejected.
- **Render orders**: `right-down`, `right-up`, `left-down` and `left-up`.
- **Tile data**: JSON arrays or base64, uncompressed or compressed with zlib, gzip or zstd.
- **Infinite maps**: Tile layers stored as chunks, with cells at negative coordinates.
- **Layer types**: Tile layers, object layers, image layers (repeating on x, y or both) and nested group layers.
- **Layer attributes**: Visibility, opacity, offsets, tint colors, parallax factors with the map parallax origin, classes and custom properties. Groups pass their offset, parallax, tint, opacity and visibility on to their children, and tints and opacities multiply down the tree. Layer locking is an editor feature and has no effect at runtime.
- **Blend modes**: The modes `normal`, `add`, `multiply` and `screen`, drawn with the matching GPU blend. Every other mode Tiled offers, such as `overlay`, `darken`, `lighten` or `difference`, cannot be reproduced with fixed-function blending, so the map fails to load with an error that names the layer and the mode. Tiled draws the children of a group with their own modes and never blends a group as a whole, so a group layer with any mode other than `normal` fails to load with an error instead of being ignored.
- **Tilesets**: Embedded or external, image tilesets with margin, spacing and tile offsets, image collection tilesets whose tiles can use a sub-rectangle of their image and keep the ids of removed tiles unused, transparent colors, object alignment, the `grid` tile render size with the `preserve-aspect-fit` fill mode, tile classes, probabilities and properties.
- **Tile flips**: Horizontal, vertical and diagonal, plus the 120-degree rotation flag of hexagonal maps.
- **Animated tiles**: Tile animations play on tile layers and on tile objects, driven by `map:update(dt)`.
- **Wang sets**: Corner, edge and mixed sets with their colors and tiles are read and returned by `map:tilesets()`. They are editor data for terrain brushes, and the runtime does not repaint tiles with them.
- **Objects**: Rectangles, ellipses, capsules, points, polygons, polylines, text and tile objects, with rotation, visibility, per-object opacity, classes and custom properties.
- **Templates**: The `.tj` object templates, whose fields and properties are defaults that each instance overrides one by one. A tile template maps its tile to the map's own copy of the template's tileset.
- **Properties**: The `string`, `int`, `float`, `bool`, `color`, `file`, `object`, `class` and `list` values, including classes nested in classes and lists nested in lists. Colors become `Color` values, files become paths inside the `content/` folder, object references become object ids, class values become nested tables and lists become sequences of their items.
- **Worlds**: `.world` files with listed maps and filename patterns.

## Loading maps

Maps are assets of [`haylen.assets`](lua-api/assets.md). The asset type `tiled` handles `.tmj` files and `tiledWorld` handles `.world` files, so `assets.load` finds the type from the extension. Loading a map also loads its external tilesets, templates and images.

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
```

The loaded asset is a `Map` value that holds the parsed data and is cached by path like any asset. The function `tiled.newMapRenderer` makes a playable `MapRenderer` from it. Each playable map owns its own copy of the data, so `map:setTile` never changes the cached asset, and two maps made from the same asset are independent.

The functions `assets.load(path, nil, options)` and `assets.loadAsync(path, nil, options)` accept the texture options `filter` (`'nearest'` by default, or `'linear'`) and `wrap` for every image of the map. The function `loadAsync` parses the map and decodes its images on worker threads and returns a `Promise` that a coroutine can `:await()`. Maps can also be part of a preload group, which is how Tiny Island loads its island while the boot screen shows progress:

```json
{
    "groups": {
        "menu": ["maps/island.tmj", "tiny_swords/units/blue/", "effects/flames.particles"]
    }
}
```

See [Lua](lua.md) for async code with Varn Promises and coroutines.

## Drawing

The method `map:draw(camera, options)` draws every visible layer in map order into the active canvas. The method `map:drawLayer(name, camera, options)` draws one layer, including the children of a group, with the offset, parallax, tint and visibility it inherits from the groups above it. Drawing layer by layer is how the app's own sprites go between map layers.

- The `camera` gives the position for parallax, and culling and repeated image layers use the area the active canvas shows, including render target canvases. Without a camera nothing is culled and parallax layers shift as if the camera stood at the world origin. Repeated image layers need it.
- The `options` table holds a [draw order](lua-api/graphics2d.md#draw-order) with `layer` and `depth`, where each map layer replaces `blend` with its own blend mode, and `x` and `y`, the world position of the map origin, so the maps of a `.world` file draw at their places through one camera.
- Tile layers are baked into static GPU batches in regions of 32 by 32 cells the first time they draw, and only the regions the camera sees are submitted. The method `map:setTile` bakes only the region of its cell again, on the next draw. Animated tiles are drawn every frame with the frame that matches the map time.
- Object layers draw their visible tile objects and text objects. Other shapes are data for the app and for collision. Objects sort by their y position unless the layer draw order is `index`, and tile objects are placed by the tileset object alignment.
- A layer with a parallax factor other than 1 shifts by the distance between the camera position and the map parallax origin, times one minus the factor. A factor below 1 makes a distant background, and a factor above 1 makes a foreground that moves faster than the camera.
- The method `map:update(dt)` advances animated tiles and belongs in the scene `update`.

Tiny Island draws its ground layers under everything and its clouds over everything, with the game's own sprites in between. The world canvas sorts by depth, the ground layers go on a low layer with increasing depths, entities go on a middle layer with their y position as depth, and the clouds go on the highest layer. The code below is trimmed from `systems/island.lua` and `systems/game.lua`.

```lua
local groundLayers = {'foam', 'ground', 'shadow', 'cliffs', 'plateau'}

function island:drawGround(camera)
    for index, name in ipairs(groundLayers) do
        self.map:drawLayer(name, camera, {layer = config.layer.ground, depth = index})
    end
    self.map:drawLayer('decorations', camera, {layer = config.layer.ground, depth = #groundLayers + 1})
end

function island:drawClouds(camera)
    self.map:drawLayer('clouds', camera, {layer = config.layer.clouds})
end
```

```lua
graphics2d.beginWorld(self.camera, {sort = 'depth', ambientLight = self.cycle.ambient})
self.island:drawGround(self.camera)
for _, standing in ipairs(self.trees) do
    standing:draw()
end
self.player:draw()
self.island:drawClouds(self.camera)
```

The property `map.pixelBounds` is the world area the grid covers, which suits the camera limits. Tiny Island sets `camera.limits = map.pixelBounds` so the camera never shows past the edge of the island map. Drawing a layer with `{ysort = true}` sorts its rows and objects by the y they stand on, so entities in the same layer of a canvas that sorts by `'y'` or by depth walk behind and in front of trees and walls, as the [`haylen.tiled` reference](lua-api/tiled.md#drawing-rules) describes. See [Rendering](rendering.md) for canvases, cameras and batching.

## Reading the map

The map answers questions about its structure, so app code can read everything the designer put in Tiled.

| Function | Returns |
| --- | --- |
| `map:layers()` and `map:layer(name)` | Layer tables with their kind, visibility, opacity, blend, offset, parallax, tint and properties. Tile layers add their size, object layers their objects, image layers their image and group layers their children. |
| `map:objects(layer)` | The objects of one object layer, or of every object layer when `layer` is omitted, with their class in `type`, their shape, size, rotation, points and properties. |
| `map:tile(layer, column, row)` and `map:setTile(layer, column, row, gid)` | The global tile id of a cell with its flip flags, and a way to change it. |
| `map:tileInfo(gid)` | The tileset, class, properties, collision shapes and animation of a tile. |
| `map:tilesets()` | Every tileset with its Wang sets. |
| `map:cellToWorld(column, row)` and `map:worldToCell(x, y)` | Conversions between cells and world positions in every orientation. |
| `map:objectToWorld(x, y)` | Object coordinates of the map file converted to a world position, which differs from them on isometric and skewed oblique maps. |

The method `map:objects()` returns the raw coordinates of the map file without layer offsets. The method `map:spawn` hands out world positions that include the offsets of layers and groups, as described in [Spawning objects](#spawning-objects).

Map properties are on `map.properties`, and the map class is `map.type`. Tiny Island stores the seed that generated the island as the map property `seed`.

## Collision

The method `map:buildCollision(world)` creates static bodies in a [`haylen.physics2d`](lua-api/physics2d.md) world for the collision shapes of the map and returns them, one body per layer that has shapes.

- Tile layers add the collision shapes that tiles have in the tileset collision editor, placed and flipped with each tile. A tile layer with a `collision` property set to `false` adds nothing. On orthogonal maps, tiles whose collision is one rectangle covering the whole cell merge into one box per horizontal run of cells.
- Object layers whose class is `collision` add every object. Other object layers add only the objects whose class is `collision`. Points and text add nothing, polylines become one segment per line, ellipses and capsules become polygons, and tile objects cover their image where it draws, placed by the tileset object alignment.
- Objects and tile shapes with a `sensor` property set to `true` become sensors.
- The layer properties `category` and `mask` set the collision filter of the layer's shapes. Both are integers of at least 0, the category defaults to 1 and the mask to every bit.
- Layer visibility does not matter, so a hidden collision layer still collides.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}, pixelsPerMeter = 64})
local walls = map:buildCollision(world)
```

## Spawning objects

The method `map:spawn(factories, layer)` turns objects into app entities. The argument `factories` maps an object class to a function. Every object whose class has a factory, in the object layer `layer` or in every object layer when `layer` is omitted, is passed to its factory. The object table also carries `worldX` and `worldY`, the object origin in world coordinates including the offsets of its layer and groups. Objects without a factory are skipped, which leaves markers and collision shapes for other code. The method `spawn` returns the list of non-`nil` values the factories returned, in map order. Hidden objects and hidden layers are spawned too.

With the object classes of the Tiny Island map, spawning looks like this:

```lua
local spawned = map:spawn({
    campfire = function(object)
        return {kind = 'campfire', x = object.worldX, y = object.worldY}
    end,
    enemy_spawn = function(object)
        return {kind = 'spawn', x = object.worldX, y = object.worldY}
    end,
    tree_region = function(object)
        return {kind = 'trees', x = object.worldX, y = object.worldY, width = object.width, height = object.height}
    end,
}, 'gameplay')
```

Tiny Island itself reads the gameplay layer with `map:objects('gameplay')` and switches on `object.type`, because it gathers the points into its own tables rather than creating entities one by one. Both approaches read the same data, and the raw coordinates are world coordinates there because the island layers have no offsets and the map is orthogonal.

## Worlds

A `.world` file loads as a plain list with one table per map, each with its asset `path` and its `x`, `y`, `width` and `height` in world pixels. Maps listed in `maps` keep their order and are followed by the maps that `patterns` match. A world draws each map through the same camera, passing the map position as the `x` and `y` options of `map:draw`. The reference shows [a complete example](lua-api/tiled.md#placing-the-maps-of-a-world).

```lua
for _, entry in ipairs(assets.load('maps/world/overworld.world')) do
    print(entry.path, entry.x, entry.y, entry.width, entry.height)
end
```

## The Tiny Island map

The island lives in `samples/games/tiny-island/content/maps/` as `island.tmj` and four external tilesets. The tool `tools/generate_island_map.py` writes all of them from seeded noise, and they open and edit in Tiled 1.12. The file `samples/games/tiny-island/source/systems/island.lua` loads the map and gives the rest of the game what it needs from it.

The map is orthogonal, 56 by 36 cells of 64 pixels, with the water color `#47aba9` as its background color, which matches the `clearColor` in `app.json` so the sea fills the screen around the map.

| Layer | Kind | Content |
| --- | --- | --- |
| `foam` | Tile | Animated foam under every coast cell. |
| `ground` | Tile | Grass with edge tiles picked from the four neighbours of each cell. |
| `shadow` | Tile | Shadows under the cliff. |
| `cliffs` | Tile | The cliff face south of the plateau. |
| `plateau` | Tile | The raised ground in the north east. |
| `decorations` | Object | Bushes, rocks and rocks in the water as tile objects of class `decoration`. |
| `gameplay` | Object | Points and regions that the game reads by class. |
| `collision` | Object | Rectangles that block movement, in a hidden layer of class `collision`. |
| `clouds` | Object | Cloud tile objects of class `cloud`, with opacity 0.55 and parallax 1.25. |

The tilesets follow the Tiny Swords art that `python3 make.py assets` imports.

- The file `terrain.tsj` is an image tileset of 64-pixel tiles with two edge Wang sets, `Grass` and `High ground`, so the island can be repainted by hand with the Tiled terrain brush.
- The files `foam.tsj` and `shadow.tsj` hold 192-pixel tiles with a tile offset of `(-64, 64)`, which centers each large tile on its 64-pixel cell. The foam tile animates through 16 frames.
- The file `decorations.tsj` is an image collection with `objectalignment` set to `bottom`, so decorations stand on their point. Bushes and rocks in the water are sub-rectangles of strip images with tile animations.

The conventions the game relies on are these.

- Every tile layer has the property `collision` set to `false`, so `map:buildCollision` ignores the art and takes collision only from the `collision` layer. Those rectangles are merged from blocked cells (water, cliffs and plateau) and collide with the default category 1, which is `config.category.world` in the game.
- The `gameplay` layer holds one point of class `campfire`, one point of class `player_start`, twelve points of class `enemy_spawn` on walkable coast cells at twelve bearings around the fire and four rectangles of class `tree_region`, one per quarter of the island.
- The `campfire` point sits on a walkable cell at or near the middle of the island, and the player starts two cells south of it.

The file `island.lua` uses these conventions in a few lines. It builds physics collision with `map:buildCollision`, marks every cell under a `collision` rectangle as blocked in a [navigation grid](lua-api/navigation2d.md) with `map:worldToCell`, collects the gameplay points by class, and scatters trees with Poisson disk sampling inside the tree regions, away from the fire.

```lua
self.map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
self.world = physics2d.newWorld({gravity = {0, 0}, pixelsPerMeter = 64})
self.walls = self.map:buildCollision(self.world)

self.grid = navigation2d.newGrid(self.map.width, self.map.height)
for _, wall in ipairs(self.map:objects('collision')) do
    local left, top = self.map:worldToCell(wall.x, wall.y)
    local right, bottom = self.map:worldToCell(wall.x + wall.width - 1, wall.y + wall.height - 1)
    for row = top, bottom do
        for column = left, right do
            self.grid:setWalkable(column, row, false)
        end
    end
end

for _, object in ipairs(self.map:objects('gameplay')) do
    if object.type == 'campfire' then
        self.fire = {x = object.x, y = object.y}
    elseif object.type == 'enemy_spawn' then
        self.spawns[#self.spawns + 1] = {x = object.x, y = object.y}
    end
end
```

### Regenerating the map

```sh
python3 make.py map
```

The command `make.py map` runs `tools/generate_island_map.py --package samples/games/tiny-island` with the default seed `20260927` and rewrites `island.tmj`, `terrain.tsj`, `foam.tsj`, `shadow.tsj` and `decorations.tsj` in the sample's `content/maps` folder. The tileset images come from the imported Tiny Swords pack, so run `python3 make.py assets <path to the Tiny Swords zip>` first, and the generator stops with a message when the art is missing. The same seed always gives the same island. Another island comes from calling the tool with a seed of its own:

```sh
python3 tools/generate_island_map.py --package samples/games/tiny-island --seed 7
```

Regenerating overwrites the five files, including any edit made to them in Tiled. The game reads whatever `island.tmj` holds, so a hand-edited island works as long as it keeps the layer names and object classes above. See [Build](build.md) for the other `make.py` commands.

## From C++

The runtime is plain C++ under `haylen/2d/tiled/`, in the `haylen::tiled` namespace. The class `tiled::Map` (`Map.hpp`) holds the parsed data, with one header per model type such as `Layer`, `Object`, `Tileset`, `Tile`, `WangSet` and `Properties`, and `tiled::World` (`World.hpp`) reads `.world` files. The class `tiled::MapRenderer` (`MapRenderer.hpp`), the class behind the Lua `MapRenderer`, draws a map through a `graphics2d::Renderer`, visits objects with `forEachObject` and builds collision into a `physics2d::World`, and `tiled::ObjectFactories` (`ObjectFactories.hpp`) registers one factory per object class and spawns them, like `map:spawn` does in Lua.
