# haylen.graphics

`haylen.graphics` creates the GPU resources that every kind of drawing shares, textures, render targets, bitmap fonts and font families, and names the GPU backend. It also describes the `Texture`, `RenderTarget`, `Font`, `FontFamily` and `Shader` values the rest of the engine passes around. Drawing in 2D, and the materials that draw with shaders, live in [haylen.graphics2d](graphics2d.md).

```lua
local graphics = require('haylen.graphics')
```

## Functions

### graphics.newRenderTarget(width, height, options)

Creates a `RenderTarget`, an offscreen texture that `graphics2d.beginTarget` draws into. `options` accepts `filter` (`'nearest'` or `'linear'`, default `'nearest'`) and `wrap` (`'clamp'`, `'repeat'` or `'mirror'`, default `'clamp'`). Sizes of 0 or less raise `Texture dimensions must be positive.` and sizes above the device limit raise `Texture dimensions exceed the device limit.`

```lua
local graphics = require('haylen.graphics')

local target = graphics.newRenderTarget(320, 180, {filter = 'linear'})
print(target.width, target.height, target.texture.width)
```

### graphics.newTexture(width, height, options)

Creates a `Texture` from code. `options` is optional:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `fill` | Color | `'#FFFFFFFF'` | Color of every pixel when `pixels` is absent. |
| `pixels` | string | none | Raw RGBA bytes, four per pixel, row by row from the top. Its length must be `width * height * 4`, otherwise the call raises `Image pixel data does not match its dimensions.` |
| `filter` | string | `'nearest'` | `'nearest'` or `'linear'` sampling. |
| `wrap` | string | `'clamp'` | `'clamp'`, `'repeat'` or `'mirror'` addressing. |

```lua
local graphics = require('haylen.graphics')

local red = graphics.newTexture(4, 4, {fill = '#FFFF0000'})
local checker = graphics.newTexture(2, 2, {
    pixels = string.char(255, 255, 255, 255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255, 255),
    wrap = 'repeat',
})
print(red.width, checker.height)
```

### graphics.newFontFamily(faces)

Creates a `FontFamily` from its faces: `regular`, which it needs, and the optional `bold`, `italic`, `boldItalic` and `mono` faces, plus `fallback`, a list of fonts for the characters a face lacks, such as a CJK, Arabic, Devanagari or symbol font. A family picks the font of every character with its marks as one unit, so a letter and its marks always come from one font, and each run of one font is shaped on its own. `graphics2d.drawText` and `graphics2d.measureText` take a family in place of a font to draw plain text in every script its fonts cover. Rich text draws `[b]` and `[i]` with the real faces the family has and synthesizes the others: a TrueType face grows its strokes and leans its glyphs through its distance field, and a bitmap face draws a bold glyph twice a native pixel apart and leans italic ones. `[code]` uses the mono face, or the regular faces when the family has none. A family without `regular` raises `A font family needs a regular face.`.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local serif = graphics.newFontFamily({
    regular = assets.font('fonts/serif.ttf'),
    bold = assets.font('fonts/serif_bold.ttf'),
    mono = assets.font('fonts/mono.ttf'),
    fallback = {assets.font('fonts/cjk.ttf'), assets.font('fonts/symbols.ttf')},
})
```

### graphics.newBitmapFont(data, pages)

Creates a bitmap `Font` from the contents of a BMFont `.fnt` file, in its text or binary format, and one texture for each of its pages, in order. `assets.font(path)` loads a `.fnt` file and its page images in one call, which suits most apps. A file that is not a BMFont raises an error that names the problem, such as `A BMFont file needs its info and common lines.`.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local pixel = graphics.newBitmapFont(assets.bytes('fonts/pixel.fnt'), {assets.texture('fonts/pixel_0.png', {filter = 'nearest'})})
```

### graphics.newGridFont(texture, options)

Creates a bitmap `Font` from an image of equal cells, read left to right and top to bottom. `options` takes `characters` (the characters of the cells in order, as UTF-8), `cellWidth` and `cellHeight` (the size of a cell in pixels), and the optional `spacing` and `margin` (Vec2 gaps between cells and around the grid), `advance` (the pen advance of every character), `lineHeight` and `baseline` (the baseline below the top of a cell), where 0 takes the size of the cell. An image with fewer cells than characters raises an error.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local digits = graphics.newGridFont(assets.texture('fonts/digits.png', {filter = 'nearest'}), {characters = '0123456789', cellWidth = 12, cellHeight = 16, baseline = 14})
```

### graphics.whiteTexture()

Returns the engine's 1 by 1 white `Texture`, handy for tinted quads in sprites and batches.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local pixel = graphics2d.newSprite(graphics.whiteTexture(), {width = 8, height = 8, color = '#FF40FF80'})
```

### graphics.backend()

Returns the name of the GPU backend: `'glcore'`, `'gles3'`, `'d3d11'`, `'metal'`, `'webgpu'` or `'dummy'` in headless tests.

```lua
local graphics = require('haylen.graphics')
local log = require('haylen.log')

log.info('Rendering with ' .. graphics.backend())
```

## Texture

A `Texture` is a shared handle to a GPU image. `assets.texture(path, options)` from [haylen.assets](assets.md) loads one from the package, where `options` accepts the same `filter` and `wrap` keys as `graphics.newTexture` and the same image loaded with the same options returns the same texture. Other textures come from `graphics.newTexture`, `graphics.whiteTexture`, `target.texture`, `atlas.texture` and `animation.texture`. Two texture values compare equal with `==` when they refer to the same GPU image.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `width` | integer | read | Width in pixels. |
| `height` | integer | read | Height in pixels. |
| `filter` | string | read | Sampling filter, `'nearest'` or `'linear'`. |
| `wrap` | string | read | Addressing mode, `'clamp'`, `'repeat'` or `'mirror'`. |

```lua
local assets = require('haylen.assets')

local texture = assets.texture('tiny_swords/units/blue/warrior/warrior_run.png', {filter = 'linear'})
print(texture.width, texture.height, texture.filter, texture.wrap, texture == assets.texture('tiny_swords/units/blue/warrior/warrior_run.png', {filter = 'linear'}))
```

## RenderTarget

A `RenderTarget` is an offscreen color texture created by `graphics.newRenderTarget`. Two target values compare equal with `==` when they refer to the same target.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `width` | integer | read | Width in pixels. |
| `height` | integer | read | Height in pixels. |
| `texture` | Texture | read | The texture to draw the target with. |

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local target = graphics.newRenderTarget(128, 64)
local preview = graphics2d.newSprite(target.texture, {x = 64, y = 32})
```

## Font

A `Font` is a TrueType or OpenType font drawn through a signed distance field, or a bitmap font drawn from its own images. `assets.font(path, options)` loads either one by the extension of the file: a `.ttf` or `.otf` file takes `bakeSize` (the em size of the glyphs in the atlas, default 48), `spread` (how far the distance field reaches past a glyph, in pixels at the bake size, default 8, which bounds outlines, glows and blurred shadows) and `atlasSize` (initial atlas size, default 512), and a `.fnt` BMFont file takes the `filter` and `wrap` of its page textures. `graphics.newGridFont` and `graphics.newBitmapFont` make bitmap fonts from textures, and `graphics2d.defaultFont()` returns the built-in one. Every function that takes a font takes either kind. A bitmap font draws pixel for pixel at its native size, scales at other sizes and draws nothing for the characters it lacks. Two font values compare equal with `==` when they refer to the same font, such as a face read twice from a family or a font loaded twice with the same options.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `nativeSize` | number | read | The size the glyph images were made for: the bake size of a TrueType font or the size of a bitmap font. |
| `distanceField` | boolean | read | `true` for TrueType fonts, whose text takes outlines, glows, blurs and synthetic styles. |
| `pageCount` | integer | read | The number of textures that hold the glyphs. |

```lua
local assets = require('haylen.assets')

local pixel = assets.font('fonts/pixel.fnt', {filter = 'nearest'})
print(pixel.nativeSize, pixel.distanceField, pixel.pageCount)
```

### font:measure(text, style)

Returns the width and height of `text` shaped and laid out with `style`. The style takes the text keys of `graphics2d.drawText` without its draw order keys, which raise `Unknown option 'name'.` because a font draws nothing itself.

```lua
local graphics2d = require('haylen.graphics2d')

local width, height = graphics2d.defaultFont():measure('Hello', {size = 32})
print(width, height)
```

### font:layout(text, style)

Lays `text` out with the layout keys of `style`, like `graphics2d.drawText` does, where `style` takes the same keys as `font:measure`, and returns a table with these fields. Positions are relative to the anchor point of the text block, before rotation.

| Field | Type | Meaning |
| --- | --- | --- |
| `quads` | table | One entry per visible glyph in the order the lines show them, each with `position` (Vec2, top-left corner), `size` (Vec2), `source` (Rect in the page), `page` (counted from 1) and `font`, the font that draws it. Spaces and line breaks have no entry, and a shaped cluster, such as a ligature or a letter with its marks, has one entry per glyph the font draws for it. |
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

Returns the glyph a character shapes to on its own at the native size as a table with `index` (the glyph index in the font, 0 for a missing glyph), `source` (Rect in its page), `offset` (Vec2 from the pen on the baseline to the top-left of the image), `advance`, `page` (counted from 1) and `visible`.

```lua
local graphics2d = require('haylen.graphics2d')

local glyph = graphics2d.defaultFont():glyph('g')
print(glyph.index, glyph.advance, glyph.offset.y, glyph.source.height)
```

### font:shape(text, options)

Shapes `text` as one run and returns its glyphs in visual order, each as a table with `index` (the glyph index in the font), `cluster` (the position of the first character of its cluster in the text, counted in code points from 1), `advance` and `offset` (Vec2 from the pen, where positive y points down). A TrueType font shapes with HarfBuzz in the script of the first letter, so the glyphs show ligatures, the joining forms of Arabic, the conjuncts of Indic scripts, kerning and the marks placed on their letters. A bitmap font maps every character to its glyph and applies its kerning pairs. `options` takes `size` (the text size of the lengths, the native size by default), `direction` (`'auto'`, the default, which reads the direction of the first strong letter, `'ltr'` or `'rtl'`) and `language` (a BCP 47 tag such as `'fa'` or `'sr'`, which picks the forms a language prefers). `font:layout` and the draw functions split mixed text into runs and shape each one themselves.

```lua
local assets = require('haylen.assets')

local arabic = assets.font('fonts/noto_sans_arabic_regular.ttf')
for _, glyph in ipairs(arabic:shape('لا سلام', {size = 32, language = 'ar'})) do
    print(glyph.index, glyph.cluster, glyph.advance)
end
print(#assets.font('fonts/serif.ttf'):shape('office'))
```

### font:page(index)

Returns the texture of a page, counted from 1, with the glyphs used so far uploaded. An index out of range raises `page out of range`.

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

A `FontFamily` is the set of faces rich text draws with, created by `graphics.newFontFamily`. Its faces are read-only properties named like the keys of `graphics.newFontFamily`, which are `nil` for the faces it lacks, and `fallback` is the list of its fallback fonts. Two family values compare equal with `==` when they refer to the same family.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local family = graphics.newFontFamily({regular = graphics2d.defaultFont()})
print(family.regular.nativeSize, family.bold, #family.fallback)
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

local family = graphics.newFontFamily({regular = graphics2d.defaultFont(), fallback = {assets.font('fonts/cjk.ttf')}})
local font, syntheticBold = family:resolve('世', {bold = true})
print(font == family.fallback[1], syntheticBold)
```

### family:measure(text, style)

Returns the width and height of `text` laid out in the family with `style`, like `font:measure` does. The style takes the text keys of `graphics2d.drawText`, where `bold` and `italic` pick the faces, and every character the selected face lacks comes from the first fallback that has it.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local family = graphics.newFontFamily({regular = graphics2d.defaultFont(), fallback = {assets.font('fonts/noto_sans_hebrew_regular.ttf')}})
print(family:measure('Shalom שלום', {size = 32}))
```

### family:layout(text, style)

Lays `text` out in the family and returns the same table as `font:layout`, where the `font` of every quad is the face or fallback that draws it.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local family = graphics.newFontFamily({regular = graphics2d.defaultFont(), fallback = {assets.font('fonts/noto_sans_devanagari_regular.ttf')}})
local layout = family:layout('Hindi हिन्दी', {size = 32})
for _, quad in ipairs(layout.quads) do
    print(quad.font == family.regular, quad.position.x)
end
```

## Shader

A `Shader` is a custom shader that `make.py shaders` compiled from annotated GLSL for every backend the engine runs on, loaded with `assets.shader(path)` or `assets.load(path)` from a `.shader` file. [Materials](graphics2d.md#material) draw with it, and the [shader guide](../shaders.md) explains how to write one. The engine creates the GPU program of the active backend the first time a draw needs it.

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
