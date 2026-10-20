# haylen.graphics

The module `haylen.graphics` creates the GPU resources that every kind of drawing shares, textures, render targets, bitmap fonts, font families and vector images, and names the GPU backend and its texture size limit. It also describes the `Texture`, `RenderTarget`, `Font`, `FontFamily`, `Shader` and `VectorImage` values the rest of the engine passes around. Drawing in 2D, and the materials that draw with shaders, live in [`haylen.graphics2d`](graphics2d.md).

```lua
local graphics = require('haylen.graphics')
```

## Functions

### graphics.newRenderTarget(width, height, options)

Creates a `RenderTarget`, an offscreen texture that `graphics2d.beginTarget` draws into. The table `options` accepts `filter` (`'nearest'` or `'linear'`, default `'nearest'`) and `wrap` (`'clamp'`, `'repeat'` or `'mirror'`, default `'clamp'`). Sizes of 0 or less raise `Texture dimensions must be positive.` and sizes above [`graphics.maxTextureSize()`](#graphicsmaxtexturesize) raise `Texture dimensions exceed the device limit.`

```lua
local graphics = require('haylen.graphics')

local target = graphics.newRenderTarget(320, 180, {filter = 'linear'})
print(target.width, target.height, target.texture.width)
```

### graphics.newTexture(width, height, options)

Creates a `Texture` from code. The table `options` is optional:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `fill` | Color | `'#FFFFFFFF'` | Color of every pixel when `pixels` is absent. |
| `pixels` | string | none | Raw RGBA bytes, four per pixel, row by row from the top. Its length must be `width * height * 4`, otherwise the call raises `Image pixel data does not match its dimensions.` |
| `dynamic` | boolean | `false` | Makes a texture whose pixels change in place with [`texture:update(pixels)`](#textureupdatepixels), such as a minimap drawn by code. |
| `filter` | string | `'nearest'` | The `'nearest'` or `'linear'` sampling. |
| `wrap` | string | `'clamp'` | The `'clamp'`, `'repeat'` or `'mirror'` addressing. |
| `mipmaps` | boolean | `false` | Keeps smaller copies of the image down to one pixel, which a texture drawn smaller than its size samples, blended between them with the `'linear'` filter. A dynamic texture raises `A texture that changes in place has no mipmaps.` |

Sizes of 0 or less raise `Texture dimensions must be positive.` and sizes above [`graphics.maxTextureSize()`](#graphicsmaxtexturesize) raise `Texture dimensions exceed the device limit.`, and a filled texture checks its size before it makes any pixel.

```lua
local graphics = require('haylen.graphics')

local red = graphics.newTexture(4, 4, {fill = '#FFFF0000'})
local checker = graphics.newTexture(2, 2, {
    pixels = string.char(255, 255, 255, 255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255, 255),
    wrap = 'repeat',
})
print(red.width, checker.height)
```

### graphics.newTexture(bytes, options)

Creates a `Texture` from `bytes`, a string with the contents of a PNG, JPEG, TGA, BMP or GIF file, such as the image a plugin returns from native code or a file the app downloaded. The texture has the size of the image, and `options` takes `dynamic`, `filter` and `wrap` of the form above, while `fill` and `pixels` raise an unknown option error, since the image gives the pixels. Bytes that are no image raise `The image could not be decoded, and the decoder reported "<reason>".`. Raw RGBA pixels take the form with the width and the height and the `pixels` option, and images of the package load with [`assets.texture`](assets.md), which caches them.

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

### graphics.newFontFamily(faces)

Creates a `FontFamily` from its faces: `regular`, which it needs, and the optional `bold`, `italic`, `boldItalic` and `mono` faces, plus `fallbacks`, a list of fonts for the characters a face lacks, such as a CJK, Arabic, Devanagari or symbol font. A family picks the font of every character with its marks as one unit, so a letter and its marks always come from one font, and each run of one font is shaped on its own. The functions `graphics2d.drawText` and `graphics2d.measureText` take a family in place of a font to draw plain text in every script its fonts cover. Rich text draws `[b]` and `[i]` with the real faces the family has and synthesizes the others: a TrueType face grows its strokes and leans its glyphs through its distance field, and a bitmap face draws a bold glyph twice a native pixel apart and leans italic ones. The tag `[code]` uses the mono face, or the regular faces when the family has none. A family without `regular` raises `A font family needs a regular face.`.

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

### graphics.newBitmapFont(data, pages)

Creates a bitmap `Font` from the contents of a BMFont `.fnt` file, in its text or binary format, and one texture for each of its pages, in order. The function `assets.font(path)` loads a `.fnt` file and its page images in one call, which suits most apps. A file that is not a BMFont raises an error that names the problem, such as `A BMFont file needs its "info" and "common" lines.`.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local pixel = graphics.newBitmapFont(assets.bytes('fonts/pixel.fnt'), {assets.texture('fonts/pixel-0.png', {filter = 'nearest'})})
```

### graphics.newGridFont(texture, options)

Creates a bitmap `Font` from an image of equal cells, read left to right and top to bottom. The table `options` takes `characters` (the characters of the cells in order, as UTF-8), `cellWidth` and `cellHeight` (the size of a cell in pixels, at least 1), and the optional `spacing` and `margin` (`Vec2` gaps between cells and around the grid, never negative), `advance` (the pen advance of every character), `lineHeight` and `baseline` (the baseline below the top of a cell), where 0 takes the size of the cell. An image with fewer cells than characters, such as one whose margins leave no room, raises an error such as `The grid font image holds 0 cells, fewer than its 10 characters.`.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local digits = graphics.newGridFont(assets.texture('fonts/digits.png', {filter = 'nearest'}), {characters = '0123456789', cellWidth = 12, cellHeight = 16, baseline = 14})
```

### graphics.newVectorImage(text)

Reads the text of an SVG document into a [`VectorImage`](#vectorimage), such as a document an app builds or downloads. Images of the package load with [`assets.vectorImage`](assets.md), which reads them on a worker thread and caches them. Text that is not an SVG document with a size raises `The bytes are not an SVG document with a size. Give its root element a view box, or a width and a height.`

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

### graphics.whiteTexture()

Returns the engine's 1 by 1 white `Texture`, handy for tinted quads in sprites and batches.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local pixel = graphics2d.newSprite(graphics.whiteTexture(), {width = 8, height = 8, color = '#FF40FF80'})
```

### graphics.backendName()

Returns the name of the GPU backend: `'glcore'`, `'gles3'`, `'d3d11'`, `'metal'`, `'webgpu'` or `'dummy'` in headless tests.

```lua
local graphics = require('haylen.graphics')
local log = require('haylen.log')

log.info('Rendering with ' .. graphics.backendName())
```

### graphics.maxTextureSize()

Returns the largest width and height in pixels that a texture or render target can have on the GPU, which depends on the device. Larger sizes raise `Texture dimensions exceed the device limit.`

```lua
local graphics = require('haylen.graphics')

local size = math.min(4096, graphics.maxTextureSize())
local atlas = graphics.newRenderTarget(size, size)
print(atlas.width, graphics.maxTextureSize())
```

### graphics.newTexture(image, options)

Creates a `Texture` from an [`Image`](#image), with the options `filter`, `wrap` and `mipmaps` of `graphics.newTexture`. Unlike textures loaded from files, it takes the pixels as they are, so call `image:bleed()` first for a `'linear'` texture whose transparent edges should not darken.

### graphics.newImage(width, height, fill)

Creates an [`Image`](#image) of the size, every pixel `fill`, transparent by default. A size under one pixel raises `An image needs a width and a height of one pixel or more.`

### graphics.decodeImage(bytes) and graphics.decodeImageAsync(bytes)

Decodes the bytes of a PNG, JPG, TGA, BMP or GIF file into an [`Image`](#image), on the frame thread, or on the task pool with a promise of the image. The `image` asset type of [`haylen.assets`](assets.md) loads images of the package the same way, with `assets.loadAsync(path, 'image')` off the frame thread. Bytes that are no image raise `The image could not be decoded, and the decoder reported "reason".`

### graphics.decodePlaceholder(code, width, height)

Decodes a text of `image:placeholder` into a blurred [`Image`](#image) of `width` by `height` pixels, from 1 to 512 on each side, which a texture shows scaled up with the `'linear'` filter until the real image arrives. A placeholder may come from anywhere, so a text that is not one raises `The text is not an image placeholder.`, and a size out of range raises `An image placeholder decodes to a size from 1 to 512 pixels on each side.`

```lua
local assets = require('haylen.assets')
local async = require('async')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local http = require('http')

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

function render()
    graphics2d.draw(cover, 40, 40, {width = 320, height = 240})
end
```

### graphics.readTarget(target) and graphics.captureScreen()

Return a promise of an [`Image`](#image) of the pixels of the render target `target` as the current frame renders it, or of everything the screen shows of the next frame the app renders, the interface and the debug overlay included. The GPU copies the pixels after the frame is submitted without the frame thread waiting for it, where the backend allows, as the [rendering guide](../graphics/rendering.md#reading-pixels-back) describes, and the task pool turns them into straight colors with the top row first, so the promise settles a frame or more later. An image saves as a PNG file with `image:encodePngAsync()` and [`storage.writeTextAsync`](storage.md).

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

## Image

An `Image` holds RGBA pixels on the CPU, for art that code makes, composes or packs before it becomes a texture, such as generated portraits, decals stamped into a map or the images of a [runtime atlas](animation2d.md#atlasbuilder). Areas are rectangles of whole pixels, `{x, y, width, height}` or a `Rect`. An image that work on the task pool still reads is copied before Lua changes it, so `blitAsync` and `builder:buildAsync` never see a later change and Lua never waits for them, and images of the asset cache stay as they were loaded.

| Property | Type | Meaning |
| --- | --- | --- |
| `width`, `height` | integer | Size in pixels, read-only. |

| Method | Meaning |
| --- | --- |
| `image:pixel(x, y)` | Returns the `Color` of a pixel, transparent outside the image. |
| `image:setPixel(x, y, color)` | Sets a pixel, and does nothing outside the image. |
| `image:fill(color, area)` | Fills the part of the area inside the image, the whole image without an area. |
| `image:fillGradient(gradient, area)` | Paints a [gradient](#gradients) laid over the whole area into the part of it inside the image, the whole image without an area. |
| `image:blit(source, x, y, options)` | Copies the image `source`, or the area `options.source` of it, to `x`, `y`, clipped to both images, replacing the pixels it covers, or blending over them by alpha with `options.blend = true`. Other keys raise `Unknown option "name".` |
| `image:blitAsync(source, x, y, options)` | Returns a promise of a copy of the image with the blit done, both made on the task pool, while the image stays as it is. |
| `image:crop(area)` | Returns a new image of the area, which raises `The area x, y, w by h is not inside the image of w by h pixels.` outside it. |
| `image:bleed()` | Gives the transparent pixels around what shows the color of the nearest pixel that shows, which keeps the edges of a `'linear'` texture from darkening. |
| `image:visibleArea()` | Returns the `Rect` around every pixel that shows, or `nil`. |
| `image:clone()` | Returns a copy. |
| `image:encodePng()` | Returns the bytes of a PNG file of the image. |
| `image:encodePngAsync()` | Returns a promise of the bytes of a PNG file, encoded on the task pool from the image as it is now. |
| `image:blur(radius)` | Blurs the image as softly as a Gaussian of the radius in pixels, from 0 to 64, with three box blurs whose cost does not grow with the radius. It mixes the colors premultiplied by their alpha, so the transparent pixels around a shape never darken its edge. A radius out of range raises `A blur has a radius from 0 to 64 pixels.` |
| `image:sharpen(amount)` | Raises the contrast of the details by the amount, 1 by default, by subtracting a blur of one pixel. A negative amount raises `A sharpening has an amount of 0 or more.` |
| `image:adjust(options)` | Changes the colors with `hue`, which turns the hue by its turns, `saturation`, which scales the distance from gray, 1 by default, `contrast`, which scales the distance from the middle gray, 1 by default, and `brightness`, which adds to every channel, applied in that order. Unknown keys raise `Unknown option "<key>".` |
| `image:grayscale(amount)` | Moves the colors toward their gray by the amount, 1 by default for full gray. |
| `image:invert()` | Inverts the colors and keeps the alpha. |
| `image:tint(color, amount)` | Moves the colors toward `color` by the amount, 1 by default, weighed by the alpha of the color. |
| `image:pixelate(size)` | Fills every square of `size` pixels with the average of its colors. A size below 1 raises `A pixelation has squares of 1 pixel or more.` |
| `image:outline(color, thickness)` | Draws an outline of `color` around the pixels that show, solid within `thickness` pixels, 1 by default and up to 32, and fading over the pixel past it, under the pixels that show and inside the image, so a sprite needs a transparent margin as wide as its outline. A thickness out of range raises `An outline has a thickness from 0 to 32 pixels.` |
| `image:applyAsync(effects)` | Returns a promise of a copy of the image with the effects of the list applied in order on the task pool, while the image stays as it is. Each effect is a table with its name in `effect` and the options of its method: `{effect = 'blur', radius}`, `{effect = 'sharpen', amount}`, `{effect = 'adjust', brightness, contrast, saturation, hue}`, `{effect = 'grayscale', amount}`, `{effect = 'invert'}`, `{effect = 'tint', color, amount}`, `{effect = 'pixelate', size}` and `{effect = 'outline', color, thickness}`. An unknown name raises `The image effect "<name>" is unknown.` with the names, unknown keys raise `Unknown option "<key>".`, and a value out of range rejects the promise with the message of its method. |
| `image:palette(count)` | Returns up to `count` colors of the visible pixels, 8 by default and from 1 to 64, the most common first, each as `{color, population}` with the share of the pixels it stands for, or an empty list for an image without visible pixels. The colors come from a median cut over a sample of at most 64 pixels per side, so a large image costs what a small one does. A count out of range raises `A palette has from 1 to 64 colors, not <count>.` |
| `image:swatches(count)` | Returns the colors that an interface takes from the image, picked from a palette of `count` colors, 16 by default: `dominant`, the most common color, and `vibrant`, `darkVibrant`, `lightVibrant`, `muted`, `darkMuted` and `lightMuted`, the colors closest to each target of lightness and saturation, each one absent when no color of the palette fits it. |
| `image:placeholder(options)` | Returns a short text that stands for the image while it loads, made of a few cosines of its colors on each axis: `columns` and `rows`, from 1 to 9, 4 and 3 by default, where more keep more of the shapes in a longer text. The text holds only letters, digits, `-` and `_`, 36 characters with the defaults, so a server sends it next to the address of the image. Options out of range raise `An image placeholder keeps from 1 to 9 cosines on each axis.`, and unknown keys raise `Unknown option "<key>".` |

```lua
local assets = require('haylen.assets')
local async = require('async')
local graphics = require('haylen.graphics')

-- A blurred and darkened copy of a picture behind a dialog, made on the task pool, and a locked level card in gray.
async.spawn(function()
    local picture = assets.loadAsync('images/harbor.png', 'image'):await()
    local backdrop = picture:applyAsync({{effect = 'blur', radius = 12}, {effect = 'adjust', brightness = -0.2}}):await()
    local locked = picture:clone()
    locked:grayscale()
    locked:outline('#FF1E2A44', 3)
    print(graphics.newTexture(backdrop, {filter = 'linear'}).width, locked.width)
end)
```

### Gradients

The method `image:fillGradient(gradient, area)` paints colors that blend from stop to stop, which a texture then shows, such as the background of a menu, the sky behind a level, a button or the [image of a shape](graphics2d.md#graphics2ddrawshaperect-options), whose edge stays smooth at any size. The colors blend premultiplied by their alpha, so a stop that fades out never darkens its neighbours. The table `gradient` takes these keys:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `kind` | string | `'linear'` | The kind `'linear'` runs along `angle` across the whole area, from the corner behind its direction to the corner ahead of it, `'radial'` grows from `center` to `radius`, and `'conic'` turns around `center` clockwise from `angle`. |
| `stops` | list | none | The colors as pairs of an offset from 0 to 1 and a color, in increasing order, such as `{{0, '#FF1E2A44'}, {0.6, '#FF4A78C8'}, {1, '#FFF6C26B'}}`. Two stops at one offset switch colors there. |
| `colors` | list of Color | none | The colors spread evenly from 0 to 1, in place of `stops`. |
| `angle` | number | `0` | The direction of a linear gradient and the start of a conic one, in radians clockwise from the x axis, so `math.pi / 2` runs from the top down. |
| `center` | Vec2 | `{0.5, 0.5}` | The center of a radial or conic gradient as a share of the area on each axis. |
| `radius` | number or Vec2 | `0.5` | The radius of a radial gradient as a share of the width and of the height of the area, so the default reaches the middle of every side, and a number sets both. |
| `dither` | boolean | `true` | Spreads the rounding of every pixel to 8 bits with an ordered pattern, so wide gradients show no bands. |

The first color holds before the first stop and the last one after the last stop. A table with both `stops` and `colors` or neither raises `A gradient takes its colors from either "stops" or "colors".`, a stop that is not a pair raises `The stops of a gradient are a list of offset and color pairs, such as {{0, '#FF000000'}, {1, '#FFFFFFFF'}}.`, offsets out of 0 to 1 or out of order raise `The stops of a gradient need offsets from 0 to 1 in increasing order.`, a radius that is not above 0 raises `A radial gradient needs a radius above 0 on both axes.`, and unknown keys raise `Unknown option "<key>".`

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

## Texture

A `Texture` is a shared handle to a GPU image. The function `assets.texture(path, options)` from [`haylen.assets`](assets.md) loads one from the package, where `options` accepts the same `filter` and `wrap` keys as `graphics.newTexture` and the same image loaded with the same options returns the same texture. Other textures come from `graphics.newTexture`, `graphics.whiteTexture`, `target.texture`, `atlas.texture` and `animation.texture`. Two texture values compare equal with `==` when they refer to the same GPU image.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `width` | integer | read | Width in pixels. |
| `height` | integer | read | Height in pixels. |
| `filter` | string | read | Sampling filter, `'nearest'` or `'linear'`. |
| `wrap` | string | read | Addressing mode, `'clamp'`, `'repeat'` or `'mirror'`. |
| `mipmaps` | boolean | read | Whether the texture keeps smaller copies of its image. |

A texture with the `'linear'` filter loaded from a file has the colors of its visible pixels bled into the transparent pixels around them while it decodes, as the [rendering guide](../graphics/rendering.md#textures-and-atlases) explains.

```lua
local assets = require('haylen.assets')

local texture = assets.texture('sprites/knight/run.png', {filter = 'linear'})
print(texture.width, texture.height, texture.filter, texture.wrap, texture == assets.texture('sprites/knight/run.png', {filter = 'linear'}))
```

### texture:update(pixels)

Replaces every pixel of a texture made with `dynamic = true` by `pixels`, a string of raw RGBA bytes, four per pixel, row by row from the top, whose length is `width * height * 4`. The texture keeps its size, and every draw of the frame shows the new pixels. A texture that changes several times in one frame sends only its last pixels, whole and once, before the frame draws, which `graphics2d.stats().uploadedBytes` counts. A texture that is not dynamic raises `Only a dynamic texture changes its pixels in place.`, and pixels of another length raise `The pixels do not match the size of the dynamic texture.`

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

## RenderTarget

A `RenderTarget` is an offscreen color texture created by `graphics.newRenderTarget`. Two target values compare equal with `==` when they refer to the same target.

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

## Font

A `Font` is a TrueType or OpenType font drawn through a signed distance field, or a bitmap font drawn from its own images. The function `assets.font(path, options)` loads either one by the extension of the file: a `.ttf` or `.otf` file takes `bakeSize` (the em size of the glyphs in the atlas, default 48), `spread` (how far the distance field reaches past a glyph, in pixels at the bake size, default 8, which bounds outlines, glows and blurred shadows) and `atlasSize` (initial atlas size, default 512), which must be positive and at most the maximum texture size of the device or raise an error such as `The bake size of a TrueType font must be positive and at most the maximum texture size of 4096.`, and a `.fnt` BMFont file takes the `filter` and `wrap` of its page textures. The functions `graphics.newGridFont` and `graphics.newBitmapFont` make bitmap fonts from textures, and `graphics2d.defaultFont()` returns the built-in one. Every function that takes a font takes either kind. A bitmap font draws pixel for pixel at its native size, scales at other sizes and draws nothing for the characters it lacks. Two font values compare equal with `==` when they refer to the same font, such as a face read twice from a family or a font loaded twice with the same options.

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

Returns the width and height of `text` shaped and laid out with `style`, stretched by its `scale`, which is the block `graphics2d.drawText` covers. The style takes the text keys of `graphics2d.drawText` without its draw order keys, which raise `Unknown option "name".` because a font draws nothing itself. A `size`, `maxWidth`, `maxHeight`, `minScale` or `lineSpacing` that is not a finite number raises `Text needs a finite size, maximum width, maximum height, minimum scale and line spacing.`.

```lua
local graphics2d = require('haylen.graphics2d')

local width, height = graphics2d.defaultFont():measure('Hello', {size = 32})
print(width, height)
```

### font:layout(text, style)

Lays `text` out with the layout keys of `style`, like `graphics2d.drawText` does, where `style` takes the same keys as `font:measure`, and returns a table with these fields. Positions are relative to the anchor point of the text block, before rotation and scale.

| Field | Type | Meaning |
| --- | --- | --- |
| `quads` | table | One entry per visible glyph in the order the lines show them, each with `position` (`Vec2`, top-left corner), `size` (`Vec2`), `source` (`Rect` in the page), `page` (counted from 1) and `font`, the font that draws it. Spaces and line breaks have no entry, and a shaped cluster, such as a ligature or a letter with its marks, has one entry per glyph the font draws for it. |
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

Returns whether the font has a glyph for a character, given as a one-character string or a code point.

```lua
local graphics2d = require('haylen.graphics2d')

local font = graphics2d.defaultFont()
print(font:hasGlyph('A'), font:hasGlyph(0x4E16))
```

### font:glyph(character)

Returns the glyph a character shapes to on its own at the native size as a table with `index` (the glyph index in the font, 0 for a missing glyph), `source` (`Rect` in its page), `offset` (`Vec2` from the pen on the baseline to the top-left of the image), `advance`, `page` (counted from 1) and `visible`.

```lua
local graphics2d = require('haylen.graphics2d')

local glyph = graphics2d.defaultFont():glyph('g')
print(glyph.index, glyph.advance, glyph.offset.y, glyph.source.height)
```

### font:glyphByIndex(index)

Returns the glyph with an index that `font:shape` gave, as the same table as `font:glyph`, which is how an app draws shaped text from the atlas itself. The index of a glyph of a bitmap font is its code point. A TrueType font raises `The font has no glyph with index N.` for an index it does not have, a glyph too large for any atlas the device allows raises `The glyph is larger than the largest font atlas the device can hold.`, and a full atlas that cannot grow raises `The font atlas exceeded the maximum texture size.` every time it is used.

```lua
local graphics2d = require('haylen.graphics2d')

local font = graphics2d.defaultFont()
for _, shaped in ipairs(font:shape('office', {size = 48})) do
    local glyph = font:glyphByIndex(shaped.index)
    print(shaped.cluster, glyph.source.width, glyph.page)
end
```

### font:shape(text, options)

Shapes `text` as one run and returns its glyphs in visual order, each as a table with `index` (the glyph index in the font, which `font:glyphByIndex` reads), `cluster` (the position of the first character of its cluster in the text, counted in code points from 1), `advance` and `offset` (`Vec2` from the pen, where positive y points down). A TrueType font shapes with HarfBuzz in the script of the first letter, so the glyphs show ligatures, the joining forms of Arabic, the conjuncts of Indic scripts, kerning and the marks placed on their letters. A bitmap font maps every character to its glyph and applies its kerning pairs. The table `options` takes `size` (the text size of the lengths, the native size by default), `direction` (`'auto'`, the default, which reads the direction of the first strong letter of the first paragraph, `'leftToRight'` or `'rightToLeft'`) and `language` (a BCP 47 tag such as `'fa'` or `'sr'`, which picks the forms a language prefers). The function `font:layout` and the draw functions split mixed text into runs and shape each one themselves.

```lua
local assets = require('haylen.assets')

local arabic = assets.font('fonts/noto-sans-arabic-regular.ttf')
for _, glyph in ipairs(arabic:shape('لا سلام', {size = 32, language = 'ar'})) do
    print(glyph.index, glyph.cluster, glyph.advance)
end
print(#assets.font('fonts/serif.ttf'):shape('office'))
```

### font:page(index)

Returns the texture of a page, counted from 1, with the glyphs used so far uploaded. An index out of range raises `page out of range`. The atlas of a TrueType font becomes a new, larger texture when it grows, so a page read earlier keeps only the glyphs of its time, and an app that draws from the page reads it again after using new glyphs.

```lua
local graphics2d = require('haylen.graphics2d')

local atlas = graphics2d.defaultFont():page(1)
print(atlas.width, atlas.height)
```

### font:toDistance(pixels, size)

Converts a length in pixels at a text size to the distance field units of the text shader, where 0.5 spans the spread of the field, which shows how far an outline or a glow can reach. Bitmap fonts return 0.

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

## FontFamily

A `FontFamily` is the set of faces rich text draws with, created by `graphics.newFontFamily`. Its faces are read-only properties named like the keys of `graphics.newFontFamily`, which are `nil` for the faces it lacks, and `fallbacks` is the list of its fallback fonts. Two family values compare equal with `==` when they refer to the same family.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local family = graphics.newFontFamily({regular = graphics2d.defaultFont()})
print(family.regular.nativeSize, family.bold, #family.fallbacks)
```

### family:select(style)

Returns the face that draws a style, where `style` takes `bold`, `italic` and `mono`, followed by whether bold and italic are synthesized because the family lacks the real face.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local family = graphics.newFontFamily({regular = graphics2d.defaultFont()})
local face, syntheticBold, syntheticItalic = family:select({bold = true})
print(face == family.regular, syntheticBold, syntheticItalic)
```

### family:resolve(character, style)

Returns the font that draws a character in a style, followed by whether bold and italic are synthesized: the selected face when it has the glyph, otherwise the first fallback that has it, which synthesizes the requested styles, and otherwise the selected face, which draws its missing glyph. The character is a code point or a string, which may hold a letter with its marks, and a font draws it only when it has a glyph for every character of the string.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local family = graphics.newFontFamily({regular = graphics2d.defaultFont(), fallbacks = {assets.font('fonts/cjk.ttf')}})
local font, syntheticBold = family:resolve('世', {bold = true})
print(font == family.fallbacks[1], syntheticBold)
```

### family:measure(text, style)

Returns the width and height of `text` laid out in the family with `style`, like `font:measure` does. The style takes the text keys of `graphics2d.drawText`, where `bold` and `italic` pick the faces, and every character the selected face lacks comes from the first fallback that has it.

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

A `Shader` is a custom shader that `haylen.py shaders` compiled from annotated GLSL for every backend the engine runs on, loaded with `assets.shader(path)` or `assets.load(path)` from a `.shader` file. [Materials](graphics2d.md#material) draw with it, and the [shader guide](../graphics/shaders.md) explains how to write one. The engine creates the GPU program of the active backend the first time a draw needs it.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `name` | string | read | The name of the program in the source. |
| `uniforms` | table | read | The uniforms of the shader's own blocks that materials set, in order, each as `{name = 'time', type = 'float', count = 1}`, where the type is a GLSL type and the count is the number of elements of arrays and 1 otherwise. |
| `textures` | table | read | The names of the textures of the shader's own that materials set. |

Two shaders compare equal when they are the same loaded shader.

```lua
local assets = require('haylen.assets')

local shader = assets.shader('shaders/ripple.shader')
for _, uniform in ipairs(shader.uniforms) do
    print(uniform.name, uniform.type, uniform.count)
end
print(table.concat(shader.textures, ', '))
```

## VectorImage

A `VectorImage` is an SVG document read into curves, loaded with `assets.vectorImage(path)` or `assets.load(path)` from a `.svg` file, or made with [`graphics.newVectorImage`](#graphicsnewvectorimagetext). It draws sharp at any size with [`graphics2d.drawVector`](graphics2d.md#graphics2ddrawvectorimage-x-y-options) and rasterizes into a texture with `image:rasterize(scale)`. The engine reads paths and basic shapes with their fills and strokes, line joins, caps and dashes, linear and radial gradients, transforms, groups, opacity and the view box, and leaves out clip paths, masks, `use`, style sheets and text. The color `currentColor` paints white, so the color of a draw tints the parts that use it, the way icons follow the color of their text. Two images compare equal when they are the same image. A file that changes on disk loads anew the next time the app asks for it, and the images the app holds keep their document.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `width` | number | read | The width the document gives itself: its view box, its width, or the extent of what it draws. |
| `height` | number | read | The height, likewise. |

```lua
local assets = require('haylen.assets')

local star = assets.vectorImage('icons/star.svg')
print(star.width, star.height, star == assets.load('icons/star.svg'))
```

### image:rasterize(scale, options)

Rasterizes the whole image at `scale`, pixels per unit of the image, on a worker thread, and returns a promise of a `Texture` of `ceil(width * scale)` by `ceil(height * scale)` pixels, for draws that need a texture of the image, such as a material or a sprite batch. The UI draws `.svg` files itself, as its [image values](ui.md#value-types) describe. The table `options` takes `filter` and `wrap` like `graphics.newTexture`, with `filter = 'linear'` by default. A scale that is not positive, or whose raster is larger than [`graphics.maxTextureSize()`](#graphicsmaxtexturesize), raises `A vector image rasterizes at a positive scale whose raster fits the maximum texture size of the device.` A raster whose texture finds no room on the device, such as when the texture pool is full, makes `:await()` return `nil` and the message of the device.

```lua
local assets = require('haylen.assets')
local async = require('async')

local gear = assets.vectorImage('icons/gear.svg')
async.spawn(function()
    local texture = gear:rasterize(4):await()
    print(texture.width, texture.height)
end)
```
