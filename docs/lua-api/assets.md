# 📖 `haylen.assets`

The module `haylen.assets` loads the files of the app package: textures, fonts, shaders, sounds, JSON data, sprite atlases, particle effects, Tiled maps and more. Use it to load an asset at once, to load one in the background with a promise, and to preload named groups of assets behind a loading screen. The [loading guide](../content/loading.md) explains how these loads work with autoloads, scenes, content that downloads and the caches of a release.

```lua
local assets = require('haylen.assets')
```

## Paths

Every path is relative to the `content/` folder of the app package and never includes the `content/` prefix. So the file `content/images/hero.png` loads as `'images/hero.png'`. Slashes and backslashes both separate folders.

- An absolute path raises `The path "<path>" must be relative.`.
- A `..` segment that leaves the folder raises `The path "<path>" must stay inside its root folder.`.
- An empty path raises `An asset path cannot be empty.`.
- A missing file raises `The package file "<file>" was not found.`.

## Asset types

The file extension picks the asset type, and the type decides what Lua receives. Extensions match without regard to case. Options are tables whose keys each type checks. Unknown keys raise `Unknown option "<key>".`, and options that are not a table raise a `bad argument` error.

| Type | Extensions | Lua value | Options |
| --- | --- | --- | --- |
| `texture` | `.png`, `.jpg`, `.jpeg`, `.tga`, `.bmp`, `.gif` | Texture from [`haylen.graphics`](graphics.md). | The option `filter` is `'nearest'` (default) or `'linear'`. The option `wrap` is `'clamp'` (default), `'repeat'` or `'mirror'`. The option `mipmaps` is `false` (default) or `true`. |
| `font` | `.ttf`, `.otf` | Font from [`haylen.graphics`](graphics.md). | The option `bakeSize` is the em size that the glyphs are baked at in the distance field atlas (default `48`). The option `spread` is how far the distance field reaches past a glyph, in pixels at the bake size (default `8`). The option `atlasSize` is the starting side of the glyph atlas in pixels, which grows as new glyphs are used (default `512`). |
| `bitmapFont` | `.fnt` | Bitmap Font from a BMFont file in its text or binary format, whose page images sit next to it. | The `filter` and `wrap` of its page textures. |
| `gridFont` | None, so load an image with the type `'gridFont'` | Bitmap Font from an image of equal cells. | The options `characters`, `cellWidth` and `cellHeight`, which it needs, and `spacing`, `margin`, `advance`, `lineHeight` and `baseline`, like `graphics.newGridFont` from [`haylen.graphics`](graphics.md), plus the `filter` and `wrap` of its texture. |
| `shader` | `.shader` | Shader from [`haylen.graphics`](graphics.md), compiled by `haylen.py shaders` as the [shader guide](../graphics/shaders.md) explains. | None. |
| `vectorImage` | `.svg` | VectorImage from [`haylen.graphics`](graphics.md#vectorimage), an SVG document that draws sharp at any size. | None. |
| `json` | `.json` | A plain Lua table, with objects as tables with string keys and arrays as sequences. | None. |
| `sound` | `.wav`, `.ogg`, `.mp3`, `.flac` | Sound from [`haylen.audio`](audio.md). | The option `stream` reads the file in ranges while it plays instead of decoding it whole (default `false`). |
| `image` | None, so load it with the type `'image'` | An `Image` of pixels on the CPU from [`haylen.graphics`](graphics.md#image). | None. |
| `atlas` | None, so load it with the type `'atlas'` | A `SpriteAtlas` from [`haylen.animation2d`](animation2d.md). | The `filter`, `wrap` and `mipmaps` of its texture. |
| `skeleton` | `.skeleton` | `SkeletonData` from [`haylen.animation2d`](animation2d.md#skeletons), with the atlas that the file names. | The `filter`, `wrap` and `mipmaps` of its atlas texture. |
| `prefab` | `.prefab` | `Prefab` from [`haylen.nodes2d`](nodes2d.md#prefab), checked on a worker thread. | None. |
| `particles` | `.particles` | A particle effect for `particles2d.newEmitter()` and `particles2d.newSystem()` from [`haylen.particles2d`](particles2d.md). | The `filter` and `wrap` of its textures, which replace the ones that the file names. |
| `imageShape` | None, so load an image with the type `'imageShape'` | An `ImageShape` from [`haylen.particles2d`](particles2d.md#imageshape), the visible pixels of the image for the `image` shape of emitters. | The option `source` is the `{x, y, width, height}` part of the image to read, the whole image by default. The option `alphaThreshold` is the alpha that a pixel needs to count, `0.5` by default. |
| `tiled` | `.tmj` | Tiled map data for `tiled.newMapRenderer()` from [`haylen.tiled`](tiled.md). | The `filter` and `wrap` of its tileset and layer images, and the `project` file whose classes fill in default members. |
| `tiledWorld` | `.world` | A list of the maps of a Tiled world. | None. |
| `tiledRules` | None, so load a rules file or a rule map with the type `'tiledRules'` | Automapping rules for `map:automap()` from [`haylen.tiled`](tiled.md#automapping). | The `filter` and `wrap` of the tileset images of its rule maps. |

### The cache

Assets are cached by type, path and options. Loading the same asset again returns the same object while anything still holds it, such as a Lua variable, another asset or a preload group. So two loads of one asset compare equal with `==`, whatever its type. Different options load a separate asset.

Atlases, skeletons, particle effects and Tiled maps share their images with textures loaded directly with the same `filter` and `wrap`. Tiled images with a transparent color are the exception: they stay private to their map. A particle effect loads the images it names as textures of the cache before it is ready. So effects that share an image, such as the effects of one atlas, decode it once however many of them load at the same time.

## Loading

### assets.load(path, type, options)

Loads an asset at once and returns it. The `type` is optional, and the file extension picks it when it is `nil`. The `options` are optional.

A synchronous load never waits for a download. So an asset that [downloads later](../content/loading.md#delivery) and is not on the device yet raises `The asset "<path>" is not on the device yet, since it downloads when the app asks for it. Load it with "loadAsync" or a preload group, which bring it first.`. The functions `assets.json`, `assets.text` and `assets.bytes` raise the same. A file whose extension no type handles raises `No asset type handles the file "<path>". Pass its type or use an extension that an asset type handles.`, and an unknown type raises `The asset type "<type>" does not exist.`.

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

Loads a texture at once with the options `filter`, `wrap` and `mipmaps`, and returns it.

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

Loads a font at once and returns it. It takes a TrueType or OpenType font with the options `bakeSize`, `spread` and `atlasSize`, or a BMFont `.fnt` file with the `filter` and `wrap` of its pages. Load a grid font with `assets.load(path, 'gridFont', options)` instead.

A file whose extension no asset type handles, such as `.woff`, raises `No asset type handles the file "<path>". Pass its type or use an extension that an asset type handles.`. A file of another asset type, such as an image, raises the `bad argument` error of Lua with `expected a .ttf, .otf or .fnt file`. A font option that is not a number raises `The font option "<option>" must be a number, not <value>.`, with the value as JSON.

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

Loads a `.shader` file at once and returns its Shader, for `graphics2d.newMaterial`. On Metal the programs of the shader start compiling on worker threads as soon as the file is read. So a scene that loads its shaders with `assets.loadAsync` or a preload group in its `load` hook gives them the time of its loading, as [shader load time](../graphics/shaders.md#load-time) explains. In development, a changed `.shader` file reloads in place, so every material of the shader draws with the new programs from the next frame.

A file that is not a compiled shader raises `The shader file is malformed: ` followed by the problem. A file that is not valid JSON raises `The shader file is malformed, and the JSON reader reported "<reason>".`.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local shader = assets.shader('shaders/ripple.shader')
local ripple = graphics2d.newMaterial(shader, {strength = 0.03})
print(shader.name, #shader.uniforms)
```

### assets.vectorImage(path)

Loads an `.svg` file at once and returns its [`VectorImage`](graphics.md#vectorimage), for [`graphics2d.drawVector`](graphics2d.md#graphics2ddrawvectorimage-x-y-options). A file that is not an SVG document with a size raises `The bytes are not an SVG document with a size. Give its root element a view box, or a width and a height.`.

```lua
local assets = require('haylen.assets')

local icon = assets.vectorImage('icons/heart.svg')
print(icon.width, icon.height)
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

Reads a file and returns its raw bytes as a Lua string, without caching it. Lua strings hold any bytes, so the result is the same as `assets.text()`. The name tells readers that the file is binary data, such as a level format of your own.

```lua
local assets = require('haylen.assets')

local level = assets.bytes('levels/cave.bin')
local width, height = string.unpack('<I2I2', level)
print(#level, width, height)
```

With an `offset` and a `count`, it reads only that range of the file: `count` bytes from the byte at `offset`, which starts at `0`. It returns fewer bytes when the file ends first, and an empty string from an offset at or past the end. Only the part of the file that holds the range is read, so a file of any size, such as a large data set or a recording, reads in parts in bounded memory.

- A whole read of a file larger than one gibibyte raises `The package file "<file>" holds <size> bytes, more than a whole read allows. Read it in ranges instead.`. Read such a file in ranges.
- A range that covers more than one gibibyte of the file raises `The range of <count> bytes of the package file "<file>" is more than one read allows. Read it in smaller ranges instead.`. A larger count, such as `math.maxinteger`, still reads the rest of a smaller file.
- A negative offset or count raises the `bad argument` error of Lua.

```lua
local assets = require('haylen.assets')

-- Reads a large file of records of a fixed size, one record at a time.
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

Starts loading an asset in the background and returns a promise. The `type` and `options` work as in [`assets.load`](#assetsloadpath-type-options). An asset that is not on the device yet downloads first, as [`haylen.content`](content.md) brings it, and a failed download rejects the promise with its message. Reading and decoding run on worker threads. The frame thread creates the GPU resources within the [upload budget](#assetssetuploadbudgetseconds) of each frame, so the app keeps running smoothly while large files load.

Promises come from the `async` module of Varn. Call `:await()` on a promise inside a task started with `async.spawn()`. The task pauses until the asset is ready and resumes in a later frame. The method `:await()` returns the asset on success. On failure it returns `nil` and the error message instead of raising, so check the first value. A load whose promise nothing awaits or catches stops the app when it fails, as an [unobserved rejection](../core/lua.md#asynchronous-code) does. An unknown type or invalid options still raise at once, when `assets.loadAsync()` is called.

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

Several loads can run at once. Wait for them together with `async.all()`, which returns `nil` and the first error when any of them fails.

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
    print('The bonus level is included.')
end
```

### assets.bytesAsync(path, offset, count)

Reads a file, or the range of `count` bytes at `offset`, like [`assets.bytes`](#assetsbytespath-offset-count), but on the I/O pool, so a large read never holds a frame up. It returns a promise that resolves to the bytes as a Lua string. A file that downloads on demand in a protected release comes down first, as `assets.loadAsync` brings it. The promise rejects with the messages of `assets.bytes` and of `content.prefetch`: for a file that does not exist, for a range that is too large and for a download that fails.

```lua
local async = require('async')
local assets = require('haylen.assets')

async.spawn(function()
    local header, err = assets.bytesAsync('recordings/match.bin', 0, 64):await()
    if not header then
        print(err)
        return
    end
    print(string.unpack('<I4', header))
end)
```

### assets.fileSize(path)

Returns the size of a file in bytes, without reading it. A missing file raises `The package file "<file>" was not found.`.

```lua
local assets = require('haylen.assets')

print(assets.fileSize('music/theme.ogg'))
```

### assets.list(folder)

Returns a sorted list of every file under a folder of the content folder, files in subfolders included, as paths relative to the content folder. Without a folder it lists every asset. A missing folder gives an empty list. A folder that leads out of the content folder, such as `'..'`, raises `The path "<folder>" must stay inside its root folder.`. An app that runs from a folder raises `The package folder "<folder>" could not be listed, and the file system reported "<reason>".` when a folder under it cannot be read, instead of listing part of its files.

```lua
local assets = require('haylen.assets')

for _, path in ipairs(assets.list('maps')) do
    if path:match('%.tmj$') then
        print(path)
    end
end
```

### assets.typeForPath(path)

Returns the name of the asset type that handles the file, picked by its extension without regard to case, as `assets.load()` picks it. A file whose extension no type handles raises `No asset type handles the file "<path>". Pass its type or use an extension that an asset type handles.`.

```lua
local assets = require('haylen.assets')

for _, path in ipairs(assets.list('sfx')) do
    if assets.typeForPath(path) == 'sound' then
        print('Sound effect', path)
    end
end
```

### assets.hasType(name)

Returns `true` when an asset type with that name is registered, such as `'texture'`, `'sound'` or a type that a C++ plugin added.

```lua
local assets = require('haylen.assets')

if assets.hasType('tiled') then
    print('Tiled maps can be loaded.')
end
```

## Preload groups

A preload group names a set of assets that load together in the background and are released together. Groups suit loading screens and levels: preload the group of the next level, show progress while it loads, and unload the group of the previous level. The [loading guide](../content/loading.md#preload-groups) shows groups at work with scenes.

A group manifest is a table or a JSON file with a `groups` object. Each group is a list of entries. An entry is either a path, or a table with `path` and optional `type` and `options`. A path that ends with `/` includes every file under that folder, in subfolders too, whose extension an asset type handles, and the `type` and `options` of that entry apply to each of those files. A JSON file that `content.groups` of `app.json` names also decides delivery: a rule of `content.delivery` with `group` instead of `match` makes the files of a group download in the background or on demand, as the [delivery settings](../content/content.md#delivery) describe.

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

Defines every group of a manifest, given as a table or as the path of a JSON file in the content folder. A group that already exists gets the new entries. An empty table counts as an empty list. A manifest that breaks the format raises one of these messages:

- A manifest key other than `groups` raises `Unknown key "<key>" in the asset group manifest.`.
- An entry key other than `path`, `type` and `options` raises `Unknown key "<key>" in an asset group entry.`.
- A manifest without a `groups` table raises `The asset group manifest needs "groups", which maps each group name to its list of entries.`.
- A group that is not a list raises `The asset group "<name>" must be a list of entries.`.
- An entry that is neither a path nor a table with a `path` string, an optional `type` string and an optional `options` table raises `The asset group "<name>" has the entry <entry>, which is not a path or an object with a "path" string, an optional "type" string and optional "options".`, with the entry as JSON.

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

### assets.preload(name, progress, options)

Starts loading every asset of a group and returns a promise. The assets of the group that are not on the device yet download first, in one request that takes the first half of the progress. So a scene that preloads its groups in its `load` hook brings its content down without code of its own.

The promise resolves once every asset finished, with a list of error messages of the form `'<path>: <error>'` for the assets that failed. An empty list means that everything loaded, and the promise never rejects. Preloading a group that is already loaded loads nothing again, reports progress `1` and resolves with its earlier errors. When the group unloads while it loads, its promises resolve on a later frame with the one error `The asset group "<name>" was unloaded before it finished loading.`.

The optional `progress` function receives the fraction of finished assets, from above `0` to `1`, after each asset. An error raised by it shows the engine error screen. The optional `options` table takes `owner`, a table or a userdata such as a scene. The owner ends the progress function when it is released or collected, like the [owners of `haylen.events`](events.md#owners), and the progress function never keeps its owner alive. Without an `owner`, the progress function takes the default owner, which in development is the module that preloads at its top level.

The group checks every entry before it starts to load. So an error leaves the group as it was, without the progress function of that call.

- Unknown keys of `options` raise `Unknown option "<key>".`, and an owner that is not a table or a userdata raises `An owner must be a table or a userdata, not <type>.`.
- A `progress` that is not a function raises a `bad argument` error.
- An unknown group raises `The asset group "<name>" is not defined.`.
- An entry without a type whose extension no type handles raises `No asset type handles the file "<path>". Pass its type or use an extension that an asset type handles.`, and an entry with invalid options raises the error of its options.

The group holds its assets until `assets.unloadGroup()`. So they stay cached even when nothing else refers to them, and later `assets.load()` calls return them at once.

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
            end, {owner = self}):await()
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

Releases the hold of a group on its assets. Assets that nothing else refers to leave memory, and assets still in use stay loaded. A group that is still loading stops, and the promises of its preloads resolve on a later frame with the one error `The asset group "<name>" was unloaded before it finished loading.`. The group stays defined and can be preloaded again. An unknown group raises `The asset group "<name>" is not defined.`.

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

Returns the fraction of the group that finished loading, from `0` to `1`. A loaded group returns `1`. An unknown group raises `The asset group "<name>" is not defined.`.

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

Removes the cache entries of assets that nothing holds anymore and returns how many it removed. The memory of those assets was already freed when their last holder let go, so this only trims the bookkeeping of the cache. The engine also calls it when the platform reports low memory.

```lua
local assets = require('haylen.assets')

collectgarbage()
print(assets.releaseUnused())
```

### assets.cachedCount()

Returns how many cached assets are still alive, which is how many distinct assets something holds.

```lua
local assets = require('haylen.assets')

print('Assets in memory', assets.cachedCount())
```

### assets.pendingCount()

Returns how many assets load in the background. Each asset counts once, however many `assets.loadAsync()` calls or preload groups wait for it.

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

Sets how much time of each frame the asset manager spends creating the GPU resources of assets that finished decoding in the background, such as uploading textures or building font atlases. The budget is 4 milliseconds by default. Once it runs out, the rest waits for the next frame, and at least one asset finishes every frame. So a burst of loads spreads over frames instead of stalling one, and a transition that reveals a scene right after its load stays smooth. A budget of `0` finishes exactly one asset per frame, and a negative budget raises `The upload budget cannot be negative.`. An app in the background creates no GPU resources, so its decoded assets wait until it comes back.

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

## Hot reload of assets

While an app runs in development, a changed file reloads the assets read from it.

- Textures, shaders and JSON update in place. So every sprite that uses a texture shows the new pixels, and `assets.json` returns the new data even while a preload group holds it. Each updated asset publishes `assetReloaded`.
- The other types leave the cache, so the next load reads the new file. A live asset of such a type, such as a sound, a sprite atlas, a particle effect, a map or a vector image, publishes `assetChanged` with its type and path, so the app loads it again where it uses it.
- A changed font file restarts the app, because the text and the UI bake their fonts into atlases.

The [Lua guide](../core/lua.md#hot-reload) describes the rest of hot reload.

```lua
local assets = require('haylen.assets')
local events = require('haylen.events')

local level = {map = assets.json('levels/forest.json')}

-- A level that an editor changes while the app runs reads it again.
events.on('assetReloaded', function(event)
    if event.path == 'levels/forest.json' then
        level.map = assets.json(event.path)
    end
end, {owner = level})
```

## Events

The asset manager publishes these events on [`haylen.events`](events.md#engine-events), queued for the end of the frame. Each one carries a table with the `type` and the `path` of the asset.

| Event | When |
| --- | --- |
| `assetLoaded` | An asset entered the cache. |
| `assetUnloaded` | The last holder of a cached asset let it go, such as the last preload group that held it. |
| `assetReloaded` | A changed file updated a live asset in place during hot reload. |
| `assetChanged` | A changed file left a live asset of a type that cannot update in place behind during hot reload, so the app loads the file again. |

```lua
local events = require('haylen.events')

events.on('assetUnloaded', function(asset)
    print('Released ' .. asset.type .. ' ' .. asset.path)
end)
```

## Asset types of other modules

### Sounds

Sound files load as `haylen.Sound` for [`haylen.audio`](audio.md). Streaming suits music and long ambience, and decoding suits short effects that play often. A streamed sound never reads its file whole. Loading it reads only the header, and each voice reads the file in ranges while it plays and keeps only a bounded window of encoded bytes, which a protected release decrypts chunk by chunk. So a track of any length plays in bounded memory.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local coin = assets.load('sfx/coin.wav')
local music = assets.load('music/island.ogg', 'sound', {stream = true})
audio.play(coin)
audio.playMusic(music)
```

### Sprite atlases

Sprite atlases are JSON files in the hash or array layout, with frame tags and slices. They have no extension of their own, so pass the type `'atlas'`. The image path stored in the file is relative to the JSON file, and its frame tags become animations. [`haylen.animation2d`](animation2d.md) describes the atlas.

```lua
local assets = require('haylen.assets')

local hero = assets.load('sprites/hero.json', 'atlas', {filter = 'nearest'})
print(hero.texture.width, #hero:animationNames())
```

### Bitmap fonts

BMFont files load with `assets.font` or `assets.load`, in their text or binary format, and name their page images relative to the file. Images of equal cells load with the type `'gridFont'`. Both are fonts from [`haylen.graphics`](graphics.md) that draw text anywhere a TrueType font does.

```lua
local assets = require('haylen.assets')

local pixel = assets.font('fonts/pixel.fnt', {filter = 'nearest'})
local digits = assets.load('fonts/digits.png', 'gridFont', {characters = '0123456789', cellWidth = 12, cellHeight = 16, filter = 'nearest'})
print(pixel.nativeSize, digits:hasGlyph('7'))
```

### Particle effects

A `.particles` file describes a particle effect, or a composite of several, and names its textures and the effect files it uses relative to the file. Pass the effect to `particles2d.newEmitter()` or `particles2d.newSystem()` from [`haylen.particles2d`](particles2d.md). The type `imageShape` reads the visible pixels of an image for emitters that spawn in the shape of a sprite.

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

A `.tmj` file is a Tiled map in the JSON format. Its tilesets and images load with it. The asset is map data with a `path` property, and `tiled.newMapRenderer()` from [`haylen.tiled`](tiled.md) turns it into a map to draw and query.

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

A `.world` file is a Tiled world. It loads as a list of tables with the fields `path`, `x`, `y`, `width` and `height`, one for each map of the world, with the map path relative to the content folder.

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
| `The range of <count> bytes of the package file "<file>" is more than one read allows. Read it in smaller ranges instead.` | A range read covers more than one gibibyte of a file. |
| `The package folder "<folder>" could not be listed, and the file system reported "<reason>".` | A folder of an app that runs from a folder cannot be read. |
| `The path "<path>" must be relative.` | A path starts with a slash or a drive letter. |
| `The path "<path>" must stay inside its root folder.` | A path uses `..` to leave the content folder. |
| `An asset path cannot be empty.` | The path is empty. |
| `The asset "<path>" is not on the device yet, since it downloads when the app asks for it. Load it with "loadAsync" or a preload group, which bring it first.` | A synchronous load names an asset that downloads later and has not arrived. |
| `No asset type handles the file "<path>". Pass its type or use an extension that an asset type handles.` | The extension belongs to no asset type, and no type was given. |
| `The asset type "<type>" does not exist.` | The type argument names no asset type. |
| `Unknown option "<key>".` | An options table has a key that the asset type does not accept. |
| `The texture filter must be "nearest" or "linear", not <filter>.` | The `filter` option is not `nearest` or `linear`. The message shows it as JSON, such as `"blurry"` or `3`. |
| `The texture wrap must be "clamp", "repeat" or "mirror", not <wrap>.` | The `wrap` option is not `clamp`, `repeat` or `mirror`. The message shows it as JSON. |
| `The texture option "mipmaps" must be true or false, not <value>.` | The `mipmaps` option is not a boolean. The message shows it as JSON. |
| `The font option "<option>" must be a number, not <value>.` | The `bakeSize`, `spread` or `atlasSize` of a font is not a number. |
| `The upload budget cannot be negative.` | The budget of `assets.setUploadBudget` is below `0`. |
| `The asset group "<name>" is not defined.` | A group function names a group that was never defined. |
| `Unknown key "<key>" in the asset group manifest.` | A manifest has a key other than `groups`. |
| `Unknown key "<key>" in an asset group entry.` | A group entry has a key other than `path`, `type` and `options`. |
| `The asset group manifest needs "groups", which maps each group name to its list of entries.` | A manifest has no `groups` table. |
| `The asset group "<name>" must be a list of entries.` | A group of a manifest is not a list. |
| `The asset group "<name>" has the entry <entry>, which is not a path or an object with a "path" string, an optional "type" string and optional "options".` | An entry is not a path or a table with a `path` string, an optional `type` string and an optional `options` table. |
| `The asset group "<name>" was unloaded before it finished loading.` | The one error that a preload resolves with when its group unloads while it loads. |

Decoding errors, such as an image that is not a valid PNG, raise from `assets.load()` and come back as the second value of `:await()` for `assets.loadAsync()`.
