# 📖 `haylen.graphics`

The module `haylen.graphics` makes the resources that every kind of drawing shares: buffers, textures, render targets, images on the CPU, fonts, font families and vector images. It also names the graphics API and what the device can do: its capabilities, what it does with each pixel format and the range of depth. Drawing in 2D, and the materials that draw with shaders, live in [`haylen.graphics2d`](graphics2d.md). The [rendering guide](../graphics/rendering.md) explains how the engine draws, and the [text guide](../graphics/text.md) explains fonts.

```lua
local graphics = require('haylen.graphics')
```

## Graphics device

### graphics.apiName()

Returns the name of the graphics API that the GPU device draws through: `'metal'`, `'direct3D12'`, `'direct3D11'`, `'vulkan'`, `'openGl'`, `'openGlEs'`, `'webGl2'`, `'webGpu'`, `'port'` on the device of a port to another platform, or `'none'` on the headless host, where nothing draws. It is the value of [`haylen.graphicsApi`](haylen.md#haylengraphicsapi). The [RHI guide](../graphics/rhi.md#choosing-the-graphics-api) lists the APIs of each platform.

```lua
local graphics = require('haylen.graphics')
local log = require('haylen.log')

log.info('Rendering with ' .. graphics.apiName())
if graphics.apiName() == 'webGl2' then
    log.info('The browser draws through WebGL 2, so programs compile when the first draw needs them.')
end
```

### graphics.maxTextureSize()

Returns the largest width and height in pixels that a texture or render target can have on this device, which is never more than [`graphics.maxImageSize()`](#graphicsmaximagesize). A larger size raises `Texture dimensions exceed the device limit.`.

```lua
local graphics = require('haylen.graphics')

local size = math.min(4096, graphics.maxTextureSize())
local atlas = graphics.newRenderTarget(size, size)
print(atlas.width, graphics.maxTextureSize())
```

### graphics.trim()

Gives back to the system the GPU memory that the device holds for later, such as the empty memory blocks and the upload memory that a burst of loads left behind, as the [RHI guide](../graphics/rhi.md#memory-held-for-later) describes. The engine already trims two seconds after that memory stopped growing and when the system warns that its memory is low, so an app calls it only at a moment it knows about, such as after it left a level and released its art. It never waits for the GPU, and what the frames in flight still read goes as they finish.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

assets.releaseUnused()
graphics.trim()
```

### graphics.maxImageSize()

Returns the longest side in pixels that an `Image` can have on every platform, 16384. It keeps every image within the memory of a device and within the 32-bit arithmetic of its pixels. The images that `graphics.newImage` and `graphics.decodeImage` make and the rasters of vector images never have a longer side.

```lua
local graphics = require('haylen.graphics')

local side = math.min(8192, graphics.maxImageSize())
local canvas = graphics.newImage(side, 64)
print(canvas.width)
```

### graphics.capabilities()

Returns a table of what the GPU device of this app can do, as the [RHI guide](../graphics/rhi.md#capabilities) reports it for each graphics API. The values come from the GPU when the device starts, so they differ between machines.

| Field | Type | Meaning |
| --- | --- | --- |
| `maxTextureSize` | integer | The largest side in pixels of a flat texture or render target, the value of [`graphics.maxTextureSize()`](#graphicsmaxtexturesize). |
| `maxArrayLayers` | integer | The most layers of an array texture. |
| `maxVolumeSize` | integer | The largest side in pixels of a volume texture, along its width, its height and its slices, which the 3D renderer reads for volumes such as the grids of fog and light. |
| `maxCubeSize` | integer | The largest side in pixels of the square faces of a cube texture, such as the faces of an environment map. |
| `cubeArrays` | boolean | Whether textures of the kind `'cubeArray'` exist and shaders sample them. OpenGL ES before 3.2 and WebGL2 have none. |
| `imageViews` | boolean | Whether [`texture:view`](#textureviewoptions) makes views of textures. OpenGL ES without a texture view extension and WebGL2 have none. |
| `originTopLeft` | boolean | Whether the first row of a render target is its top row. It is `false` on OpenGL, OpenGL ES and WebGL2. |
| `depthRange` | string | The range of depth of clip space, as [`graphics.depthRange()`](#graphicsdepthrange) returns it. |
| `independentBlend` | boolean | Whether each color target of a draw takes a blend of its own. |
| `gpuTiming` | boolean | Whether the device measures the time the GPU takes for a frame, which [`debug.stats()`](debug.md#debugstats) reports. |
| `maxAnisotropy` | integer | The most samples of anisotropic filtering that the option `anisotropy` of a texture takes, 1 on a device without anisotropic filtering. |
| `maxLodBias` | number | The largest bias of the level of detail that the option `lodBias` takes in either direction, 0 on a device whose samplers take no bias, such as Metal, OpenGL ES, WebGL2 and WebGPU. |
| `borderColors` | boolean | Whether textures read a border color through the wrap `'border'`. WebGL2 and WebGPU have none. |
| `storageBuffers` | boolean | Whether buffers take the usage `'storage'`. OpenGL ES 3.0 and WebGL2 have none. |
| `indirectBuffers` | boolean | Whether buffers take the usage `'indirect'`. OpenGL ES 3.0 and WebGL2 have none. |
| `maxUniformBlockSize` | integer | The most bytes of one uniform block of a shader. |
| `uniformBufferAlignment` | integer | The multiple of bytes, 256, at which the uniform blocks of shaders read a uniform buffer. |

```lua
local graphics = require('haylen.graphics')

local capabilities = graphics.capabilities()
local slices = math.min(64, capabilities.maxVolumeSize)
local face = math.min(1024, capabilities.maxCubeSize)
print('A volume of ' .. slices .. ' slices and cubes of ' .. face .. ' pixels fit this device.')
```

### graphics.formatSupport(format)

Returns a table of what the GPU device does with a pixel format, so an app picks a format that the device has. The formats are the uncompressed color formats `'r8'`, `'rg8'`, `'rgba8'`, `'rgba8Srgb'`, `'bgra8'`, `'rgb10A2'`, `'rg11B10Float'`, `'r16Float'`, `'rg16Float'`, `'rgba16Float'`, `'r32Float'`, `'rg32Float'` and `'rgba32Float'`, the depth formats `'depth16'`, `'depth32Float'`, `'depth24Stencil8'` and `'depth32FloatStencil8'`, the BC formats `'bc1'`, `'bc1Srgb'`, `'bc2'`, `'bc2Srgb'`, `'bc3'`, `'bc3Srgb'`, `'bc4'`, `'bc4Snorm'`, `'bc5'`, `'bc5Snorm'`, `'bc6hUfloat'`, `'bc6hFloat'`, `'bc7'` and `'bc7Srgb'`, the ETC2 and EAC formats `'etc2Rgb8'`, `'etc2Rgb8Srgb'`, `'etc2Rgb8A1'`, `'etc2Rgb8A1Srgb'`, `'etc2Rgba8'`, `'etc2Rgba8Srgb'`, `'eacR11'`, `'eacR11Snorm'`, `'eacRg11'` and `'eacRg11Snorm'`, and the ASTC formats of every block size from `'astc4x4'` to `'astc12x12'`, which are `'astc4x4'`, `'astc5x4'`, `'astc5x5'`, `'astc6x5'`, `'astc6x6'`, `'astc8x5'`, `'astc8x6'`, `'astc8x8'`, `'astc10x5'`, `'astc10x6'`, `'astc10x8'`, `'astc10x10'`, `'astc12x10'` and `'astc12x12'`, each also with `Srgb` and with `Hdr` after its name, such as `'astc6x6Srgb'` and `'astc6x6Hdr'` of the HDR profile. Another name raises `bad argument #1 to 'formatSupport' (unknown value '<name>')`.

| Field | Type | Meaning |
| --- | --- | --- |
| `sample` | boolean | Whether textures of the format exist and draws sample them. A block compressed format that the device lacks is `false`, and the [texture guide](../graphics/rendering.md#compressed-textures) explains how an app ships the formats of each target. |
| `filter` | boolean | Whether draws sample the format with the `'linear'` filter. |
| `render` | boolean | Whether render targets, or depth attachments for the depth formats, of the format exist. Block compressed formats never render. |
| `blend` | boolean | Whether draws blend into render targets of the format. |
| `storage` | boolean | Whether shaders write the format as a storage image. |
| `mipmaps` | boolean | Whether the GPU generates the mipmaps of a texture of the format from its first level, as the option `mipmaps` of a texture made from pixels or from one level asks. Depth and block compressed formats never do. |
| `sampleCounts` | list of integers | The sample counts that render targets and depth attachments of the format take, from one sample up, such as `{1, 4}`, or an empty list for a format that the device does not draw into. A color format that takes more samples than one also resolves into a texture of one sample. |

```lua
local graphics = require('haylen.graphics')

local light = graphics.formatSupport('rgba16Float')
print('The most samples of a light buffer: ' .. light.sampleCounts[#light.sampleCounts])

local format = graphics.formatSupport('astc6x6').sample and 'astc6x6' or 'bc7'
print('The textures of this device use ' .. format .. '.')
```

### graphics.depthRange()

Returns the range of depth in the clip space of this device, `'zeroToOne'` or `'negativeOneToOne'`, which the projections of [`haylen.math`](math.md) take as their `depthRange`, so a projection matches the device that draws, as the [coordinates guide](../3d/coordinates.md#depth) explains. Every device takes `'zeroToOne'`, except OpenGL, OpenGL ES and WebGL2 on a driver without clip control.

```lua
local graphics = require('haylen.graphics')
local m = require('haylen.math')

local projection = m.perspective({nearPlane = 0.1, depthRange = graphics.depthRange()})
print(projection:apply({0, 0, -0.1}).z)
```

## Texture

A `Texture` is a shared handle to an image on the GPU. Make one with `graphics.newTexture` or `graphics.whiteTexture`, or load one with `assets.texture(path, options)` from [`haylen.assets`](assets.md#assetstexturepath-options), which caches it: the same image loaded with the same options returns the same texture. Other textures come from `target.texture`, `atlas.texture` and `animation.texture`. Two texture values compare equal with `==` when they refer to the same GPU image.

A texture with the `'linear'` filter loaded from a file has the colors of its visible pixels bled into the transparent pixels around them while it decodes, as the [rendering guide](../graphics/rendering.md#textures-and-atlases) explains.

### graphics.newTexture(width, height, options)

Makes a `Texture` of `width` by `height` pixels from code. The table `options` is optional:

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `kind` | string | `'flat'` | The kind of the texture: `'flat'`, `'array'` of layers, `'cube'` of six faces, `'cubeArray'` of cubes or `'volume'` of slices, which the [kinds of textures](#kinds-of-textures) explain. |
| `layers` | integer | 6 for `'cube'` and `'cubeArray'`, 1 otherwise | The layers of an array, or the faces of the cubes of a cube array, six for each cube. |
| `depth` | integer | 1 | The slices of a volume. |
| `fill` | Color | `'#FFFFFFFF'` | The color of every pixel when `pixels` is absent. |
| `pixels` | string or data view | none | Raw RGBA bytes, four per pixel, row by row from the top, in a string or in a view of [`dataview`](https://github.com/varn-org/varn/blob/main/docs/lua-api/dataview.md). Its length must be `width * height * 4`, or the call raises `Image pixel data does not match its dimensions.`. A texture of another kind takes every layer or slice one after another, the first one first. |
| `dynamic` | boolean | `false` | Makes a texture whose pixels change in place with [`texture:update(pixels, options)`](#textureupdatepixels-options), such as a minimap that code draws. |
| `filter` | string | `'nearest'` | The `'nearest'` or `'linear'` sampling. |
| `wrap` | string | `'clamp'` | The addressing of every axis, `'clamp'`, `'repeat'`, `'mirror'` or `'border'`, which reads `borderColor` outside the texture where [`graphics.capabilities()`](#graphicscapabilities) has `borderColors`. |
| `wrapU`, `wrapV`, `wrapW` | string | `wrap` | The addressing of one axis, across, down or through the slices, which replaces `wrap` on that axis. |
| `anisotropy` | integer | the `maxAnisotropy` of the device with the `'linear'` filter and mipmaps, and 1 otherwise | The most samples of anisotropic filtering, from 1 to the `maxAnisotropy` of the device, which a texture drawn much smaller along one side than the other takes along its longer side. More than one needs the `'linear'` filter and mipmaps. |
| `lodBias` | number | `0` | The bias that each lookup adds to its level of detail, within the `maxLodBias` of the device, where a negative bias reads sharper levels. A device whose `maxLodBias` is 0 takes no bias. |
| `minLod`, `maxLod` | number | `0`, `1000` | The levels of detail that lookups stay within, from 0 up, such as `maxLod = 2` for a texture that never reads its smallest levels. |
| `borderColor` | string | `'transparentBlack'` | The color that the wrap `'border'` reads, `'transparentBlack'`, `'opaqueBlack'` or `'opaqueWhite'`, the colors that every graphics API with border colors has. |
| `mipmaps` | boolean | `true` with the `'linear'` filter, `false` for a dynamic texture and otherwise | Keeps smaller copies of the image down to one pixel, which a texture drawn smaller than its size samples, blended between them with the `'linear'` filter. A flat texture makes them on the CPU as it is made. A texture of another kind and a dynamic texture with `mipmaps = true` have the GPU generate them from the first level, the dynamic one after each update, in a format whose [`graphics.formatSupport`](#graphicsformatsupportformat) has `mipmaps`. |

With the options `format` and `levels`, the texture takes its pixels in a pixel format of the device, such as the blocks of a compressed format that the app ships for its target, as the [texture guide](../graphics/rendering.md#compressed-textures) explains. These two options take no `fill`, `pixels` or `dynamic`, which raise `Unknown option "<key>".`:

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `format` | string | `'rgba8'` | A format of [`graphics.formatSupport`](#graphicsformatsupportformat) that the device samples and filters, such as `'bc7'` or `'astc6x6'`. |
| `levels` | list of strings or data views | none | The bytes of each mip level, the largest level first, each holding the rows of blocks of every layer or slice of its level tightly packed, the first layer first. A level smaller than a block takes one whole block. With more than one level the texture keeps mipmaps, unless `mipmaps = false` keeps only the first level. With one level and `mipmaps = true`, the GPU generates the others in a format whose `formatSupport` has `mipmaps`. |

These options also take `kind`, `layers` and `depth`, and a texture of the kind `'volume'` takes no block compressed format.

A texture made in a format checks it before any byte reaches the GPU. A depth format raises `The format "<format>" belongs to depth attachments, so no texture is made in it.`. A format that the device does not sample raises `The graphics API "<api>" of this device samples no texture of the format "<format>". Make the texture in a format that the device samples, as its format support tells.`, and one that it samples only without filtering raises the same kind of sentence with `only without filtering`. Sides that hold no whole blocks raise `A texture of the format "<format>" has sides of whole blocks of <w> by <h> pixels, which <width> by <height> pixels are not.`. A missing list raises `A texture made in a format takes the bytes of each of its levels in the list "levels", the largest level first.`, a list of another count raises `A texture of <width> by <height> pixels brings from 1 to <count> levels, not <given>.`, a level of another size raises `The level <n> of a texture of <width> by <height> pixels of the format "<format>" holds <bytes> bytes, and it needs <expected>.`, and `mipmaps = true` with one level of a format whose mipmaps the GPU does not generate raises `The graphics API "<api>" of this device generates no mipmaps of the format "<format>", so a texture of it brings each of its levels.`.

The sampling options check against the device before the texture is made. An `anisotropy` beyond the device raises `The texture option "anisotropy" takes from 1 to <most> samples on this device, not <anisotropy>.`, and more than one without the `'linear'` filter and mipmaps raises `Anisotropic filtering takes the filter "linear" and mipmaps.`. Levels of detail out of order raise `The levels of detail of a texture run from "minLod" up to "maxLod", from 0 on.`. A bias on a device without one raises `The graphics API "<api>" of this device takes no bias of the level of detail.`, and a bias beyond the device raises `The bias of the level of detail of a texture lies within <most> on this device, not <bias>.`. The wrap `'border'` on a device without border colors raises `The graphics API "<api>" of this device reads no border color, so its textures take no wrap "border".`.

A size of 0 or less raises `Texture dimensions must be positive.`, and a size above [`graphics.maxTextureSize()`](#graphicsmaxtexturesize) raises `Texture dimensions exceed the device limit.`. A filled texture checks its size before it makes any pixel. With `pixels`, the image of the pixels checks its size first: a negative size raises `Image dimensions cannot be negative.`, a side above 16384 raises `The image of <width> by <height> pixels is too large. An image has at most 16384 pixels on a side.`, and pixels of another length, an empty string among them, raise `Image pixel data does not match its dimensions.`.

```lua
local graphics = require('haylen.graphics')

local red = graphics.newTexture(4, 4, {fill = '#FFFF0000'})
local checker = graphics.newTexture(2, 2, {
    pixels = string.char(255, 255, 255, 255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255, 255),
    wrap = 'repeat',
})
print(red.width, checker.height)

if graphics.formatSupport('bc7').sample then
    local blocks = graphics.newTexture(4, 4, {format = 'bc7', levels = {string.rep('\0', 16)}, filter = 'linear'})
    print(blocks.format)
end
```

### Kinds of textures

A texture of the kind `'flat'` is an image that every draw shows. The other kinds hold several images that a [material](graphics2d.md#material) samples through a shader that declares a texture of that kind, as the [shader guide](../graphics/shaders.md) explains: an `'array'` holds layers of one size, such as the cascades of a shadow map or the frames of a flip book, a `'cube'` holds six square faces in the order +X, -X, +Y, -Y, +Z and -Z, such as an environment map, a `'cubeArray'` holds six faces for each of its cubes, such as the probes of a scene, and a `'volume'` holds slices, such as a grid of fog or a lookup of colors. A draw of `haylen.graphics2d` takes flat textures alone and raises `A texture of the kind "<kind>" is no image that a draw shows. Give it to a material whose shader samples a texture of that kind.` for another kind, and a material raises `The texture "<name>" of the shader "<shader>" takes a texture of the kind "<kind>", not "<other>".` for a texture of another kind than its shader declares.

The shape checks against the device before any pixel exists. A cube or a cube array of faces that are not square raises `A texture of the kind "<kind>" has square faces, which <w> by <h> pixels are not.`, a cube of other than 6 layers raises `A texture of the kind "cube" has 6 layers, one for each face, not <n>.`, and a cube array whose layers are no multiple of 6 or exceed `maxArrayLayers` raises `A texture of the kind "cubeArray" has 6 layers for each of its cubes, up to <most> on this device, not <n>.`. An array raises `A texture of the kind "array" has from 1 to <most> layers on this device, not <n>.` and a volume raises `A texture of the kind "volume" has from 1 to <most> slices on this device, not <n>.` beyond [`graphics.capabilities()`](#graphicscapabilities). A flat texture or a volume with layers raises `A texture of the kind "<kind>" has 1 layer, not <n>.`, and a depth on another kind than a volume raises `Only a texture of the kind "volume" has a depth, and one of the kind "<kind>" has none.`. A cube array on a device without them raises `The graphics API "<api>" of this device has no textures of the kind "cubeArray". Check "cubeArrays" of "graphics.capabilities()" before making one.`, and a shader that samples a cube array raises the same kind of sentence when it first draws there.

```lua
local graphics = require('haylen.graphics')

local sky = graphics.newTexture(64, 64, {kind = 'cube', fill = '#FF87CEEB', filter = 'linear'})
local cascades = graphics.newTexture(512, 512, {kind = 'array', layers = 4})
local fog = graphics.newTexture(32, 32, {kind = 'volume', depth = 32, filter = 'linear'})
print(sky.layers, sky.mipLevels, cascades.layers, fog.depth)

if graphics.capabilities().cubeArrays then
    local probes = graphics.newTexture(32, 32, {kind = 'cubeArray', layers = 6 * 8})
    print(probes.layers)
end
```

### graphics.newTexture(bytes, options)

Makes a `Texture` from `bytes`, a string or a data view with the contents of a PNG, JPEG, WebP, TGA, BMP or GIF file. Use it for an image that a plugin returns from native code or a file the app downloaded. The texture has the size of the image. The table `options` takes `dynamic`, `filter`, `wrap` and `mipmaps` as above. The options `fill` and `pixels` raise `Unknown option "<key>".`, since the image gives the pixels. Bytes that are not an image raise `The image is damaged, or it is not a PNG, JPEG, WebP, TGA, BMP or GIF file.`. Raw RGBA pixels take the form with the width and the height, and images of the package load with `assets.texture`.

```lua
local async = require('async')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local platform = require('haylen.platform')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        async.spawn(function()
            -- The native part answers with the PNG of a photo as bytes.
            local photo = platform.call('camera.takePhoto', {width = 640, height = 480}):await()
            if photo then
                self.texture = graphics.newTexture(photo.png, {filter = 'linear'})
            end
        end)
    end,
    render = function(self)
        if self.texture then
            graphics2d.beginScreen()
            graphics2d.draw(self.texture, 20, 20, {pivotX = 0, pivotY = 0})
        end
    end,
})
```

### graphics.newTexture(image, options)

Makes a `Texture` from an [`Image`](#image), with the options `filter`, `wrap` and `mipmaps` of `graphics.newTexture`. Other keys raise `Unknown option "<key>".`. Unlike textures loaded from files, it takes the pixels as they are. So call `image:bleed()` first for a `'linear'` texture whose transparent edges should not darken.

```lua
local graphics = require('haylen.graphics')

local badge = graphics.newImage(64, 64, '#FF2E7D32')
badge:bleed()
local texture = graphics.newTexture(badge, {filter = 'linear', mipmaps = true})
print(texture.mipmaps)
```

### graphics.whiteTexture()

Returns the 1 by 1 white `Texture` of the engine, handy for tinted quads in sprites and batches.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local pixel = graphics2d.newSprite(graphics.whiteTexture(), {width = 8, height = 8, color = '#FF40FF80'})
```

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `width` | integer | read | Width in pixels. |
| `height` | integer | read | Height in pixels. |
| `filter` | string | read | Sampling filter, `'nearest'` or `'linear'`. |
| `wrapU`, `wrapV`, `wrapW` | string | read | The addressing of each axis, `'clamp'`, `'repeat'`, `'mirror'` or `'border'`. |
| `anisotropy` | integer | read | The most samples of anisotropic filtering of the texture. |
| `lodBias`, `minLod`, `maxLod` | number | read | The bias of the level of detail and the levels that lookups stay within. |
| `borderColor` | string | read | The color that the wrap `'border'` reads. |
| `mipmaps` | boolean | read | Whether the texture keeps smaller copies of its image. |
| `format` | string | read | The pixel format of the texture on the GPU, `'rgba8'` for the textures of images and the format of a texture made from its levels. |
| `kind` | string | read | The [kind](#kinds-of-textures) of the texture, `'flat'`, `'array'`, `'cube'`, `'cubeArray'` or `'volume'`. |
| `layers` | integer | read | The layers of an array, the faces of a cube and of the cubes of a cube array, and 1 for the other kinds. |
| `depth` | integer | read | The slices of a volume, and 1 for the other kinds. |
| `mipLevels` | integer | read | The mip levels of the texture, 1 without mipmaps. |

```lua
local assets = require('haylen.assets')

local texture = assets.texture('sprites/knight/run.png', {filter = 'linear'})
print(texture.width, texture.height, texture.filter, texture.wrapU, texture.mipmaps, texture.anisotropy)
print(texture == assets.texture('sprites/knight/run.png', {filter = 'linear'}))
```

### texture:update(pixels, options)

Replaces the pixels of a texture made with `dynamic = true`. The value `pixels` holds every pixel of the texture as raw RGBA bytes, four per pixel, row by row from the top, and its length is `width * height * 4`. It is a string or a [data view](platform.md#data-views), whose memory the upload reads in place, such as a frame that a plugin fills. Any other value raises `Bytes come as a string or a data view that was not released, not as a <type>.`. The texture keeps its size, and every draw of the frame shows the new pixels.

| Option | Type | Default | Description |
| --- | --- | --- | --- |
| `region` | rectangle | the whole texture | The part of the texture that changed since its last update, as `{x, y, width, height}` or a table with those fields, in whole pixels from the top left corner. Only the pixels of the region go to the GPU, so a sprite that a paint app adds to a big canvas sends only its own pixels. |
| `layer` | integer | `0` | The layer of an array, the face of a cube or a cube array, or the slice of a volume, from 0, whose pixels `pixels` holds, every pixel of that layer. A layer that the texture lacks raises `The dynamic texture has <n> layers from 0, so it has no layer <layer>.`. |

A dynamic texture with `mipmaps = true` has the GPU generate its smaller levels from the new pixels before the frame draws. A texture that changes several times in one frame sends once, before the frame draws, the smallest region of each layer that holds every change, taken from its last pixels, which `graphics2d.stats().uploadedBytes` counts. A texture that is not dynamic raises `Only a dynamic texture changes its pixels in place.`, and pixels of another length raise `The pixels do not match the size of the dynamic texture.`. A region outside the texture or without a pixel raises `The region of a texture update must lie inside the dynamic texture and hold at least one pixel.`, and a region of fractions of a pixel raises `The region of "texture:update" must hold whole pixels.`.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local minimap = graphics.newTexture(2, 2, {dynamic = true})
local black, white = string.char(0, 0, 0, 255), string.char(255, 255, 255, 255)

scene.push({
    update = function(self, dt)
        local on = math.floor(os.clock() * 2) % 2 == 0
        minimap:update(on and (white .. black .. black .. white) or (black .. white .. white .. black))
    end,
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.draw(minimap, 20, 20, {width = 64, height = 64})
    end,
})
```

A paint app keeps the pixels of its canvas and names the region of each stroke, so the GPU receives only the pixels that the stroke covered.

```lua
local graphics = require('haylen.graphics')

local size = 128
local white, black = string.char(255, 255, 255, 255), string.char(0, 0, 0, 255)
local rows = {}
for row = 1, size do
    rows[row] = string.rep(white, size)
end

local canvas = graphics.newTexture(size, size, {dynamic = true, fill = '#FFFFFF'})

local function paintRow(y, from, to)
    local row = rows[y + 1]
    rows[y + 1] = row:sub(1, from * 4) .. string.rep(black, to - from) .. row:sub(to * 4 + 1)
    canvas:update(table.concat(rows), {region = {from, y, to - from, 1}})
end

paintRow(64, 32, 96)
```

### texture:view(options)

Returns a `Texture` that shows a range of the mip levels and layers of the texture, which a material samples as a texture of the kind of the view, such as one face of a cube through an array, or the levels of a texture from a smaller one while a pass draws into a larger one. The view takes the sampling options of its texture, the size of its first mip level, and keeps its texture alive. The table `options` is optional:

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `kind` | string | the kind of the texture | The kind of the view: the kind of the texture, or `'array'` for faces of a cube or a cube array. |
| `mipLevel` | integer | `0` | The first mip level of the view. |
| `mipLevels` | integer | every level from `mipLevel` | The mip levels of the view. |
| `layer` | integer | `0` | The first layer of an array view, or the first face of a cube array view, a multiple of 6. |
| `layers` | integer | every layer from `layer`, 6 for a cube, 1 for a flat texture and a volume | The layers of the view. A cube view takes the 6 faces, a cube array view whole cubes, and a flat view and a volume view the whole texture. |

A device without views raises `The graphics API "<api>" of this device makes no views of textures. Check "imageViews" of "graphics.capabilities()" before making one.`. Another kind raises `A texture of the kind "<kind>" has no view of the kind "<view>".`, mip levels outside the texture raise `The mip levels of a view lie within the <n> mip levels of its texture, which <count> levels from the level <first> do not.`, layers outside it raise `The layers of a view lie within the <n> layers of its texture, which <count> layers from the layer <first> do not.`, a cube view of other faces raises `A view of the kind "cube" shows the 6 faces of its cube, from the layer 0.`, a cube array view of part of a cube raises `A view of the kind "cubeArray" shows whole cubes, 6 layers for each, from a layer that is a multiple of 6.`, a flat or volume view of a part raises `A view of the kind "<kind>" shows the whole texture, so its layer is 0 and it has 1 layer.`, and a view of a view raises `A view shows a range of a texture, not of another view.`.

```lua
local graphics = require('haylen.graphics')

local sky = graphics.newTexture(64, 64, {kind = 'cube', filter = 'linear'})
if graphics.capabilities().imageViews then
    local side = sky:view({kind = 'array', layer = 2, layers = 1})
    local small = sky:view({mipLevel = 2})
    print(side.kind, side.layers, small.width, small.mipLevels)
end
```

## Buffer

A `Buffer` holds bytes on the GPU with the usages they serve, such as the vertices, the indices or the uniform blocks that a renderer of a plugin draws with, and an update that says how they change. The [RHI guide](../graphics/rhi.md#buffers) explains the usages and the updates. Two buffer values compare equal with `==` when they refer to the same buffer, and the GPU buffer goes once no value refers to it.

### graphics.newBuffer(size, options)

Makes a `Buffer` of `size` bytes, which starts with zeros, or, when the first argument is a string or a data view, one that starts with those bytes and takes their size.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `usage` | table | required | A list of what the buffer serves as: `'vertex'`, `'index'`, `'uniform'`, `'storage'`, `'indirect'`, `'copySource'` and `'copyDestination'`. `'storage'` needs `storageBuffers` and `'indirect'` needs `indirectBuffers` of [`graphics.capabilities()`](#graphicscapabilities). |
| `update` | string | `'immutable'` | How its contents change: `'immutable'` keeps the bytes it is made with, `'dynamic'` keeps what the last write or copy left, and `'stream'` holds in a frame only what that frame wrote. |
| `label` | string | `''` | The name of the buffer in GPU debuggers and captures. |

A size of 0 or less raises `The size of "graphics.newBuffer" must be a positive number of bytes.`, a buffer without a usage raises `A buffer needs at least one usage: ...`, and an unknown usage raises `The buffer usage "<name>" does not exist. ...`. A uniform buffer serves no other draw and raises `A uniform buffer serves no other draw. ...` with another one, an index buffer that holds vertices raises `An index buffer holds no vertices. ...`, an immutable buffer made with a size alone or with `'copyDestination'` raises `An immutable buffer keeps the data it is made with, so it needs data and takes no "copyDestination" usage.`, and a stream buffer with a copy usage raises `A stream buffer holds new contents every frame, so it takes no "copySource" or "copyDestination" usage.`.

```lua
local graphics = require('haylen.graphics')

local points = graphics.newBuffer(1024, {usage = {'vertex', 'copySource'}, update = 'dynamic', label = 'points'})
local corners = graphics.newBuffer(string.pack('<ffffffff', 0, 0, 1, 0, 0, 1, 1, 1), {usage = {'vertex'}})
print(points.size, corners.size, corners.update)
```

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `size` | integer | read | The size in bytes. |
| `usage` | table | read | The list of its usages, in the order of the table of `usage` above. |
| `update` | string | read | `'immutable'`, `'dynamic'` or `'stream'`. |
| `label` | string | read | The name that it was made with. |

### buffer:write(offset, bytes)

Writes the bytes, a string or a data view, into the buffer at the byte `offset`, a multiple of 4, before the draws of the frame that read them. A dynamic buffer keeps them and takes whole words, a size that is a multiple of 4 too, and a stream buffer holds in a frame only what that frame wrote, each part once. An immutable buffer raises `An immutable buffer keeps the data it was made with. Make it dynamic or stream to write into it.`, and a range outside the buffer or not of whole words raises a sentence that names the range.

```lua
local graphics = require('haylen.graphics')

local points = graphics.newBuffer(64, {usage = {'vertex'}, update = 'dynamic'})
points:write(16, string.pack('<ff', 0.5, 0.25))
```

### buffer:copy(source, options)

Copies bytes of the buffer `source`, which has the usage `'copySource'`, into this buffer, which has the usage `'copyDestination'`, before the draws of the frame that read them. The table `options` takes `sourceOffset` and `offset`, 0 by default, and `size`, the size of `source` by default, all multiples of 4. A buffer copies nothing into itself, and a range outside either buffer raises a sentence that names the range.

```lua
local graphics = require('haylen.graphics')

local staged = graphics.newBuffer(string.rep('\0', 256), {usage = {'copySource'}})
local points = graphics.newBuffer(1024, {usage = {'vertex', 'copyDestination'}, update = 'dynamic'})
points:copy(staged, {offset = 512})
```

### buffer:readAsync(offset, size)

Returns a promise of a string of the bytes of the buffer from `offset`, 0 by default, for `size` bytes, the rest of the buffer by default, both multiples of 4, as the frame that asks for them leaves the buffer. The GPU copies them after the frame is submitted, so the promise settles a frame or more later. The buffer needs the usage `'copySource'`. The headless host of tests draws nothing and hands over zeros.

```lua
local async = require('async')
local graphics = require('haylen.graphics')

local results = graphics.newBuffer(16, {usage = {'copySource'}, update = 'dynamic'})
results:write(0, string.pack('<i4', 42))
async.spawn(function()
    local bytes = results:readAsync(0, 4):await()
    print(string.unpack('<i4', bytes))
end)
```

## RenderTarget

A `RenderTarget` is a texture on the GPU that canvases draw into, such as a minimap, a mirror or a captured frame. Draw into it with `graphics2d.beginTarget` or `graphics2d.beginCapture`, and draw its picture with `target.texture`. Two target values compare equal with `==` when they refer to the same target.

### graphics.newRenderTarget(width, height, options)

Makes a `RenderTarget` of `width` by `height` pixels. The table `options` takes `filter`, `'nearest'` by default or `'linear'`, and `wrap`, `'clamp'` by default, `'repeat'` or `'mirror'`. A render target has no mipmaps, so `mipmaps = true` raises `A render target has no mipmaps.`. A size of 0 or less raises `Texture dimensions must be positive.`, and a size above [`graphics.maxTextureSize()`](#graphicsmaxtexturesize) raises `Texture dimensions exceed the device limit.`.

```lua
local graphics = require('haylen.graphics')

local target = graphics.newRenderTarget(320, 180, {filter = 'linear'})
print(target.width, target.height, target.texture.width)
```

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `width` | integer | read | Width in pixels. |
| `height` | integer | read | Height in pixels. |
| `texture` | Texture | read | The texture to draw the target with. It holds colors premultiplied by their alpha, so draw it with `blend = 'premultiplied'` wherever it is not fully opaque. |

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local target = graphics.newRenderTarget(128, 64)
local preview = graphics2d.newSprite(target.texture, {x = 64, y = 32, blend = 'premultiplied'})
```

## Image

An `Image` holds RGBA pixels on the CPU, for art that code makes, composes or packs before it becomes a texture. Examples are generated portraits, decals stamped into a map or the frames of a [runtime atlas](animation2d.md#atlasbuilder). Areas are rectangles of whole pixels, `{x, y, width, height}` or a `Rect`, and an area left out means the whole image.

Work on the task pool, such as `image:blitAsync` and `builder:buildAsync`, reads an image while Lua runs on. So an image that such work still reads is copied before Lua changes it: the work never sees a later change, and Lua never waits for it. The images of the asset cache stay as they were loaded.

### graphics.newImage(width, height, fill)

Makes an `Image` of `width` by `height` pixels, every pixel `fill`, which is transparent by default. A size under one pixel raises `An image needs a width and a height of one pixel or more.`, and a side longer than [`graphics.maxImageSize()`](#graphicsmaximagesize) raises `The image of <width> by <height> pixels is too large. An image has at most 16384 pixels on a side.`.

```lua
local graphics = require('haylen.graphics')

local canvas = graphics.newImage(256, 128, '#FF1E2A44')
print(canvas.width, canvas.height)
```

### graphics.decodeImage(bytes, options)

Decodes the bytes of a PNG, JPEG, WebP, TGA, BMP or GIF file into an `Image` on the frame thread. Bytes that are not an image raise `The image is damaged, or it is not a PNG, JPEG, WebP, TGA, BMP or GIF file.`. The `image` asset type of [`haylen.assets`](assets.md) loads the images of the package the same way, and `assets.loadAsync(path, 'image')` does it off the frame thread.

The decoder reads the size in the header of the file before it allocates a pixel. The table `options` takes `maxSize`, the longest side in pixels that the image may have, from 1 to `graphics.maxImageSize()`, which is the default. A larger image raises `The image of <width> by <height> pixels is too large to decode. The image may have at most <maxSize> pixels on a side.`. Bytes from outside the app, such as a picture that a person uploads, should name the size the app expects, so a small file that declares a huge image never takes the memory of one.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local icon = graphics.decodeImage(assets.bytes('icons/star.png'))
print(icon.width, icon.height)
local avatar = graphics.decodeImage(assets.bytes('icons/avatar.png'), {maxSize = 512})
print(avatar.width)
```

### graphics.decodeImageAsync(bytes, options)

Decodes like `graphics.decodeImage`, with the same `options`, on the task pool, and returns a promise of the image. The promise rejects with the same message when the bytes are not an image or the image is too large.

```lua
local async = require('async')
local graphics = require('haylen.graphics')
local http = require('http')

async.spawn(function()
    local response = http.client.get('https://example.com/shoes.png'):await()
    local image, failure = graphics.decodeImageAsync(response.body, {maxSize = 1024}):await()
    print(image and image.width or failure)
end)
```

### graphics.decodePlaceholder(code, width, height)

Decodes a text that [`image:placeholder`](#imageplaceholderoptions) made into a blurred `Image` of `width` by `height` pixels, from 1 to 512 on each side. A texture shows it scaled up with the `'linear'` filter until the real image arrives. A placeholder may come from anywhere, so a text that is not one raises `The text is not an image placeholder.`. A size out of range raises `An image placeholder decodes to a size from 1 to 512 pixels on each side.`.

```lua
local async = require('async')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local http = require('http')
local scene = require('haylen.scene')

-- A product of a list shows its placeholder at once, and its picture with the vivid color of the picture once it downloads.
local product = {name = 'Trail shoes', placeholder = 'FUqTgEQNZQ1lDWUAaw1lDWUNZQMlDWUNZQ1l'}
local cover = graphics.newTexture(graphics.decodePlaceholder(product.placeholder, 32, 24), {filter = 'linear'})

async.spawn(function()
    local response = http.client.get('https://example.com/shoes.png'):await()
    local image = graphics.decodeImageAsync(response.body):await()
    local swatches = image:swatches()
    product.tint = swatches.vibrant or swatches.dominant
    cover = graphics.newTexture(image, {filter = 'linear'})
end)

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.draw(cover, 40, 40, {width = 320, height = 240, pivotX = 0, pivotY = 0})
    end,
})
```

### graphics.readTarget(target)

Returns a promise of an `Image` of the pixels of the render target `target` as the current frame renders it. The GPU copies the pixels after the frame is submitted, without the frame thread waiting for it where the graphics API allows, as the [rendering guide](../graphics/rendering.md#reading-pixels-back) describes. The task pool then turns them into straight colors with the top row first, so the promise settles a frame or more later. A target that is not valid raises `A read needs a valid render target.`.

```lua
local async = require('async')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local portrait = graphics.newRenderTarget(128, 128)
local camera = graphics2d.newCamera()
camera.position = {64, 64}

scene.push({
    enter = function(self)
        async.spawn(function()
            local image = graphics.readTarget(portrait):await()
            print(image:pixel(64, 64):toHex())
        end)
    end,
    render = function(self)
        graphics2d.beginTarget(portrait, camera, {clear = '#FF203040'})
        graphics2d.drawCircle(64, 64, 40, '#FFFFD040')
    end,
})
```

### graphics.captureScreen()

Returns a promise of an `Image` of everything the screen shows in the next frame the app renders, the interface and the debug overlay included. It reads the pixels back the same way as `graphics.readTarget`. Save the image as a PNG file with `image:encodePngAsync()` and [`storage.writeTextAsync`](storage.md).

```lua
local async = require('async')
local graphics = require('haylen.graphics')
local storage = require('haylen.storage')

async.spawn(function()
    local shot = graphics.captureScreen():await()
    local png = shot:encodePngAsync():await()
    storage.writeTextAsync('screenshots/latest.png', png):await()
    print(shot.width, shot.height, #png)
end)
```

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `width` | integer | read | Width in pixels. |
| `height` | integer | read | Height in pixels. |

```lua
local graphics = require('haylen.graphics')

local image = graphics.newImage(32, 16)
print(image.width * image.height .. ' pixels')
```

### image:pixel(x, y)

Returns the `Color` of the pixel at `x`, `y`, counted from 0 at the top-left corner, or a transparent color outside the image.

```lua
local graphics = require('haylen.graphics')

local image = graphics.newImage(4, 4, '#FF40C060')
print(image:pixel(0, 0):toHex(), image:pixel(10, 10).a)
```

### image:setPixel(x, y, color)

Sets the pixel at `x`, `y` to `color`. Outside the image it does nothing.

```lua
local graphics = require('haylen.graphics')

local image = graphics.newImage(8, 8)
for index = 0, 7 do
    image:setPixel(index, index, '#FFFFFFFF')
end
```

### image:fill(color, area)

Fills the part of `area` that lies inside the image with `color`, or the whole image without an area.

```lua
local graphics = require('haylen.graphics')

local flag = graphics.newImage(90, 60, '#FFFFFFFF')
flag:fill('#FFC0392B', {0, 0, 30, 60})
flag:fill('#FF2E86C1', {60, 0, 30, 60})
```

### image:fillGradient(gradient, area)

Paints a [gradient](#gradients) laid over the whole `area` into the part of it inside the image, or over the whole image without an area.

```lua
local graphics = require('haylen.graphics')

local button = graphics.newImage(200, 60)
button:fillGradient({angle = math.pi / 2, colors = {'#FF4A78C8', '#FF1E2A44'}})
```

### image:blit(source, x, y, options)

Copies the image `source`, or the area `options.source` of it, to `x`, `y`, clipped to both images. It replaces the pixels it covers, or blends over them by alpha with `options.blend = true`. Other keys raise `Unknown option "<key>".`.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local sheet = graphics.decodeImage(assets.bytes('sprites/icons.png'))
local slot = graphics.newImage(64, 64, '#FF202020')
slot:blit(sheet, 8, 8, {source = {48, 0, 48, 48}, blend = true})
```

### image:blitAsync(source, x, y, options)

Returns a promise of a copy of the image with the blit done, both made on the task pool, while the image stays as it is. It takes the arguments of `image:blit`.

```lua
local assets = require('haylen.assets')
local async = require('async')
local graphics = require('haylen.graphics')

async.spawn(function()
    local badge = graphics.newImage(128, 128)
    badge:fill('#FF2E7D32', {8, 8, 112, 112})
    local icon = assets.loadAsync('icons/star.png', 'image'):await()
    badge = badge:blitAsync(icon, 32, 32, {blend = true}):await()
    local texture = graphics.newTexture(badge, {filter = 'linear', mipmaps = true})
    print(texture.width, texture.mipmaps)
end)
```

### image:crop(area)

Returns a new image of `area`. An area outside the image raises `The area <x>, <y>, <w> by <h> is not inside the image of <w> by <h> pixels.`.

```lua
local graphics = require('haylen.graphics')

local sheet = graphics.newImage(256, 64)
local first = sheet:crop({0, 0, 64, 64})
print(first.width)
```

### image:bleed()

Gives the transparent pixels around what shows the color of the nearest pixel that shows, and keeps their alpha at zero. This keeps the edges of a `'linear'` texture from darkening.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local leaf = graphics.decodeImage(assets.bytes('props/leaf.png'))
leaf:bleed()
local texture = graphics.newTexture(leaf, {filter = 'linear'})
```

### image:visibleArea()

Returns the `Rect` around every pixel that shows, or `nil` for an image without visible pixels.

```lua
local graphics = require('haylen.graphics')

local image = graphics.newImage(64, 64)
image:fill('#FFFFFFFF', {10, 20, 8, 8})
local area = image:visibleArea()
print(area.x, area.y, area.width, area.height)
```

### image:clone()

Returns a copy of the image.

```lua
local graphics = require('haylen.graphics')

local original = graphics.newImage(16, 16, '#FF000000')
local copy = original:clone()
copy:invert()
print(original:pixel(0, 0):toHex(), copy:pixel(0, 0):toHex())
```

### image:encodePng()

Returns the bytes of a PNG file of the image, made on the frame thread. An empty image raises `An empty image has no PNG file.`.

```lua
local graphics = require('haylen.graphics')

local png = graphics.newImage(8, 8, '#FFFF0000'):encodePng()
print(#png .. ' bytes')
```

### image:encodePngAsync()

Returns a promise of the bytes of a PNG file, encoded on the task pool from the image as it is now.

```lua
local async = require('async')
local graphics = require('haylen.graphics')
local storage = require('haylen.storage')

async.spawn(function()
    local png = graphics.newImage(64, 64, '#FF40C060'):encodePngAsync():await()
    storage.writeTextAsync('exports/swatch.png', png):await()
end)
```

### image:blur(radius)

Blurs the image as softly as a Gaussian of `radius` pixels, from 0 to 64, with three box blurs whose cost does not grow with the radius. It mixes the colors premultiplied by their alpha, so the transparent pixels around a shape never darken its edge. A radius out of range raises `A blur has a radius from 0 to 64 pixels.`.

```lua
local graphics = require('haylen.graphics')

local shadow = graphics.newImage(96, 96)
shadow:fill('#C0000000', {24, 24, 48, 48})
shadow:blur(12)
```

### image:sharpen(amount)

Raises the contrast of the details by `amount`, 1 by default, by subtracting a blur of one pixel. A negative amount raises `A sharpening has an amount of 0 or more.`.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local photo = graphics.decodeImage(assets.bytes('images/harbor.png'))
photo:sharpen(0.5)
```

### image:adjust(options)

Changes the colors with these keys, applied in this order:

| Option | Default | Meaning |
| --- | --- | --- |
| `hue` | `0` | Turns the hue by this many turns. |
| `saturation` | `1` | Scales the distance from gray. |
| `contrast` | `1` | Scales the distance from middle gray. |
| `brightness` | `0` | Adds to every channel. |

Unknown keys raise `Unknown option "<key>".`.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local photo = graphics.decodeImage(assets.bytes('images/harbor.png'))
photo:adjust({saturation = 1.3, contrast = 1.1, brightness = -0.05})
```

### image:grayscale(amount)

Moves the colors toward their gray by `amount`, 1 by default for full gray.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local locked = graphics.decodeImage(assets.bytes('ui/level-card.png'))
locked:grayscale()
```

### image:invert()

Inverts the colors and keeps the alpha.

```lua
local graphics = require('haylen.graphics')

local negative = graphics.newImage(32, 32, '#FF336699')
negative:invert()
print(negative:pixel(0, 0):toHex())
```

### image:tint(color, amount)

Moves the colors toward `color` by `amount`, 1 by default, weighed by the alpha of the color.

```lua
local graphics = require('haylen.graphics')

local frozen = graphics.newImage(32, 32, '#FF808080')
frozen:tint('#FF80C0FF', 0.6)
```

### image:pixelate(size)

Fills every square of `size` pixels with the average of its colors. A size below 1 raises `A pixelation has squares of 1 pixel or more.`.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local censored = graphics.decodeImage(assets.bytes('images/harbor.png'))
censored:pixelate(8)
```

### image:outline(color, thickness)

Draws an outline of `color` around the pixels that show. It is solid within `thickness` pixels, 1 by default and up to 32, and fades over the pixel past it. It draws under the pixels that show and inside the image, so a sprite needs a transparent margin as wide as its outline. A thickness out of range raises `An outline has a thickness from 0 to 32 pixels.`.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local selected = graphics.decodeImage(assets.bytes('units/archer.png'))
selected:outline('#FFFFE070', 3)
```

### image:applyAsync(effects)

Returns a promise of a copy of the image with the effects of the list applied in order on the task pool, while the image stays as it is. Each effect is a table with its name in `effect` and the options of its method:

| Effect | Keys |
| --- | --- |
| `{effect = 'blur'}` | `radius`, 0 by default. |
| `{effect = 'sharpen'}` | `amount`, 1 by default. |
| `{effect = 'adjust'}` | `brightness`, `contrast`, `saturation` and `hue`, with the defaults of `image:adjust`. |
| `{effect = 'grayscale'}` | `amount`, 1 by default. |
| `{effect = 'invert'}` | None. |
| `{effect = 'tint'}` | `color` and `amount`, 1 by default. |
| `{effect = 'pixelate'}` | `size`, 1 by default. |
| `{effect = 'outline'}` | `color` and `thickness`, 1 by default. |

An unknown name raises `The image effect "<name>" is unknown. It is "blur", "sharpen", "adjust", "grayscale", "invert", "tint", "pixelate" or "outline".`, and unknown keys raise `Unknown option "<key>".`. A value out of range rejects the promise with the message of its method.

```lua
local assets = require('haylen.assets')
local async = require('async')
local graphics = require('haylen.graphics')

-- A blurred and darkened copy of a picture behind a dialog, made on the task pool.
async.spawn(function()
    local picture = assets.loadAsync('images/harbor.png', 'image'):await()
    local backdrop = picture:applyAsync({{effect = 'blur', radius = 12}, {effect = 'adjust', brightness = -0.2}}):await()
    print(graphics.newTexture(backdrop, {filter = 'linear'}).width)
end)
```

### image:palette(count)

Returns up to `count` colors of the visible pixels, 8 by default and from 1 to 64, the most common first. Each entry is `{color, population}`, where `population` is the share of the pixels the color stands for. An image without visible pixels returns an empty list. The colors come from a median cut over a sample of at most 64 pixels per side, so a large image costs what a small one does. A count out of range raises `A palette has from 1 to 64 colors, not <count>.`.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local cover = graphics.decodeImage(assets.bytes('images/album.png'))
for _, entry in ipairs(cover:palette(4)) do
    print(entry.color:toHex(), entry.population)
end
```

### image:swatches(count)

Returns the colors that an interface takes from the image, picked from a palette of `count` colors, 16 by default. The table holds `dominant`, the most common color, and `vibrant`, `darkVibrant`, `lightVibrant`, `muted`, `darkMuted` and `lightMuted`, the colors closest to each target of lightness and saturation. Each one is absent when no color of the palette fits it.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local cover = graphics.decodeImage(assets.bytes('images/album.png'))
local swatches = cover:swatches()
local accent = swatches.vibrant or swatches.dominant
print(accent:toHex())
```

### image:placeholder(options)

Returns a short text that stands for the image while it loads, made of a few cosines of its colors on each axis. The options `columns` and `rows`, from 1 to 9, are 4 and 3 by default, and more of them keep more of the shapes in a longer text. The text holds only letters, digits, `-` and `_`, 36 characters with the defaults, so a server sends it next to the address of the image. The function [`graphics.decodePlaceholder`](#graphicsdecodeplaceholdercode-width-height) turns it back into an image. Options out of range raise `An image placeholder keeps from 1 to 9 cosines on each axis.`, unknown keys raise `Unknown option "<key>".`, and an image without pixels raises `An image placeholder needs an image with pixels.`.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local picture = graphics.decodeImage(assets.bytes('images/harbor.png'))
local code = picture:placeholder({columns = 5, rows = 4})
print(code, #code)
```

### Gradients

The method `image:fillGradient(gradient, area)` paints colors that blend from stop to stop, which a texture then shows. Use it for the background of a menu, the sky behind a level, a button or the [texture of a shape](graphics2d.md#graphics2ddrawshaperect-options), whose edge stays smooth at any size. The colors blend premultiplied by their alpha, so a stop that fades out never darkens its neighbours. The table `gradient` takes these keys:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `kind` | string | `'linear'` | The kind `'linear'` runs along `angle` across the whole area, from the corner behind its direction to the corner ahead of it. The kind `'radial'` grows from `center` to `radius`, and `'conic'` turns around `center` clockwise from `angle`. |
| `stops` | list | none | The colors as pairs of an offset from 0 to 1 and a color, in increasing order, such as `{{0, '#FF1E2A44'}, {0.6, '#FF4A78C8'}, {1, '#FFF6C26B'}}`. Two stops at one offset switch colors there. |
| `colors` | list of Color | none | The colors spread evenly from 0 to 1, in place of `stops`. |
| `angle` | number | `0` | The direction of a linear gradient and the start of a conic one, in radians clockwise from the x axis, so `math.pi / 2` runs from the top down. |
| `center` | Vec2 | `{0.5, 0.5}` | The center of a radial or conic gradient as a share of the area on each axis. |
| `radius` | number or Vec2 | `0.5` | The radius of a radial gradient as a share of the width and of the height of the area, so the default reaches the middle of every side. A number sets both. |
| `dither` | boolean | `true` | Spreads the rounding of every pixel to 8 bits with an ordered pattern, so wide gradients show no bands. |

The first color holds before the first stop, and the last one after the last stop. These errors come from a wrong gradient:

| Error | Cause |
| --- | --- |
| `A gradient takes its colors from either "stops" or "colors".` | The table has both keys, or neither. |
| `A gradient needs at least one color stop.` | The list of stops or colors is empty. |
| `The stops of a gradient are a list of offset and color pairs, such as {{0, '#FF000000'}, {1, '#FFFFFFFF'}}.` | A stop is not a pair. |
| `The stops of a gradient need offsets from 0 to 1 in increasing order.` | An offset is out of range or out of order. |
| `A radial gradient needs a finite radius above 0 on both axes.` | The radius is not above 0 or not finite. |
| `A gradient needs a finite angle.` | The angle is not finite. |
| `A gradient needs a center of finite coordinates.` | The center is not finite. |
| `Unknown option "<key>".` | The table has another key. |

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local sky = graphics.newImage(4, 256)
sky:fillGradient({angle = math.pi / 2, stops = {{0, '#FF1E2A44'}, {0.7, '#FF4A78C8'}, {1, '#FFF6C26B'}}})
local skyTexture = graphics.newTexture(sky, {filter = 'linear'})

local glow = graphics.newImage(128, 128)
glow:fillGradient({kind = 'radial', stops = {{0, '#FFFFE9A0'}, {1, '#00FFB040'}}})
local glowTexture = graphics.newTexture(glow, {filter = 'linear'})

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.draw(skyTexture, 0, 0, {width = 1280, height = 720, pivotX = 0, pivotY = 0})
        graphics2d.drawShape({480, 300, 320, 72}, {radius = 36, texture = skyTexture, borderWidth = 3, borderColor = '#FFFFFFFF'})
        graphics2d.drawShape({1000, 120, 160, 160}, {kind = 'ellipse', texture = glowTexture, blend = 'additive'})
    end,
})
```

## Font

A `Font` is a TrueType or OpenType font drawn through a signed distance field, or a bitmap font drawn from its own images, as the [text guide](../graphics/text.md#fonts) explains. Every function that takes a font takes either kind. The glyphs that the `COLR` table of a TrueType font paints, such as color emoji, draw in their own colors as [color glyphs](../graphics/text.md#color-emoji). A bitmap font draws pixel for pixel at its native size, scales at other sizes and draws nothing for the characters it lacks. Two font values compare equal with `==` when they refer to the same font, such as a face read twice from a family or a font loaded twice with the same options.

The function `assets.font(path, options)` of [`haylen.assets`](assets.md#assetsfontpath-options) loads either kind by the extension of the file:

- A `.ttf` or `.otf` file takes `bakeSize`, the em size of the glyphs in the atlas, 48 by default. It takes `spread`, how far the distance field reaches past a glyph in pixels at the bake size, 8 by default, which bounds outlines, glows and blurred shadows. It takes `atlasSize`, the first size of the atlas, 512 by default. It takes `face`, the index from 0 of the face of a collection, a `.ttc` file, 0 by default. Each one must be positive and at most the maximum texture size of the device, or it raises an error such as `The bake size of a TrueType font must be positive and at most the maximum texture size of 4096.`.
- A `.fnt` BMFont file takes the `filter` and `wrap` of its page textures.

The functions `graphics.newGridFont` and `graphics.newBitmapFont` make bitmap fonts from textures, and `graphics2d.defaultFont()` returns the font of the engine.

### graphics.newBitmapFont(data, pages)

Makes a bitmap `Font` from the contents of a BMFont `.fnt` file, in its text or binary format, and `pages`, a list with one texture for each of its pages, in order. The function `assets.font(path)` loads a `.fnt` file and its page images in one call, which suits most apps. A file that is not a BMFont raises an error that names the problem, such as `A BMFont file needs its "info" and "common" lines.`, and the [errors](#errors) list them all. A list with another number of textures raises `A bitmap font needs one texture for each of its <count> pages.`.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local pixel = graphics.newBitmapFont(assets.bytes('fonts/pixel.fnt'), {assets.texture('fonts/pixel-0.png', {filter = 'nearest'})})
print(pixel.nativeSize, pixel.pageCount)
```

### graphics.newGridFont(texture, options)

Makes a bitmap `Font` from an image of equal cells, read left to right and top to bottom. The table `options` takes these keys:

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `characters` | string | required | The characters of the cells in order, as UTF-8. |
| `cellWidth`, `cellHeight` | integer | required | The size of a cell in pixels, at least 1. |
| `spacing` | Vec2 | `{0, 0}` | The gap between cells, never negative. |
| `margin` | Vec2 | `{0, 0}` | The gap around the grid, never negative. |
| `advance` | number | `0` | The pen advance of every character, where 0 takes the width of a cell. |
| `lineHeight` | number | `0` | The height of a line, where 0 takes the height of a cell. |
| `baseline` | number | `0` | The baseline below the top of a cell, where 0 takes the height of a cell. |

Cells smaller than a pixel raise `A grid font needs cells at least one pixel wide and tall.`, a negative gap raises `A grid font cannot have a negative spacing or margin.`, and no characters raise `A grid font needs the characters of its cells.`. An image with fewer cells than characters, such as one whose margins leave no room, raises an error such as `The grid font image holds 0 cells, fewer than its 10 characters.`.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local digits = graphics.newGridFont(assets.texture('fonts/digits.png', {filter = 'nearest'}), {characters = '0123456789', cellWidth = 12, cellHeight = 16, baseline = 14})
```

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `nativeSize` | number | read | The size the glyph images were made for: the bake size of a TrueType font or the size of a bitmap font. |
| `distanceField` | boolean | read | The value is `true` for TrueType fonts, whose text takes outlines, glows, blurs and synthetic styles. |
| `pageCount` | integer | read | The number of textures that hold the glyphs. |

```lua
local assets = require('haylen.assets')

local pixel = assets.font('fonts/pixel.fnt', {filter = 'nearest'})
print(pixel.nativeSize, pixel.distanceField, pixel.pageCount)
```

### font:measure(text, style)

Returns the width and height of `text` shaped and laid out with `style`, stretched by its `scale`, which is the block that `graphics2d.drawText` covers. The style takes the text keys of [`graphics2d.drawText`](graphics2d.md#text-style) without its draw order keys, which raise `Unknown option "<key>".` because a font draws nothing itself. A `size`, `maxWidth`, `maxHeight`, `minScale` or `lineSpacing` that is not a finite number raises `Text needs a finite size, maximum width, maximum height, minimum scale and line spacing.`.

```lua
local graphics2d = require('haylen.graphics2d')

local width, height = graphics2d.defaultFont():measure('Hello', {size = 32})
print(width, height)
```

### font:layout(text, style)

Lays `text` out with the layout keys of `style`, like `graphics2d.drawText` does, where `style` takes the same keys as `font:measure`. It returns a table with these fields. Positions are relative to the anchor point of the text block, before rotation and scale.

| Field | Type | Meaning |
| --- | --- | --- |
| `quads` | table | One entry per visible glyph in the order the lines show them, each with `position` (`Vec2`, the top-left corner), `size` (`Vec2`), `source` (`Rect` in the page), `page` (counted from 1) and `font`, the font that draws it. A color glyph also has `image`, the `VectorImage` of its colors, which its quad shows in place of a region of a page. Spaces and line breaks have no entry. A shaped cluster, such as a ligature or a letter with its marks, has one entry per glyph the font draws for it. |
| `size` | Vec2 | Width and height of the whole block, as `font:measure` returns them. |
| `lineCount` | integer | Number of lines after wrapping. |

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local font = graphics2d.defaultFont()
local layout = font:layout('Press start', {size = 48, anchor = {0.5, 0.5}})

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(font, 'Press start', 960, 540, {size = 48, anchor = {0.5, 0.5}})
        for _, quad in ipairs(layout.quads) do
            graphics2d.drawRectOutline({960 + quad.position.x, 540 + quad.position.y, quad.size.x, quad.size.y}, 1, '#80FF4040')
        end
    end,
})
```

### font:hasGlyph(character)

Returns whether the font has a glyph for `character`, given as a string of one character or as a code point. A longer string raises a `bad argument` error that ends with `expected one character or a code point`, and a number outside the code points of Unicode raises one that ends with `expected a code point from 0 to 0x10FFFF`.

```lua
local graphics2d = require('haylen.graphics2d')

local font = graphics2d.defaultFont()
print(font:hasGlyph('A'), font:hasGlyph(0x4E16))
```

### font:hasColorGlyph(character)

Returns whether the glyph of `character` draws in its own colors, such as an emoji of a color font, rather than in the color of the text. The `character` is a string of one character or a code point, as for `font:hasGlyph`. A bitmap font returns `false`, since the color of the text tints its images.

```lua
local assets = require('haylen.assets')

local emoji = assets.font('fonts/noto-color-emoji-regular.ttf')
print(emoji:hasColorGlyph('\u{1F600}'), emoji:hasColorGlyph(0x2600))
```

### font:glyph(character)

Returns the glyph that `character` shapes to on its own, at the native size, as a table:

| Field | Meaning |
| --- | --- |
| `index` | The glyph index in the font, 0 for a missing glyph. |
| `source` | The `Rect` of the glyph in its page. A color glyph lies in no page, and its source is the box of its image at the origin. |
| `offset` | The `Vec2` from the pen on the baseline to the top-left corner of the image. |
| `advance` | How far the pen moves after the glyph. |
| `page` | The page that holds the glyph, counted from 1. |
| `visible` | Whether the glyph draws anything. |
| `ink` | The `Rect` that the outline of the glyph, or the paint of a color glyph, covers from the pen on the baseline, with y pointing down, which a [natural line](../graphics/text.md#line-heights) grows to hold. It is empty for a glyph without an outline, such as a space, and for every glyph of a bitmap font. |
| `image` | The `VectorImage` of the colors of a color glyph, as large as its box at the native size, and `nil` for every other glyph. It draws at any size with `graphics2d.drawVector`. |

```lua
local graphics2d = require('haylen.graphics2d')

local glyph = graphics2d.defaultFont():glyph('g')
print(glyph.index, glyph.advance, glyph.offset.y, glyph.source.height, glyph.ink.y)
```

### font:glyphByIndex(index)

Returns the glyph with an index that `font:shape` gave, as the same table as `font:glyph`. This is how an app draws shaped text from the atlas itself. The index of a glyph of a bitmap font is its code point. A negative index raises a `bad argument` error that ends with `expected a glyph index`. A TrueType font raises `The font has no glyph with index <index>.` for an index it does not have. A glyph too large for any atlas the device allows raises `The glyph is larger than the largest font atlas the device can hold.`, and a full atlas that cannot grow raises `The font atlas exceeded the maximum texture size.` every time it is used.

```lua
local graphics2d = require('haylen.graphics2d')

local font = graphics2d.defaultFont()
for _, shaped in ipairs(font:shape('office', {size = 48})) do
    local glyph = font:glyphByIndex(shaped.index)
    print(shaped.cluster, glyph.source.width, glyph.page)
end
```

### font:shape(text, options)

Shapes `text` as one run and returns its glyphs in visual order. Each glyph is a table with these fields:

| Field | Meaning |
| --- | --- |
| `index` | The glyph index in the font, which `font:glyphByIndex` reads. |
| `cluster` | The position of the first character of its cluster in the text, counted in code points from 1. |
| `advance` | How far the pen moves after the glyph, at the size of the options. |
| `offset` | The `Vec2` from the pen, where positive y points down. |

A TrueType font shapes with HarfBuzz in the script of the first letter. So the glyphs show ligatures, the joining forms of Arabic, the conjuncts of Indic scripts, kerning and the marks placed on their letters. A bitmap font maps every character to its glyph and applies its kerning pairs. The functions `font:layout` and the draw functions split mixed text into runs and shape each one themselves.

| Option | Default | Meaning |
| --- | --- | --- |
| `size` | the native size | The text size of the lengths. |
| `direction` | `'auto'` | The value `'auto'` reads the direction of the first strong letter of the first paragraph, and `'leftToRight'` and `'rightToLeft'` force it. |
| `language` | `''` | A BCP 47 tag such as `'fa'` or `'sr'`, which picks the forms a language prefers. |

Unknown keys raise `Unknown option "<key>".`.

```lua
local assets = require('haylen.assets')

local arabic = assets.font('fonts/noto-sans-arabic-regular.ttf')
for _, glyph in ipairs(arabic:shape('لا سلام', {size = 32, language = 'ar'})) do
    print(glyph.index, glyph.cluster, glyph.advance)
end
print(#assets.font('fonts/serif.ttf'):shape('office'))
```

### font:page(index)

Returns the texture of a page, counted from 1, with the glyphs used so far uploaded. An index out of range raises a `bad argument` error that ends with `page out of range`. The atlas of a TrueType font becomes a new, larger texture when it grows. So a page read earlier keeps only the glyphs of its time, and an app that draws from the page reads it again after using new glyphs.

```lua
local graphics2d = require('haylen.graphics2d')

local atlas = graphics2d.defaultFont():page(1)
print(atlas.width, atlas.height)
```

### font:toDistance(pixels, size)

Converts a length of `pixels` at a text `size` to the distance field units of the text shader, where 0.5 spans the spread of the field. This shows how far an outline or a glow can reach. Bitmap fonts return 0.

```lua
local graphics2d = require('haylen.graphics2d')

print(graphics2d.defaultFont():toDistance(3, 32))
```

### font:lineHeight(size)

Returns the height of one line of text at `size`.

```lua
local assets = require('haylen.assets')

local font = assets.font('fonts/title.ttf', {bakeSize = 64})
print(font:lineHeight(40))
```

### font:ascent(size)

Returns the distance from the top of a line to its baseline at `size`, which lines text up with other drawings.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local font = graphics2d.defaultFont()

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(font, 'Baseline', 100, 100, {size = 40})
        graphics2d.drawLine(100, 100 + font:ascent(40), 400, 100 + font:ascent(40), 1, '#FFFF4040')
    end,
})
```

### font:capHeight(size)

Returns how high the capital H of the font reaches above the baseline at `size`. A font without a capital H returns its ascent. Center a word of capitals or a number on a button or a badge by its cap height. The ascent keeps room for accents, so it would put the word too low.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local font = graphics2d.defaultFont()

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawRect({100, 100, 300, 80}, '#FF2C3147')
        local baseline = 140 + font:capHeight(48) / 2
        graphics2d.drawText(font, 'SCORE', 250, baseline - font:ascent(48), {size = 48, anchor = {0.5, 0}})
    end,
})
```

## FontFamily

A `FontFamily` is the set of faces of one typeface, with fallback fonts for the characters its faces lack, as the [text guide](../graphics/text.md#font-families) explains. Rich text draws with a family, and `graphics2d.drawText` and `graphics2d.measureText` take one in place of a font. Two family values compare equal with `==` when they refer to the same family.

### graphics.newFontFamily(faces)

Makes a `FontFamily` from the table `faces`:

| Key | Type | Meaning |
| --- | --- | --- |
| `regular` | Font | The regular face, which every family needs. |
| `bold`, `italic`, `boldItalic`, `mono` | Font | The other faces, each one optional. |
| `fallbacks` | list of Font | Fonts for the characters a face lacks, such as a CJK, Arabic, Devanagari or symbol font, tried in order. |
| `systemFonts` | boolean | Draws what none of its fonts holds with the fonts of the system, `true` by default, as [fonts of the system](../graphics/text.md#fonts-of-the-system) explains. The value `false` draws the missing glyph of the face instead, which keeps the look of the text the same on every device. |

A family picks the font of every character with its marks as one unit, so a letter and its marks always come from one font. Each run of one font is shaped on its own. Rich text draws `[b]` and `[i]` with the real faces the family has and synthesizes the others. A TrueType face grows its strokes and leans its glyphs through its distance field. A bitmap face draws a bold glyph twice a native pixel apart and leans italic ones. The tag `[code]` uses the mono face, or the regular faces when the family has none.

A family without `regular` raises `A font family needs a regular face.`, a fallback that is not a font raises `A font family fallback must be a font.`, and unknown keys raise `Unknown option "<key>".`.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local serif = graphics.newFontFamily({
    regular = assets.font('fonts/serif.ttf'),
    bold = assets.font('fonts/serif-bold.ttf'),
    mono = assets.font('fonts/mono.ttf'),
    fallbacks = {assets.font('fonts/cjk.ttf'), assets.font('fonts/symbols.ttf')},
})
```

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `regular` | Font | read | The regular face. |
| `bold`, `italic`, `boldItalic`, `mono` | Font or `nil` | read | The other faces, `nil` for the faces the family lacks. |
| `fallbacks` | list of Font | read | The fallback fonts. |
| `systemFonts` | boolean | read | Whether the family draws what none of its fonts holds with the fonts of the system. |

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local family = graphics.newFontFamily({regular = graphics2d.defaultFont()})
print(family.regular.nativeSize, family.bold, family.italic, family.boldItalic, family.mono, #family.fallbacks)
```

### family:select(style)

Returns the face that draws a style, followed by whether bold and whether italic are synthesized because the family lacks the real face, and `false`, since a face of the family never waits for the fonts of the system. The table `style` takes `bold`, `italic` and `mono`.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local family = graphics.newFontFamily({regular = graphics2d.defaultFont()})
local face, syntheticBold, syntheticItalic = family:select({bold = true})
print(face == family.regular, syntheticBold, syntheticItalic)
```

### family:resolve(character, style)

Returns the font that draws `character` in a style, followed by whether bold and italic are synthesized and whether a font of the system that draws it is still loading. The font is the first of these that fits:

- For a character that shows as an emoji, by default or through U+FE0F, the first of the fonts below that draws it as a color glyph, and for one that U+FE0E asks to show as text, the first that draws it in the color of the text.
- The selected face, when it has a glyph for every code point of the character.
- The first fallback that has them, which synthesizes the styles asked for.
- The first font of the system that arrived and has them, in a family that uses them.
- The selected face or the first fallback that has the base letter of the character, without its marks.
- The selected face again, which draws its missing glyph. A family that uses the fonts of the system asks them for the character then, once, and the fourth value is `true` until the answer comes. The character is a code point or a string, which may hold a letter with its marks. The table `style` takes `bold`, `italic` and `mono`, and an empty string raises a `bad argument` error that ends with `expected a character`.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local family = graphics.newFontFamily({regular = graphics2d.defaultFont(), fallbacks = {assets.font('fonts/cjk.ttf')}})
local font, syntheticBold = family:resolve('世', {bold = true})
print(font == family.fallbacks[1], syntheticBold)
```

### graphics.systemFonts()

Returns the names of the faces of the system that families loaded so far, in the order they arrived, such as `'PingFangSC-Regular'` on Apple platforms, where a face is named by its PostScript name, or `'/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc#2'` elsewhere, where it is named by its file and its index in the file. The list is empty until a family asks for a character none of its fonts holds, and on the web, which has no fonts of the system, as [fonts of the system](../graphics/text.md#fonts-of-the-system) explains.

```lua
local graphics = require('haylen.graphics')

for _, name in ipairs(graphics.systemFonts()) do
    print(name)
end
```

### family:measure(text, style)

Returns the width and height of `text` laid out in the family with `style`, like `font:measure` does. The style takes the text keys of `graphics2d.drawText`, where `bold` and `italic` pick the faces. Every character that the selected face lacks comes from the first fallback that has it.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local family = graphics.newFontFamily({regular = graphics2d.defaultFont(), fallbacks = {assets.font('fonts/noto-sans-hebrew-regular.ttf')}})
print(family:measure('Shalom שלום', {size = 32}))
```

### family:layout(text, style)

Lays `text` out in the family and returns the same table as `font:layout`, where the `font` of every quad is the face or fallback that draws it.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local family = graphics.newFontFamily({regular = graphics2d.defaultFont(), fallbacks = {assets.font('fonts/noto-sans-devanagari-regular.ttf')}})
local layout = family:layout('Hindi हिन्दी', {size = 32})
for _, quad in ipairs(layout.quads) do
    print(quad.font == family.regular, quad.position.x)
end
```

## Shader

A `Shader` is a custom shader that `haylen app shaders` compiled from the Haylen shader language for every shader language the engine runs. Load it from a `.shader` file with `assets.shader(path)` or `assets.load(path)`. [Materials](graphics2d.md#material) draw with it, and the [shader guide](../graphics/shaders.md) explains how to write one. The engine creates the GPU program for the shader language of the GPU device the first time a draw needs it. Two shaders compare equal with `==` when they are the same loaded shader.

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `name` | string | read | The name of the program in the source. |
| `uniforms` | table | read | The uniforms of the own blocks of the shader that materials set, in order, each as `{name = 'time', type = 'float', count = 1}`. The type is a GLSL type, and the count is the number of elements of an array and 1 otherwise. |
| `textures` | table | read | The names of the own textures of the shader that materials set. |

```lua
local assets = require('haylen.assets')

local shader = assets.shader('shaders/ripple.shader')
print(shader.name)
for _, uniform in ipairs(shader.uniforms) do
    print(uniform.name, uniform.type, uniform.count)
end
print(table.concat(shader.textures, ', '))
```

## VectorImage

A `VectorImage` is an SVG document read into curves, or the colors of a color glyph of a font, which draws sharp at any size with [`graphics2d.drawVector`](graphics2d.md#graphics2ddrawvectorimage-x-y-options). Load one from a `.svg` file with `assets.vectorImage(path)` or `assets.load(path)`, or make one with `graphics.newVectorImage`, and take the image of a color glyph from the `image` of [`font:glyph`](#fontglyphcharacter). The [rendering guide](../graphics/rendering.md#vector-images) lists the parts of SVG that the engine reads and explains how it rasterizes them. Two images compare equal with `==` when they are the same image. A file that changes on disk loads anew the next time the app asks for it, and the images the app holds keep their document.

### graphics.newVectorImage(text)

Reads `text`, the text of an SVG document, into a `VectorImage`, such as a document that an app builds or downloads. Images of the package load with [`assets.vectorImage`](assets.md#assetsvectorimagepath), which reads them on a worker thread and caches them. Text that is not an SVG document with a size raises `The bytes are not an SVG document with a size. Give its root element a view box, or a width and a height.`.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local badge = graphics.newVectorImage([[<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><circle cx="12" cy="12" r="10" fill="currentColor"/></svg>]])

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawVector(badge, 100, 100, {width = 48, height = 48, color = '#FF40C060'})
    end,
})
```

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `width` | number | read | The width the document gives itself: its view box, its width, or the extent of what it draws. |
| `height` | number | read | The height, found the same way. |

```lua
local assets = require('haylen.assets')

local star = assets.vectorImage('icons/star.svg')
print(star.width, star.height, star == assets.load('icons/star.svg'))
```

### vectorImage:rasterize(scale, options)

Rasterizes the whole image at `scale`, in pixels per unit of the image, on a worker thread. It returns a promise of a `Texture` of `ceil(width * scale)` by `ceil(height * scale)` pixels, for draws that need a texture of the image, such as a material or a sprite batch. The UI draws `.svg` files itself, as its [image values](ui.md#value-types) describe. The table `options` takes `filter`, `wrap` and `mipmaps` like `graphics.newTexture`, with `filter = 'linear'` by default.

A scale that is not positive, or whose raster is larger than [`graphics.maxTextureSize()`](#graphicsmaxtexturesize), raises `A vector image rasterizes at a positive scale whose raster fits the maximum texture size of the device.`. A raster whose texture finds no room on the device, such as when the texture pool is full, makes `:await()` return `nil` and the message of the device.

```lua
local assets = require('haylen.assets')
local async = require('async')

local gear = assets.vectorImage('icons/gear.svg')
async.spawn(function()
    local texture = gear:rasterize(4):await()
    print(texture.width, texture.height)
end)
```

## Errors

These messages come from the functions and methods of the module.

| Message | Cause |
| --- | --- |
| `Texture dimensions must be positive.` | A texture or render target has a size of 0 or less. |
| `Texture dimensions exceed the device limit.` | A texture or render target is larger than `graphics.maxTextureSize()`. |
| `Image dimensions cannot be negative.` | The size of `graphics.newTexture` with `pixels` is negative. |
| `Image pixel data does not match its dimensions.` | The `pixels` of `graphics.newTexture` have another length than `width * height * 4`. |
| `A texture that changes in place has no mipmaps.` | A dynamic texture asked for mipmaps. |
| `A render target has no mipmaps.` | A render target asked for mipmaps. |
| `Only a dynamic texture changes its pixels in place.` | The method `texture:update` ran on a texture that is not dynamic. |
| `The pixels do not match the size of the dynamic texture.` | The pixels of `texture:update` have another length. |
| `The region of a texture update must lie inside the dynamic texture and hold at least one pixel.` | The option `region` of `texture:update` reaches past the texture or has no width or height. |
| `The region of "texture:update" must hold whole pixels.` | The option `region` of `texture:update` has a fraction of a pixel. |
| `The image is damaged, or it is not a PNG, JPEG, WebP, TGA, BMP or GIF file.` | The bytes of `graphics.newTexture`, `graphics.decodeImage` or `graphics.decodeImageAsync` are not an image. |
| `An image needs a width and a height of one pixel or more.` | The size of `graphics.newImage` is under one pixel. |
| `The image of <width> by <height> pixels is too large. An image has at most 16384 pixels on a side.` | A side of `graphics.newImage` is longer than `graphics.maxImageSize()`. |
| `The image of <width> by <height> pixels is too large to decode. The image may have at most <maxSize> pixels on a side.` | The header of the bytes of `graphics.decodeImage` or `graphics.decodeImageAsync` declares a side longer than the option `maxSize`. |
| `The image could not be decoded, because its data holds more than an image of <width> by <height> pixels.` | The compressed data of the bytes of `graphics.decodeImage` or `graphics.decodeImageAsync` inflates past the size that their header declares, so the file is damaged. |
| `The image of <width> by <height> pixels has no pixels to decode.` | The header of the bytes of `graphics.decodeImage` or `graphics.decodeImageAsync` declares a side of no pixels. |
| `The image of <count> bytes is too large to decode. An encoded image holds at most 2147483647 bytes.` | The bytes of `graphics.decodeImage` or `graphics.decodeImageAsync` are longer than a decoder reads. |
| `The option "maxSize" of a decode is from 1 to 16384 pixels.` | The option `maxSize` of `graphics.decodeImage` or `graphics.decodeImageAsync` is out of range. |
| `A read needs a valid render target.` | The target of `graphics.readTarget` is not valid. |
| `The area <x>, <y>, <w> by <h> is not inside the image of <w> by <h> pixels.` | The area of `image:crop` lies outside the image. |
| `The corner and the size of an area of an image are numbers of pixels from -1073741824 to 1073741824.` | An area of `image:fill`, `image:fillGradient`, `image:blit`, `image:crop` or another method of an image is not finite or lies farther away. |
| `An empty image has no PNG file.` | The image of `image:encodePng` has no pixels. |
| `A blur has a radius from 0 to 64 pixels.` | The radius of `image:blur` is out of range. |
| `A sharpening has an amount of 0 or more.` | The amount of `image:sharpen` is negative or not finite. |
| `An adjustment of the colors has a finite brightness, contrast, saturation and hue.` | An option of `image:adjust` is not finite. |
| `A grayscale has a finite amount.` | The amount of `image:grayscale` is not finite. |
| `A tint has a finite amount and a color of finite channels.` | The amount or the color of `image:tint` is not finite. |
| `An outline has a color of finite channels.` | The color of `image:outline` is not finite. |
| `A pixelation has squares of 1 pixel or more.` | The size of `image:pixelate` is below 1. |
| `An outline has a thickness from 0 to 32 pixels.` | The thickness of `image:outline` is out of range. |
| `The image effect "<name>" is unknown. It is "blur", "sharpen", "adjust", "grayscale", "invert", "tint", "pixelate" or "outline".` | The method `image:applyAsync` got another effect. |
| `A palette has from 1 to 64 colors, not <count>.` | The count of `image:palette` or `image:swatches` is out of range. |
| `An image placeholder keeps from 1 to 9 cosines on each axis.` | The options of `image:placeholder` are out of range. |
| `An image placeholder needs an image with pixels.` | The image of `image:placeholder` has no pixels. |
| `The text is not an image placeholder.` | The text of `graphics.decodePlaceholder` is not a placeholder. |
| `An image placeholder decodes to a size from 1 to 512 pixels on each side.` | The size of `graphics.decodePlaceholder` is out of range. |
| `A font family needs a regular face.` | The faces of `graphics.newFontFamily` lack `regular`. |
| `A font family fallback must be a font.` | A fallback of `graphics.newFontFamily` is not a font. |
| `A bitmap font needs one texture for each of its <count> pages.` | The pages of `graphics.newBitmapFont` have another count. |
| `A bitmap font page needs a texture.` | A page of `graphics.newBitmapFont` is not a texture. |
| `A bitmap font needs a positive size and line height.` | The BMFont file has no positive size or line height. |
| `A bitmap font character lies on page <page>, which the font does not have.` | A character of the BMFont file names a page it lacks. |
| `A BMFont file needs its "info" and "common" lines.` | The text file lacks these lines. |
| `A BMFont file needs its "info" and "common" blocks.` | The binary file lacks these blocks. |
| `A BMFont file must name the image of every page.` | A page of the file has no image. |
| `BMFont line <line> has a quote that never ends.` | A quoted value of the text file never closes. |
| `BMFont line <line> needs a whole number for "<key>".` | A value of the text file is not a whole number. |
| `BMFont line <line> needs a page count from 0 to 65535.` | The page count of the text file is out of range. |
| `BMFont line <line> names a page the "common" line does not count, or no file.` | A page line of the text file is wrong. |
| `BMFont line <line> needs a page from 0 to 65535.` | The page of a character of the text file is out of range. |
| `Only version 3 of the binary BMFont format is supported.` | The binary file has another version. |
| `The binary BMFont file ends in the middle of a block.` | The binary file is cut short. |
| `The binary BMFont file has a block of unknown type <type>.` | The binary file holds a block it should not. |
| `The "<block>" block of the binary BMFont file is shorter than its <size> bytes of fields.` | The `info` block of the binary file holds fewer than 14 bytes, or its `common` block fewer than 15. |
| `A grid font needs cells at least one pixel wide and tall.` | The cells of `graphics.newGridFont` are too small. |
| `A grid font cannot have a negative spacing or margin.` | A gap of `graphics.newGridFont` is negative. |
| `A grid font needs the characters of its cells.` | The `characters` of `graphics.newGridFont` are empty. |
| `The grid font image holds <cells> cells, fewer than its <count> characters.` | The image of `graphics.newGridFont` has too few cells. |
| `Text needs a finite size, maximum width, maximum height, minimum scale and line spacing.` | A style of `font:measure`, `font:layout`, `family:measure` or `family:layout` has a value that is not finite. |
| `The font has no glyph with index <index>.` | The index of `font:glyphByIndex` is past the glyphs of a TrueType font. |
| `The glyph is larger than the largest font atlas the device can hold.` | A glyph cannot fit any atlas. |
| `The font atlas exceeded the maximum texture size.` | The atlas of a TrueType font is full and cannot grow. |
| `The bytes are not an SVG document with a size. Give its root element a view box, or a width and a height.` | The text of `graphics.newVectorImage` is not an SVG document with a size. |
| `A vector image rasterizes at a positive scale whose raster fits the maximum texture size of the device.` | The scale of `vectorImage:rasterize` is not positive or too large. |
| `Unknown option "<key>".` | An options table has a key that the function does not take. |
