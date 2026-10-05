# haylen.assets

Loads the files of the app package: textures, fonts, shaders, sounds, JSON data, sprite atlases, particle effects and Tiled maps. Use it to load an asset right away, to load one in the background with a promise, and to preload named groups of assets behind a loading screen.

```lua
local assets = require('haylen.assets')
```

## Paths

Every path is relative to the `content/` folder of the app package and never includes the `content/` prefix, so the file `content/images/hero.png` is loaded as `'images/hero.png'`. Slashes and backslashes both separate folders. Absolute paths raise `The path "<path>" must be relative.`, and `..` segments that leave the folder raise `The path "<path>" must stay inside its root folder.` A missing file raises `The package file "<file>" was not found.`

## Asset types

The file extension picks the asset type, and the type decides what Lua receives. Extensions are matched without regard to case. Options are tables whose keys each type validates. Unknown keys raise `Unknown key "<key>" in <kind> options.`, where the kind names the options, such as `texture options` or `sound options`.

| Type | Extensions | Lua value | Options |
| --- | --- | --- | --- |
| `texture` | `.png`, `.jpg`, `.jpeg`, `.tga`, `.bmp`, `.gif` | Texture from [`haylen.graphics`](graphics.md). | The option `filter` is `'nearest'` (default) or `'linear'`. The option `wrap` is `'clamp'` (default), `'repeat'` or `'mirror'`. |
| `font` | `.ttf`, `.otf` | Font from [`haylen.graphics`](graphics.md). | The option `bakeSize` is the em size the glyphs are baked at in the distance field atlas (default `48`). The option `spread` is how far the distance field reaches past a glyph, in pixels at the bake size (default `8`). The option `atlasSize` is the starting side of the glyph atlas in pixels, which grows as new glyphs are used (default `512`). |
| `bitmapFont` | `.fnt` | Bitmap Font from a BMFont file in its text or binary format, whose page images sit next to it. | The `filter` and `wrap` of its page textures. |
| `gridFont` | None, so load an image with the type `'gridFont'` | Bitmap Font from an image of equal cells. | The options `characters`, `cellWidth` and `cellHeight`, which it needs, and `spacing`, `margin`, `advance`, `lineHeight` and `baseline`, like `graphics.newGridFont` from [`haylen.graphics`](graphics.md), plus the `filter` and `wrap` of its texture. |
| `shader` | `.shader` | Shader from [`haylen.graphics`](graphics.md), compiled by `haylen.py shaders` as the [shader guide](../shaders.md) explains. | None. |
| `json` | `.json` | Plain Lua table, with objects as string keyed tables and arrays as sequences. | None. |
| `sound` | `.wav`, `.ogg`, `.mp3`, `.flac` | Sound from [`haylen.audio`](audio.md). | The option `stream` keeps the encoded file and decodes it while it plays (default `false`). |
| `atlas` | None, so load it with the type `'atlas'` | A `SpriteAtlas` from [`haylen.animation2d`](animation2d.md). | The `filter` and `wrap` of its texture. |
| `particles` | `.particles` | Particle effect for `particles2d.newEmitter()` from [`haylen.particles2d`](particles2d.md). | The `filter` and `wrap` of its texture. |
| `tiled` | `.tmj` | Tiled map data for `tiled.newMapRenderer()` from [`haylen.tiled`](tiled.md). | The `filter` and `wrap` of its tileset and layer images. |
| `tiledWorld` | `.world` | List of the maps of a Tiled world. | None. |

Assets are cached by type, path and options. Loading the same asset again returns the same object while anything still holds it, such as a Lua variable, another asset or a preload group, so two loads of one asset compare equal with `==`, whatever its type. Different options load a separate asset. Atlases, particle effects and Tiled maps share their images with textures loaded directly with the same `filter` and `wrap`, except Tiled images with a transparent color, which stay private to their map.

## Loading

### assets.load(path, type, options)

Loads an asset synchronously and returns it. The type is optional and comes from the file extension when it is `nil`. Options are optional. A file whose extension no type handles raises `No asset type handles the file "<path>". Pass its type or use an extension that an asset type handles.`, and an unknown type raises `The asset type "<type>" does not exist.`

```lua
local assets = require('haylen.assets')

local hero = assets.load('images/hero.png')
local pixels = assets.load('images/tiles.png', nil, {filter = 'nearest', wrap = 'repeat'})
local level = assets.load('data/level.json')
local theme = assets.load('music/theme.ogg', 'sound', {stream = true})
local atlas = assets.load('sprites/hero.json', 'atlas')
print(hero.width, hero.height, #level.enemies, theme.duration, #atlas:frameNames())
```

### assets.texture(path, options)

Loads a texture synchronously with the `filter` and `wrap` options and returns it.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local portrait = assets.texture('images/portrait.png', {filter = 'linear'})

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.draw(portrait, 40, 40)
    end,
})
```

### assets.font(path, options)

Loads a font synchronously and returns it: a TrueType or OpenType font with the `bakeSize`, `spread` and `atlasSize` options, or a BMFont `.fnt` file with the `filter` and `wrap` of its pages. Any other extension raises `expected a .ttf, .otf or .fnt file`. Load a grid font with `assets.load(path, 'gridFont', options)`.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local title = assets.font('fonts/title.ttf', {bakeSize = 64})

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(title, 'Tiny Island', 80, 80, {size = 72})
    end,
})
```

### assets.shader(path)

Loads a `.shader` file synchronously and returns its Shader, for `graphics2d.newMaterial`. On Metal the programs of the shader start compiling on worker threads as soon as the file is read, and a scene that loads its shaders with `assets.loadAsync` or a preload group in its `load` hook gives them the time of its loading, as [shader load time](../shaders.md#load-time) explains. A file that is not a compiled shader raises `The shader file is malformed: ` followed by the problem, or `The shader file is malformed, and the JSON reader reported "<reason>".` when it is not valid JSON. In development, a changed `.shader` file reloads in place, so every material of the shader draws with the new programs from the next frame.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local shader = assets.shader('shaders/ripple.shader')
local ripple = graphics2d.newMaterial(shader, {strength = 0.03})
print(shader.name, #shader.uniforms)
```

### assets.json(path)

Reads a JSON file and returns it as plain Lua values.

```lua
local assets = require('haylen.assets')

local config = assets.json('data/balance.json')
print(config.player.speed)
```

### assets.text(path)

Reads a file and returns its bytes as a Lua string, without caching it.

```lua
local assets = require('haylen.assets')

local credits = assets.text('data/credits.txt')
for line in credits:gmatch('[^\n]+') do
    print(line)
end
```

### assets.bytes(path, offset, count)

Reads a file and returns its raw bytes as a Lua string, without caching it. Lua strings hold any bytes, so the result is the same as `assets.text()`, and the name tells readers that the file is binary data, such as a custom level format.

```lua
local assets = require('haylen.assets')

local level = assets.bytes('levels/cave.bin')
local width, height = string.unpack('<I2I2', level)
print(#level, width, height)
```

With an `offset` and a `count`, it reads only that range of the file: `count` bytes from the byte at `offset`, which starts at 0, or fewer when the file ends first, and an empty string from an offset at or past the end. Only the part of the file that holds the range is read, so files of any size, such as a large data set or a recording, are read in parts in bounded memory. A whole read of a file larger than one gibibyte raises `The package file "<file>" holds <size> bytes, more than a whole read allows. Read it in ranges instead.`, and such files are read in ranges. A negative offset or count raises the `bad argument` error of Lua.

```lua
local assets = require('haylen.assets')

-- Reads a large file of fixed-size records one record at a time.
local recordSize = 64
local count = assets.fileSize('levels/history.bin') // recordSize
for index = 0, count - 1 do
    local record = assets.bytes('levels/history.bin', index * recordSize, recordSize)
    local tick, x, y = string.unpack('<I4ff', record)
    print(tick, x, y)
end
```

## Loading in the background

### assets.loadAsync(path, type, options)

Starts loading an asset in the background and returns a promise. The type and options work as in `assets.load()`. Reading and decoding run on worker threads, and GPU resources are created on the frame thread within the [upload budget](#assetssetuploadbudgetseconds) of each frame, so the app keeps running smoothly while large files load.

Promises come from Varn's `async` module. Call `:await()` on a promise inside a coroutine started with `async.spawn()`. The coroutine pauses until the asset is ready and resumes during a later frame. The method `:await()` returns the asset on success and returns `nil` and the error message on failure instead of raising, so check the first value. An unknown type or invalid options still raise at once, when `assets.loadAsync()` is called.

```lua
local async = require('async')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local log = require('haylen.log')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        async.spawn(function()
            local background, err = assets.loadAsync('images/background.png'):await()
            if not background then
                log.error('The background did not load: ' .. err)
                return
            end
            self.background = background
        end)
    end,
    render = function(self)
        graphics2d.beginScreen()
        if self.background then
            graphics2d.draw(self.background, 0, 0)
        end
    end,
})
```

Several loads can run at once and be awaited together with `async.all()`, which returns `nil` and the first error when any of them fails.

```lua
local async = require('async')
local assets = require('haylen.assets')

async.spawn(function()
    local loaded, err = async.all({
        assets.loadAsync('images/hero.png'),
        assets.loadAsync('sfx/jump.wav'),
        assets.loadAsync('maps/island.tmj'),
    }):await()
    if loaded then
        local hero, jump, island = loaded[1], loaded[2], loaded[3]
        print(hero.width, jump.duration, island.path)
    end
end)
```

## Files

### assets.exists(path)

Returns `true` when the file exists in the content folder.

```lua
local assets = require('haylen.assets')

if assets.exists('maps/bonus.tmj') then
    print('the bonus level is included')
end
```

### assets.fileSize(path)

Returns the size of a file in bytes, without reading it. A missing file raises `The package file "<file>" was not found.`

```lua
local assets = require('haylen.assets')

print(assets.fileSize('music/theme.ogg'))
```

### assets.list(folder)

Returns a sorted list of every file under a folder of the content folder, including files in subfolders, as paths relative to the content folder. Without a folder it lists every asset. A missing folder gives an empty list, and a folder that leads out of the content folder, such as `'..'`, raises `The path "<folder>" must stay inside its root folder.`

```lua
local assets = require('haylen.assets')

for _, path in ipairs(assets.list('maps')) do
    if path:match('%.tmj$') then
        print(path)
    end
end
```

### assets.typeForPath(path)

Returns the name of the asset type that handles the file, picked by its extension without regard to case, as `assets.load()` picks it. A file whose extension no type handles raises `No asset type handles the file "<path>". Pass its type or use an extension that an asset type handles.`

```lua
local assets = require('haylen.assets')

for _, path in ipairs(assets.list('sfx')) do
    if assets.typeForPath(path) == 'sound' then
        print('sound effect', path)
    end
end
```

### assets.hasType(name)

Returns `true` when an asset type with that name is registered, such as `'texture'`, `'sound'` or a type a C++ plugin added.

```lua
local assets = require('haylen.assets')

if assets.hasType('tiled') then
    print('Tiled maps can be loaded')
end
```

## Preload groups

A preload group names a set of assets that load together in the background and are released together. Groups suit loading screens and levels: preload the group of the next level, show progress while it loads, and unload the group of the previous level.

A group manifest is a table or a JSON file with a `groups` object. Every group is a list of entries, and each entry is either a path or a table with `path` and optional `type` and `options`. A path that ends with `/` includes every file under that folder, in subfolders too, whose extension an asset type handles, and the `type` and `options` of that entry apply to each of those files.

```json
{
  "groups": {
    "menu": ["ui/", "fonts/title.ttf", "music/menu.ogg"],
    "island": [
      "maps/island.tmj",
      "images/units/",
      {"path": "sprites/hero.json", "type": "atlas"},
      {"path": "music/island.ogg", "options": {"stream": true}}
    ]
  }
}
```

### assets.defineGroups(manifest)

Defines every group of a manifest, given as a table or as the path of a JSON file in the content folder. A group that already exists gets the new entries. A manifest key other than `groups` raises `Unknown key "<key>" in the asset group manifest.`, and an entry key other than `path`, `type` and `options` raises `Unknown key "<key>" in an asset group entry.`.

```lua
local assets = require('haylen.assets')

assets.defineGroups('preload.json')
assets.defineGroups({groups = {
    credits = {'images/credits/', 'music/credits.ogg'},
}})
```

### assets.defineGroup(name, entries)

Defines one group from a list of entries in the manifest format.

```lua
local assets = require('haylen.assets')

assets.defineGroup('forest', {
    'maps/forest.tmj',
    'sfx/',
    {path = 'sprites/wolf.json', type = 'atlas', options = {filter = 'nearest'}},
    {path = 'music/forest.ogg', options = {stream = true}},
})
```

### assets.preload(name, progress)

Starts loading every asset of a group and returns a promise. The promise resolves once every asset finished, with a list of error messages in the form `'<path>: <error>'` for the assets that failed, so an empty list means everything loaded. It never rejects. The optional `progress` function receives the fraction of finished assets, from above 0 to 1, after each asset. Preloading a group that is already loaded loads nothing again, reports progress 1 and resolves with its earlier errors. An error raised by the progress function shows the engine error screen. An unknown group raises `The asset group "<name>" is not defined.`, and an entry without a type whose extension no type handles raises `No asset type handles the file "<path>". Pass its type or use an extension that an asset type handles.`

The group holds its assets until `assets.unloadGroup()`, so they stay cached even when nothing else refers to them, and later `assets.load()` calls return them at once.

```lua
local async = require('async')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local log = require('haylen.log')
local scene = require('haylen.scene')

local loading = {
    enter = function(self)
        self.fraction = 0
        assets.defineGroups('preload.json')
        async.spawn(function()
            local failures = assets.preload('island', function(fraction)
                self.fraction = fraction
            end):await()
            for _, failure in ipairs(failures) do
                log.warning(failure)
            end
            scene.replace(require('scenes.island'), {duration = 0.5})
        end)
    end,
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawRect({100, 500, 600 * self.fraction, 20}, '#FFFFFFFF')
    end,
}

scene.push(loading)
```

### assets.unloadGroup(name)

Releases the hold of a group on its assets. Assets that nothing else refers to leave memory, and assets still in use stay loaded. The group stays defined and can be preloaded again. An unknown group raises `The asset group "<name>" is not defined.`

```lua
local assets = require('haylen.assets')
local scene = require('haylen.scene')

scene.push({
    exit = function(self)
        assets.unloadGroup('island')
    end,
})
```

### assets.groupProgress(name)

Returns the fraction of the group that finished loading, from 0 to 1. A loaded group returns 1. An unknown group raises `The asset group "<name>" is not defined.`

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

assets.defineGroups('preload.json')
assets.preload('menu')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(nil, string.format('%d%%', assets.groupProgress('menu') * 100), 40, 40)
    end,
})
```

### assets.groupLoaded(name)

Returns `true` when the group finished loading. An unknown group returns `false`.

```lua
local assets = require('haylen.assets')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if assets.groupLoaded('menu') then
            scene.replace(require('scenes.menu'))
        end
    end,
})
```

### assets.groups()

Returns a sorted list of the names of every defined group.

```lua
local assets = require('haylen.assets')

for _, name in ipairs(assets.groups()) do
    print(name, assets.groupLoaded(name))
end
```

## Memory

### assets.releaseUnused()

Removes the cache entries of assets that nothing holds anymore and returns how many it removed. The memory of those assets was already freed when their last holder let go, so this only trims the cache bookkeeping. The engine also calls it when the platform reports low memory.

```lua
local assets = require('haylen.assets')

collectgarbage()
print(assets.releaseUnused())
```

### assets.cachedCount()

Returns how many cached assets are still alive, which is how many distinct assets something holds.

```lua
local assets = require('haylen.assets')

print('assets in memory', assets.cachedCount())
```

### assets.pendingCount()

Returns how many assets are loading in the background, counting each asset once however many `assets.loadAsync()` calls or preload groups wait for it.

```lua
local assets = require('haylen.assets')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        self.busy = assets.pendingCount() > 0
    end,
})
```

### assets.setUploadBudget(seconds)

Sets how much time of each frame, 4 milliseconds by default, the asset manager spends creating the GPU resources of assets that finished decoding in the background, such as uploading textures or building font atlases. Once the budget runs out the rest waits for the next frame, and at least one asset finishes every frame, so a burst of loads spreads over frames instead of stalling one, and a transition that reveals a scene right after its load stays smooth. A budget of 0 finishes exactly one asset per frame, and a negative budget raises `The upload budget cannot be negative.` An app in the background creates no GPU resources, so its decoded assets wait until it comes back.

```lua
local assets = require('haylen.assets')

-- A game that loads behind a covered screen can afford more of each frame.
assets.setUploadBudget(0.008)
print(assets.uploadBudget())
```

### assets.uploadBudget()

Returns the upload budget of a frame in seconds.

```lua
local assets = require('haylen.assets')

print(string.format('%.1f ms per frame', assets.uploadBudget() * 1000))
```

### assets.uploadCount()

Returns how many decoded assets wait for the frame thread to create their GPU resources.

```lua
local assets = require('haylen.assets')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        self.uploading = assets.uploadCount() > 0
    end,
})
```

While debug hot reload runs, a changed texture updates in place, so every sprite that uses it shows the new pixels. Other changed assets leave the cache, and the next load reads the new file.

## Asset types of other modules

### Sounds

Sound files load as `haylen.Sound` for [`haylen.audio`](audio.md). Streaming suits music and long ambience, and decoding suits short effects that play often.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local coin = assets.load('sfx/coin.wav')
local music = assets.load('music/island.ogg', 'sound', {stream = true})
audio.play(coin)
audio.playMusic(music)
```

### Sprite atlases

Sprite atlases are JSON files exported by TexturePacker or Aseprite in the hash or array layout. They have no extension of their own, so pass the type `'atlas'`. The image path stored in the file is relative to the JSON file, and Aseprite frame tags become animations. The atlas is described in [`haylen.animation2d`](animation2d.md).

```lua
local assets = require('haylen.assets')

local hero = assets.load('sprites/hero.json', 'atlas', {filter = 'nearest'})
print(hero.texture.width, #hero:animationNames())
```

### Bitmap fonts

BMFont files from tools such as BMFont, Hiero or Littera load with `assets.font` or `assets.load`, in their text or binary format, and name their page images relative to the file. Images of equal cells load with the type `'gridFont'`. Both are fonts from [`haylen.graphics`](graphics.md) that draw text anywhere a TrueType font does.

```lua
local assets = require('haylen.assets')

local pixel = assets.font('fonts/pixel.fnt', {filter = 'nearest'})
local digits = assets.load('fonts/digits.png', 'gridFont', {characters = '0123456789', cellWidth = 12, cellHeight = 16, filter = 'nearest'})
print(pixel.nativeSize, digits:hasGlyph('7'))
```

### Particle effects

The `.particles` files describe a particle effect and name its texture relative to the file. The effect is passed to `particles2d.newEmitter()` from [`haylen.particles2d`](particles2d.md).

```lua
local assets = require('haylen.assets')
local particles2d = require('haylen.particles2d')
local scene = require('haylen.scene')

local sparks = particles2d.newEmitter(assets.load('effects/sparks.particles'))

scene.push({
    update = function(self, dt)
        sparks:update(dt)
    end,
})
```

### Tiled maps

The `.tmj` files are Tiled maps in the JSON format. Their tilesets and images load with them. The asset is map data with a `path` property, and `tiled.newMapRenderer()` from [`haylen.tiled`](tiled.md) turns it into a map to draw and query.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))

scene.push({
    enter = function(self)
        self.camera = graphics2d.newCamera()
    end,
    render = function(self)
        graphics2d.beginWorld(self.camera)
        map:draw(self.camera)
    end,
})
```

### Tiled worlds

The `.world` files are Tiled worlds. They load as a list of tables with the fields `path`, `x`, `y`, `width` and `height`, one for each map of the world, with the map path relative to the content folder.

```lua
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local maps = {}
for _, entry in ipairs(assets.load('maps/world/overworld.world')) do
    maps[#maps + 1] = {map = tiled.newMapRenderer(assets.load(entry.path)), x = entry.x, y = entry.y}
end
```

## Errors

| Message | Cause |
| --- | --- |
| `The package file "<file>" was not found.` | The file does not exist in the package. |
| `The package file "<file>" holds <size> bytes, more than a whole read allows. Read it in ranges instead.` | A whole read asks for a file larger than one gibibyte. |
| `The path "<path>" must be relative.` | A path starts with a slash or a drive letter. |
| `The path "<path>" must stay inside its root folder.` | A path uses `..` to leave the content folder. |
| `An asset path cannot be empty.` | The path is empty. |
| `No asset type handles the file "<path>". Pass its type or use an extension that an asset type handles.` | The extension belongs to no asset type and no type was given. |
| `The asset type "<type>" does not exist.` | The type argument names no asset type. |
| `Unknown key "<key>" in <kind> options.` | An options table has a key the asset type does not accept. |
| `The texture filter must be "nearest" or "linear", not "<filter>".` | The `filter` option is not `nearest` or `linear`. |
| `The texture wrap must be "clamp", "repeat" or "mirror", not "<wrap>".` | The `wrap` option is not `clamp`, `repeat` or `mirror`. |
| `The asset group "<name>" is not defined.` | A group function names a group that was never defined. |
| `Unknown key "<key>" in the asset group manifest.` | A manifest has a key other than `groups`. |
| `Unknown key "<key>" in an asset group entry.` | A group entry has a key other than `path`, `type` and `options`. |

Decoding errors, such as an image that is not a valid PNG, raise from `assets.load()` and come back as the second value of `:await()` for `assets.loadAsync()`.
