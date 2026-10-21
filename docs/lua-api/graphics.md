# 📖 `haylen.graphics`

The module `haylen.graphics` makes the resources that every kind of drawing shares: textures, render targets, images on the CPU, fonts, font families and vector images. It also names the graphics API and the largest texture the device holds. Drawing in 2D, and the materials that draw with shaders, live in [`haylen.graphics2d`](graphics2d.md). The [rendering guide](../graphics/rendering.md) explains how the engine draws, and the [text guide](../graphics/text.md) explains fonts.

```lua
local graphics = require('haylen.graphics')
```

## Graphics device

### graphics.apiName()

Returns the name of the graphics API that the GPU device draws through: `'metal'`, `'direct3D11'`, `'openGl'`, `'openGlEs'`, `'webGl2'`, `'webGpu'`, or `'none'` on the headless host, where nothing draws. The name is the same on every backend, and it is the value of [`haylen.graphicsApi`](haylen.md#haylengraphicsapi). The [rendering guide](../graphics/rendering.md#the-gpu-device-and-the-backend) lists the API of each platform.

```lua
local graphics = require('haylen.graphics')
local log = require('haylen.log')

log.info('Rendering with ' .. graphics.apiName())
if graphics.apiName() == 'webGl2' then
    log.info('The browser draws through WebGL 2, so programs compile when the first draw needs them.')
end
```

### graphics.maxTextureSize()

Returns the largest width and height in pixels that a texture or render target can have on this device. A larger size raises `Texture dimensions exceed the device limit.`.

```lua
local graphics = require('haylen.graphics')

local size = math.min(4096, graphics.maxTextureSize())
local atlas = graphics.newRenderTarget(size, size)
print(atlas.width, graphics.maxTextureSize())
```

## Texture

A `Texture` is a shared handle to an image on the GPU. Make one with `graphics.newTexture` or `graphics.whiteTexture`, or load one with `assets.texture(path, options)` from [`haylen.assets`](assets.md#assetstexturepath-options), which caches it: the same image loaded with the same options returns the same texture. Other textures come from `target.texture`, `atlas.texture` and `animation.texture`. Two texture values compare equal with `==` when they refer to the same GPU image.

A texture with the `'linear'` filter loaded from a file has the colors of its visible pixels bled into the transparent pixels around them while it decodes, as the [rendering guide](../graphics/rendering.md#textures-and-atlases) explains.

### graphics.newTexture(width, height, options)

Makes a `Texture` of `width` by `height` pixels from code. The table `options` is optional:

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `fill` | Color | `'#FFFFFFFF'` | The color of every pixel when `pixels` is absent. |
| `pixels` | string | none | Raw RGBA bytes, four per pixel, row by row from the top. Its length must be `width * height * 4`, or the call raises `Image pixel data does not match its dimensions.`. |
| `dynamic` | boolean | `false` | Makes a texture whose pixels change in place with [`texture:update(pixels)`](#textureupdatepixels), such as a minimap that code draws. |
| `filter` | string | `'nearest'` | The `'nearest'` or `'linear'` sampling. |
| `wrap` | string | `'clamp'` | The `'clamp'`, `'repeat'` or `'mirror'` addressing. |
| `mipmaps` | boolean | `false` | Keeps smaller copies of the image down to one pixel, which a texture drawn smaller than its size samples, blended between them with the `'linear'` filter. A dynamic texture with mipmaps raises `A texture that changes in place has no mipmaps.`. |

A size of 0 or less raises `Texture dimensions must be positive.`, and a size above [`graphics.maxTextureSize()`](#graphicsmaxtexturesize) raises `Texture dimensions exceed the device limit.`. A filled texture checks its size before it makes any pixel.

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

Makes a `Texture` from `bytes`, a string with the contents of a PNG, JPEG, TGA, BMP or GIF file. Use it for an image that a plugin returns from native code or a file the app downloaded. The texture has the size of the image. The table `options` takes `dynamic`, `filter`, `wrap` and `mipmaps` as above. The options `fill` and `pixels` raise `Unknown option "<key>".`, since the image gives the pixels. Bytes that are not an image raise `The image could not be decoded, and the decoder reported "<reason>".`. Raw RGBA pixels take the form with the width and the height, and images of the package load with `assets.texture`.

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
| `wrap` | string | read | Addressing mode, `'clamp'`, `'repeat'` or `'mirror'`. |
| `mipmaps` | boolean | read | Whether the texture keeps smaller copies of its image. |

```lua
local assets = require('haylen.assets')

local texture = assets.texture('sprites/knight/run.png', {filter = 'linear'})
print(texture.width, texture.height, texture.filter, texture.wrap, texture.mipmaps)
print(texture == assets.texture('sprites/knight/run.png', {filter = 'linear'}))
```

### texture:update(pixels)

Replaces every pixel of a texture made with `dynamic = true`. The string `pixels` holds raw RGBA bytes, four per pixel, row by row from the top, and its length is `width * height * 4`. The texture keeps its size, and every draw of the frame shows the new pixels. A texture that changes several times in one frame sends only its last pixels, whole and once, before the frame draws, which `graphics2d.stats().uploadedBytes` counts. A texture that is not dynamic raises `Only a dynamic texture changes its pixels in place.`, and pixels of another length raise `The pixels do not match the size of the dynamic texture.`.

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

Makes an `Image` of `width` by `height` pixels, every pixel `fill`, which is transparent by default. A size under one pixel raises `An image needs a width and a height of one pixel or more.`, and a size too large for memory raises `Image dimensions are too large to hold in memory.`.

```lua
local graphics = require('haylen.graphics')

local canvas = graphics.newImage(256, 128, '#FF1E2A44')
print(canvas.width, canvas.height)
```

### graphics.decodeImage(bytes)

Decodes the bytes of a PNG, JPEG, TGA, BMP or GIF file into an `Image` on the frame thread. Bytes that are not an image raise `The image could not be decoded, and the decoder reported "<reason>".`. The `image` asset type of [`haylen.assets`](assets.md) loads the images of the package the same way, and `assets.loadAsync(path, 'image')` does it off the frame thread.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local icon = graphics.decodeImage(assets.bytes('icons/star.png'))
print(icon.width, icon.height)
```

### graphics.decodeImageAsync(bytes)

Decodes like `graphics.decodeImage` on the task pool, and returns a promise of the image. The promise rejects with the same message when the bytes are not an image.

```lua
local async = require('async')
local graphics = require('haylen.graphics')
local http = require('http')

async.spawn(function()
    local response = http.client.get('https://example.com/shoes.png'):await()
    local image, failure = graphics.decodeImageAsync(response.body):await()
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
| `A radial gradient needs a radius above 0 on both axes.` | The radius is not above 0. |
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

A `Font` is a TrueType or OpenType font drawn through a signed distance field, or a bitmap font drawn from its own images, as the [text guide](../graphics/text.md#fonts) explains. Every function that takes a font takes either kind. A bitmap font draws pixel for pixel at its native size, scales at other sizes and draws nothing for the characters it lacks. Two font values compare equal with `==` when they refer to the same font, such as a face read twice from a family or a font loaded twice with the same options.

The function `assets.font(path, options)` of [`haylen.assets`](assets.md#assetsfontpath-options) loads either kind by the extension of the file:

- A `.ttf` or `.otf` file takes `bakeSize`, the em size of the glyphs in the atlas, 48 by default. It takes `spread`, how far the distance field reaches past a glyph in pixels at the bake size, 8 by default, which bounds outlines, glows and blurred shadows. It takes `atlasSize`, the first size of the atlas, 512 by default. Each one must be positive and at most the maximum texture size of the device, or it raises an error such as `The bake size of a TrueType font must be positive and at most the maximum texture size of 4096.`.
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
| `quads` | table | One entry per visible glyph in the order the lines show them, each with `position` (`Vec2`, the top-left corner), `size` (`Vec2`), `source` (`Rect` in the page), `page` (counted from 1) and `font`, the font that draws it. Spaces and line breaks have no entry. A shaped cluster, such as a ligature or a letter with its marks, has one entry per glyph the font draws for it. |
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

Returns whether the font has a glyph for `character`, given as a string of one character or as a code point. A longer string raises a `bad argument` error that ends with `expected one character or a code point`.

```lua
local graphics2d = require('haylen.graphics2d')

local font = graphics2d.defaultFont()
print(font:hasGlyph('A'), font:hasGlyph(0x4E16))
```

### font:glyph(character)

Returns the glyph that `character` shapes to on its own, at the native size, as a table:

| Field | Meaning |
| --- | --- |
| `index` | The glyph index in the font, 0 for a missing glyph. |
| `source` | The `Rect` of the glyph in its page. |
| `offset` | The `Vec2` from the pen on the baseline to the top-left corner of the image. |
| `advance` | How far the pen moves after the glyph. |
| `page` | The page that holds the glyph, counted from 1. |
| `visible` | Whether the glyph draws anything. |

```lua
local graphics2d = require('haylen.graphics2d')

local glyph = graphics2d.defaultFont():glyph('g')
print(glyph.index, glyph.advance, glyph.offset.y, glyph.source.height)
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

## FontFamily

A `FontFamily` is the set of faces of one typeface, with fallback fonts for the characters its faces lack, as the [text guide](../graphics/text.md#font-families) explains. Rich text draws with a family, and `graphics2d.drawText` and `graphics2d.measureText` take one in place of a font. Two family values compare equal with `==` when they refer to the same family.

### graphics.newFontFamily(faces)

Makes a `FontFamily` from the table `faces`:

| Key | Type | Meaning |
| --- | --- | --- |
| `regular` | Font | The regular face, which every family needs. |
| `bold`, `italic`, `boldItalic`, `mono` | Font | The other faces, each one optional. |
| `fallbacks` | list of Font | Fonts for the characters a face lacks, such as a CJK, Arabic, Devanagari or symbol font, tried in order. |

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

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local family = graphics.newFontFamily({regular = graphics2d.defaultFont()})
print(family.regular.nativeSize, family.bold, family.italic, family.boldItalic, family.mono, #family.fallbacks)
```

### family:select(style)

Returns the face that draws a style, followed by whether bold and whether italic are synthesized because the family lacks the real face. The table `style` takes `bold`, `italic` and `mono`.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local family = graphics.newFontFamily({regular = graphics2d.defaultFont()})
local face, syntheticBold, syntheticItalic = family:select({bold = true})
print(face == family.regular, syntheticBold, syntheticItalic)
```

### family:resolve(character, style)

Returns the font that draws `character` in a style, followed by whether bold and italic are synthesized. The font is the first of these that fits:

- The selected face, when it has the glyph.
- The first fallback that has the glyph, which synthesizes the styles asked for.
- The selected face again, which draws its missing glyph. The character is a code point or a string, which may hold a letter with its marks. A font draws the string only when it has a glyph for every character of it. The table `style` takes `bold`, `italic` and `mono`, and an empty string raises a `bad argument` error that ends with `expected a character`.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local family = graphics.newFontFamily({regular = graphics2d.defaultFont(), fallbacks = {assets.font('fonts/cjk.ttf')}})
local font, syntheticBold = family:resolve('世', {bold = true})
print(font == family.fallbacks[1], syntheticBold)
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

A `Shader` is a custom shader that `haylen.py shaders` compiled from the Haylen shader language for every shader language the engine runs. Load it from a `.shader` file with `assets.shader(path)` or `assets.load(path)`. [Materials](graphics2d.md#material) draw with it, and the [shader guide](../graphics/shaders.md) explains how to write one. The engine creates the GPU program for the shader language of the GPU device the first time a draw needs it. Two shaders compare equal with `==` when they are the same loaded shader.

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

A `VectorImage` is an SVG document read into curves, which draws sharp at any size with [`graphics2d.drawVector`](graphics2d.md#graphics2ddrawvectorimage-x-y-options). Load one from a `.svg` file with `assets.vectorImage(path)` or `assets.load(path)`, or make one with `graphics.newVectorImage`. The [rendering guide](../graphics/rendering.md#vector-images) lists the parts of SVG that the engine reads and explains how it rasterizes them. Two images compare equal with `==` when they are the same image. A file that changes on disk loads anew the next time the app asks for it, and the images the app holds keep their document.

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

Rasterizes the whole image at `scale`, in pixels per unit of the image, on a worker thread. It returns a promise of a `Texture` of `ceil(width * scale)` by `ceil(height * scale)` pixels, for draws that need a texture of the image, such as a material or a sprite batch. The UI draws `.svg` files itself, as its [image values](ui.md#value-types) describe. The table `options` takes `filter` and `wrap` like `graphics.newTexture`, with `filter = 'linear'` by default.

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
| `Image pixel data does not match its dimensions.` | The `pixels` of `graphics.newTexture` have another length than `width * height * 4`. |
| `A texture that changes in place has no mipmaps.` | A dynamic texture asked for mipmaps. |
| `A render target has no mipmaps.` | A render target asked for mipmaps. |
| `Only a dynamic texture changes its pixels in place.` | The method `texture:update` ran on a texture that is not dynamic. |
| `The pixels do not match the size of the dynamic texture.` | The pixels of `texture:update` have another length. |
| `The image could not be decoded, and the decoder reported "<reason>".` | The bytes of `graphics.newTexture`, `graphics.decodeImage` or `graphics.decodeImageAsync` are not an image. |
| `An image needs a width and a height of one pixel or more.` | The size of `graphics.newImage` is under one pixel. |
| `Image dimensions are too large to hold in memory.` | The size of `graphics.newImage` is too large. |
| `A read needs a valid render target.` | The target of `graphics.readTarget` is not valid. |
| `The area <x>, <y>, <w> by <h> is not inside the image of <w> by <h> pixels.` | The area of `image:crop` lies outside the image. |
| `An empty image has no PNG file.` | The image of `image:encodePng` has no pixels. |
| `A blur has a radius from 0 to 64 pixels.` | The radius of `image:blur` is out of range. |
| `A sharpening has an amount of 0 or more.` | The amount of `image:sharpen` is negative. |
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
| `Only version 3 of the binary BMFont format is supported.` | The binary file has another version. |
| `The binary BMFont file ends in the middle of a block.` | The binary file is cut short. |
| `The binary BMFont file has a block of unknown type <type>.` | The binary file holds a block it should not. |
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
