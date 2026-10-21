# 🔤 Text

This guide explains how Haylen draws text. It is for app developers who show text in any language, from a score on the HUD to a dialogue in Arabic or a page of rich text. It covers fonts, font families and bitmap fonts, and how glyphs are made and drawn. It explains how text in every script is shaped, ordered in its direction and broken into lines. It ends with the markup of rich text, its effects and typewriter reveal, and how 2D drawing and the UI show it. The [`haylen.graphics` reference](../lua-api/graphics.md#font) lists the font API, the [`haylen.graphics2d` reference](../lua-api/graphics2d.md#richtext) the rich text API, and the [`haylen.ui` reference](../lua-api/ui.md#uirichtextproperties) the `richText` component.

## Fonts

Haylen has two kinds of fonts, and every function that takes a font takes either kind: `graphics2d.drawText`, `graphics2d.measureText`, the families of rich text and the text objects of Tiled maps.

- A TrueType or OpenType font draws through a signed distance field. That is a single-channel image that stores, for every pixel, how far it lies from the edge of a glyph. One atlas serves every text size, and the text shader draws outlines, bolder strokes, blurred shadows and glows from the same field.
- A bitmap font draws prepared images, pixel for pixel at its own size, as [bitmap fonts](#bitmap-fonts) explains.

A text size is the height of the em square of the font, the square its designer draws every glyph in. So faces and fallback fonts of one size line up whatever their proportions. A line is as tall as the ascent, descent and line gap of the font at that size. The functions `font:lineHeight`, `font:ascent` and `font:capHeight` report these heights, and `font:capHeight` tells how high the capital H reaches, which centers a word of capitals on a button.

The field reaches `spread` pixels past a glyph at the bake size, 8 by default, and that reach bounds every effect. Take a text size of 24 and the default bake size of 48. There an outline, a glow and a blur together reach at most 4 pixels, and a little less when small text smooths its edges over more of the field. Effects that would reach further shrink together, so the box of a glyph never shows. Load a font with a larger `spread` to reach further. Outlined text draws every outline first and every fill over them, so a wide outline never covers the letters beside it.

The bake size, the spread and the first `atlasSize` must be positive and at most the maximum texture size of the device, since no atlas could hold a larger glyph. The atlas grows by doubling as new glyphs are used. A grown atlas is a new texture, so text drawn earlier in the same frame keeps the image it was measured on.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local title = assets.font('fonts/title.ttf', {bakeSize = 64, spread = 12})

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(title, 'Island', 40, 40, {size = 96, outlineWidth = 6, outlineColor = '#FF402000'})
    end,
})
```

## How glyphs are drawn

Fonts live in `engine/include/haylen/text/`. Both kinds sit behind one interface, `text::Font`, which lays plain text out the same way for both: shaped glyphs at a native size, and texture pages that hold them.

The class `text::TrueTypeFont` shapes text with HarfBuzz and draws through the distance field:

- A glyph is made by its index in the font the first time a string uses it. The reader `stb_truetype` reads its outline: lines, quadratic curves and the cubic curves of CFF fonts.
- The core of msdfgen turns the outline into its exact distance field. It works at the `bakeSize` of the font, 48 by default, which is the size of the em square, with a distance `spread` of 8 pixels.
- The field comes from the whole outline. The quadratic curves of TrueType fonts and the cubic curves of the `CFF` outlines of many `.otf` files work alike, so both kinds draw cleanly at every size. Contours that overlap, as in the glyphs of many variable fonts, keep their shared inside whole.
- The fields are packed in shelves into a single-channel atlas that starts at `atlasSize` pixels square, 512 by default. A full atlas doubles its shorter side and keeps every glyph where it was, up to the maximum texture size of the device. Past that, layout raises `The font atlas exceeded the maximum texture size.`. A single glyph whose field could not fit even the largest atlas raises `The glyph is larger than the largest font atlas the device can hold.` before its field is computed.
- The atlas is a dynamic texture. A draw that added glyphs hands the atlas to `Device::updateTexture`, and the renderer sends the whole atlas once per frame before its passes, however many draws added glyphs. An atlas that grew becomes a new texture, so text drawn earlier in the frame keeps the image its coordinates were made for. The atlas of the Dear ImGui interface changes in place the same way.

The class `text::BitmapFont` draws BMFont files and grids of equal cells from their own RGBA pages, pixel for pixel at their native size, through the sprite program.

The text program draws the glyphs of distance field fonts. It smooths the edge of the field with the screen-space derivative, which keeps edges crisp when text is scaled. It reads four values of every glyph from its instance:

- The weight moves the edge outward. This is how synthetic bold grows the strokes of a face that a family lacks.
- The outline is a second threshold of the same field, filled with the flash color. The fill covers it with premultiplied colors, so every edge blends its coverage once.
- The softness widens the edge into a blur, for soft shadows and glows.
- The skew leans the quad around the baseline in the vertex shader. This is how synthetic italic slants.

The outline, the weight, the softness and the smoothing of the edge on screen add up. Together they stop a sixteenth of the field short of the border of the quad of the glyph, where the field fades out. When they would reach further, they shrink together, so no size or scale shows the box of a glyph.

Text with effects draws in layers. A shadow draws every glyph once more before the outlines: moved by its offset, in the shadow color and softened by its blur. The shadow of a bitmap glyph is its silhouette. Every outline draws next, as glyphs whose fill is transparent, and every fill draws over them. So the outline of a letter never covers the letters beside it.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(nil, 'Game over', 960, 540, {size = 96, anchor = {0.5, 0.5}, outlineWidth = 4, outlineColor = '#FF000000', shadowOffset = {6, 6}, shadowColor = '#80000000', shadowBlur = 4})
    end,
})
```

## Sharp small text

Small text looks sharpest when its glyphs start on whole pixels, since a baseline between two pixel rows spreads every horizontal stroke over both. The style option `pixelSnap` moves every line of plain text so its left edge lands on a pixel column of the destination. This keeps centered and right-aligned lines as crisp as the others. It also puts the baseline on a pixel row. It works in whatever units the canvas uses and at any zoom, so text that moves steps from pixel to pixel instead of shimmering. Text that turns, or that draws in a canvas whose view turns, keeps its exact place, because no pixel row runs along its baselines.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(nil, 'Ammo 12 / 30', 40.3, 1020.6, {size = 18, pixelSnap = true})
    end,
})
```

## Bitmap fonts

A bitmap font draws prepared images: the pages of a BMFont file, the text or binary font format that bitmap font tools write, or an image of equal cells. It draws pixel for pixel at its native size and scales at other sizes. It keeps the colors of its images, which the text color multiplies, and draws nothing for the characters it lacks. It has no distance field, so it takes no outline, glow or blur, and its shadow is the silhouette of its glyphs in the shadow color. Load BMFont files with nearest filtering for crisp pixel art.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local pixel = assets.font('fonts/pixel.fnt', {filter = 'nearest'})
local digits = assets.load('fonts/digits.png', 'gridFont', {characters = '0123456789', cellWidth = 12, cellHeight = 16, filter = 'nearest'})

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(pixel, 'PRESS START', 40, 200, {size = pixel.nativeSize * 2})
        graphics2d.drawText(digits, '1200', 40, 280, {size = 32, shadowOffset = {2, 2}, shadowColor = '#FF000000'})
    end,
})
```

## Default fonts

The engine embeds two fonts. Roboto Medium, from Dear ImGui, is the default font, which `graphics2d.defaultFont()` and `Engine::getDefaultFont()` return and which every text function uses when it gets `nil` as its font. Space Mono, from `engine/fonts/` with its license, is the monospace font of the UI and the mono face of the default family, which draws the `[code]` of rich text.

The function `assets.font(path, options)` loads other fonts, TrueType or BMFont by the extension of the file. A larger `bakeSize` keeps corners sharper at large sizes, at the price of atlas space. Each font has its own atlas texture, so switching fonts between draws starts a new draw call. The [assets reference](../lua-api/assets.md#assetsfontpath-options) describes the options.

```lua
local graphics2d = require('haylen.graphics2d')

local font = graphics2d.defaultFont()
print(font.nativeSize, font:lineHeight(32))
```

## Font families

A family groups the faces of one typeface: regular, bold, italic, bold italic and mono. It adds fallback fonts for the characters its faces lack. A fallback can be an Arabic, Hebrew, Devanagari or Thai font, a CJK font for Chinese and Japanese, or a symbol font for arrows and stars.

Layout picks the font of each run from its style and from each character, where a character is a letter with its marks as one unit. It takes the first font that has every code point of the character: the face, then each fallback in order. When none has it, the face draws its missing glyph box. Spaces, punctuation and digits keep the font of the text before them, so an Arabic sentence stays in the Arabic font up to its full stop. The fonts of the Noto family cover every script with a consistent look, and a fallback for each script the game shows is enough.

A style that the family has no face for is synthesized from the face it has:

- A distance field face grows its strokes on both sides by 3 percent of the text size and widens its advance to match. It leans italic glyphs by one fifth of their height around the baseline in the vertex shader.
- A bitmap face draws a bold glyph a second time one native pixel to the right, and leans italic glyphs the same way.
- A fallback font synthesizes the bold and italic its run asks for.
- The tag `[code]` uses the mono face, or the regular faces when the family has none.

The functions `graphics2d.drawText` and `graphics2d.measureText` take a family in place of a font, which draws text in every script its fonts cover.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local story = graphics.newFontFamily({
    regular = assets.font('fonts/serif.ttf'),
    bold = assets.font('fonts/serif-bold.ttf'),
    italic = assets.font('fonts/serif-italic.ttf'),
    mono = assets.font('fonts/mono.ttf'),
    fallbacks = {assets.font('fonts/cjk.ttf')},
})
local face, syntheticBold, syntheticItalic = story:select({bold = true, italic = true})
```

## Scripts and directions

HarfBuzz shapes every run of text before it is laid out. Runs of one font, one script, one direction and one style shape together, with the whole paragraph around them as context. Shaping applies the OpenType features of the font:

- Latin text takes its ligatures and kerning.
- Arabic, Persian and Urdu letters take the form their neighbours ask for and join, and lam and alef merge.
- Devanagari consonants form conjuncts and place their vowel signs before or above them.
- The marks of Thai, Hebrew and every other script sit on their letters.

The `language` of a style or of rich text is a BCP 47 tag such as `'ar'`, `'fa'`, `'ur'`, `'hi'` or `'sr'`. It picks the forms a language prefers where the font has them, such as the Urdu forms of some digits or the Serbian forms of some Cyrillic letters.

A character is a cluster: the code points that shaping keeps together, such as a letter with its marks, a conjunct with its vowel signs, or a ligature. Characters are what the typewriter reveal counts, what text fields move the caret over, and what hit tests and selections cover. So a reveal or a caret never stops inside a letter.

Every paragraph runs through the Unicode Bidirectional Algorithm of SheenBidi. Its direction is `'auto'` by default, taken from its first strong letter, so an Arabic or Hebrew paragraph reads right to left and an English one left to right. The values `direction = 'leftToRight'` and `'rightToLeft'` force it. After a paragraph is broken into lines, every line is ordered for display on its own. Runs that read right to left go from the right, numbers and Latin words inside them keep their own order, and brackets mirror so that `(USD)` reads correctly inside Arabic text. A paragraph that starts with a Latin word or a number but reads right to left needs a forced direction, or a right-to-left mark, U+200F, at its start.

Alignment names the sides of a paragraph by its direction:

- The alignment `'start'`, the default, lines text up where its lines begin: the left of left-to-right text and the right of right-to-left text. The alignment `'end'` is the other side.
- The alignments `'left'`, `'center'` and `'right'` name fixed sides.
- The alignment `'fill'` stretches every wrapped line to both edges and leaves the last line at its start.

In rich text, `[p dir=rightToLeft]` sets the direction of a block and `[p align=end]` its alignment. The indent, the list markers and the drop cap stand on the side where the paragraph starts. A table reads in the direction of its first paragraph, so its first column stands at the right of a right-to-left table. Backgrounds, underlines, strikes and link areas cover the shaped text after it is ordered. They split where the direction changes and grow with the reveal from the side where their text starts.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local family = graphics.newFontFamily({
    regular = graphics2d.defaultFont(),
    fallbacks = {assets.font('fonts/noto-sans-arabic-regular.ttf'), assets.font('fonts/noto-sans-hebrew-regular.ttf'), assets.font('fonts/noto-sans-devanagari-regular.ttf'), assets.font('fonts/noto-sans-thai-regular.ttf')},
})
local story = graphics2d.newRichText('[p dir=rightToLeft align=start][b]الرحلة[/b] بدأت في [u]الصباح[/u] الباكر.[/p]', {family = family, size = 30, maxWidth = 500, language = 'ar', revealSpeed = 20})

scene.push({
    update = function(self, dt)
        story:update(dt)
    end,
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(family, 'السعر 42 دولارًا (USD)', 1000, 100, {size = 32, anchor = {1, 0}})
        graphics2d.drawText(family, 'Hebrew: שלום עולם', 100, 160, {size = 32})
        graphics2d.drawText(family, 'Order 7 السعر', 1000, 220, {size = 32, direction = 'rightToLeft', anchor = {1, 0}})
        story:draw(500, 300)
    end,
})
```

A bitmap font has no shaping tables, so it maps every code point to its own glyph and applies the kerning pairs of its file. It lays out and orders right-to-left text, with mirrored brackets, but it draws neither joined Arabic letters nor Indic conjuncts, which need a TrueType or OpenType font.

## Line breaking

Lines break where the Unicode Line Breaking Algorithm of libunibreak allows: after spaces and hyphens, and between Chinese and Japanese characters. They never break before a closing punctuation mark or inside a number.

These characters end a paragraph: a line feed, a carriage return, CRLF, which counts as one, next line (U+0085), the paragraph separator U+2029 and the information separators U+001C to U+001E. A line separator (U+2028), a vertical tab or a form feed breaks the line inside its paragraph. Tabs and the other control characters draw nothing and take no room.

The language tailors the rules: the quotation marks of English, German, Spanish, French and Russian text, and the ambiguous punctuation of Chinese text. Thai writes no spaces between words, so a line of Thai breaks between the phrases that the Thai model of BudouX finds. That model is small and weighs the letters around every place of the text. Lao, Khmer and Burmese also write without spaces, and their lines break only at spaces and punctuation, because the engine has no model for them.

A hyphen-minus between two letters or digits joins the parts of one name, such as an identifier, a file name or a date, so a line never breaks there. A hyphen after a space may still end a line. A word wider than a whole line breaks between its characters, never inside a cluster.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        -- The file name stays whole, so the line breaks at the space before it.
        graphics2d.drawText(nil, 'Saved the game to slot-03-2026-10-21.sav', 100, 100, {size = 32, maxWidth = 420})
    end,
})
```

## Layout

Plain text lays out in these steps:

1. Layout decodes the UTF-8 text and splits it into runs of one font, one script and one direction, and shapes every run.
2. It ends paragraphs at every paragraph separator, such as `\n`, `\r\n` or U+2029, and lines at mandatory breaks.
3. When `maxWidth` is set, it wraps lines by the Unicode line breaking rules.
4. It orders every line for display by the bidirectional algorithm.
5. It aligns lines to their start, end, left, center or right, or fills them.
6. It places the block by its `anchor`, with an optional rotation and scale.

Plain text is a document of one paragraph per line. The carriage return of CRLF stays at the end of its line and draws nothing, so the characters of the layout keep counting the code points of the text.

Every font and every family keeps its 512 most recent plain text layouts. They are found by the text and by the style fields that change the layout: size, wrap width, line spacing, alignment, direction, language, bold and italic. The size, the wrap width and the line spacing must be finite numbers. So text that `graphics2d.drawText` draws every frame, and the labels of the UI, shape once and then only place their glyphs. Color, outline, shadow, anchor and rotation apply when the text draws and never lay it out again.

Rich text lays each paragraph out into lines. Lines wrap where the [line breaking](#line-breaking) rules allow and around images, and inside a word only when the word alone is wider than a line. Every line stands on one baseline and is as tall as its tallest text and images. Lines follow each other by their height times the line spacing, so a line with a larger size or an image makes room for it. Images are centered by default on the middle of the text of their style. The value `baseline` puts their bottom on the baseline, and `top` and `bottom` align them with the line.

The layout of rich text is cached by its width and scale, and a few widths stay cached. So a UI container that measures text at one width and draws it at another lays it out once for each, and `text:size(maxWidth)` measures at another width the same way. Changing the markup or the options lays it out again, and effects and the reveal never do. An image that is still loading takes no room, and the layout is built again once it arrives.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local hint = graphics2d.newRichText('Hold [b]Space[/b] to run.', {size = 28})

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        local width, height = hint:size(300)
        graphics2d.drawRect({100, 100, width, height}, '#C0000000')
        hint:draw(100, 100)
    end,
})
```

## Fitting text in a box

A text style whose `minScale` is below 1 fits the text in its box, as [`graphics2d.drawText`](../lua-api/graphics2d.md#graphics2ddrawtextfont-text-x-y-style) describes. The layout takes the largest size, down to that share of `size`, where wrapped lines fit `maxHeight`, or where text that does not wrap, with `wrap = false`, fits `maxWidth` and `maxHeight`. The search halves the range of sizes a few times when the layout is first made. The fitted layout stays in the layout cache of its font, so text drawn every frame fits once. The same options fit the text of nodes, and labels of the UI fit their text into their bounds through their `minScale`.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(nil, 'Captain Marigold of the Northern Sea', 960, 560, {size = 48, maxWidth = 360, wrap = false, minScale = 0.5, anchor = {0.5, 0.5}})
    end,
})
```

## Markup

Rich text is text with tags in square brackets:

- Inline tags open with `[name]` or `[name=value attribute=value]` and close with `[/name]`, in reverse order of opening.
- Values and attributes may be quoted to hold spaces, and the values of `[url]` and `[hint]` run to the closing bracket.
- The tags `[lb]` and `[rb]` write `[` and `]`.
- A newline ends a paragraph, whether the markup ends its lines with LF, CRLF or any other [paragraph separator](#line-breaking), and `[br]` breaks a line without ending the paragraph.
- Colors are names such as `red`, `gold` or `transparent`, `#RRGGBB` or `#AARRGGBB`, like the rest of the engine. Sizes are pixels at a scale of 1.

| Tag | Meaning |
| --- | --- |
| `[b]`, `[i]`, `[u]`, `[s]` | Bold, italic, underline and strike. |
| `[code]` | The mono face of the family, which in the default family of the engine is Space Mono, a monospace font without ligatures. |
| `[color=c]`, `[bgcolor=c]` | Text color and a background behind the text. |
| `[font=name]` | A font family that the host names: the `fonts` option in 2D and the fonts of `ui.addFont` in the UI. |
| `[size=24]`, `[size=150%]` | An absolute size, or a size relative to the text around it. |
| `[outline=3 color=c]` | An outline 3 pixels wide, black by default. |
| `[shadow=2,3 color=c blur=2]` | A shadow at an offset, 2,2 and half transparent black by default, softened by the blur. |
| `[glow=6 color=c]` | A glow that fades out 6 pixels past the text, white by default. |
| `[alpha=0.5]` | Multiplies the opacity of the text, images and icons inside. |
| `[url=payload]`, `[url]text[/url]` | A link, underlined unless the host turns that off, whose payload hit tests and the UI report. Without a payload the text is the payload. |
| `[hint=text]` | A hint that the UI shows as a tooltip and `text:hintAt` reports. |
| `[img=path width=24 height=24 region=x,y,w,h color=c valign=center]` | An inline image of the package content, which keeps the shape of its region when only one side is given. Rich text that draws every frame, such as `graphics2d.drawRichText`, loads each image once and keeps it while frames draw it. |
| `[icon=name width= height= color= valign=]` | An icon that `graphics2d.registerTextIcon` registered, as tall as its text unless sized. |
| `[pause=0.5]` | Holds the typewriter reveal for half a second before the next character. |
| `[speed=2]` | Reveals the text inside twice as fast. |
| `[p align=center dir=rightToLeft indent=1 hang=2]` | A paragraph block with an alignment (`start`, `end`, `left`, `center`, `right` or `fill`) and a direction (`auto`, `leftToRight` or `rightToLeft`). The indent counts levels of one and a half times the base size and stands on the side where the paragraph starts. The hanging indent, in the same levels, moves every line after the first further in, as wrapped code reads. |
| `[center]`, `[left]`, `[right]`, `[fill]` | Paragraph blocks with an alignment. Fill stretches the spaces of every wrapped line to both edges. |
| `[ul bullet=*]`, `[ol type=1]` | Lists, where every paragraph inside is an item with a bullet or a number, counted as `1`, `a`, `A`, `i` or `I`. Lists nest and indent one level each. |
| `[hr width=50% height=2 color=c]` | A horizontal rule, centered unless its block aligns it. |
| `[table=3]` with `[cell bg=c border=c padding=4]` | A table of 1 to 64 columns whose cells read row by row. Columns take the width their content wants. When the table is too wide, they shrink in proportion to the room of their widest word. |
| `[dropcap size=64 font=name color=c margin=4]W[/dropcap]` | A large first letter, three times the base size unless sized. Its capital lines up with the top of the capitals of the first line: the height of a capital H, or the ascent of a font without one. The lines whose capitals start above its foot flow around it, so a drop cap sized to span whole lines also stands on the baseline of the last of them. It must start its paragraph. |

Block tags end the paragraph before them. A newline right after an opening block tag, or right before a closing one, belongs to the markup, so blocks read naturally on lines of their own. Empty lines inside a list take no number.

```lua
local graphics2d = require('haylen.graphics2d')

local markup = [[
[center][size=150%][b]The Old Lighthouse[/b][/size][/center]
[dropcap]T[/dropcap]he keeper left a note: [i]"Mind the [color=#FF6A6A]crabs[/color]."[/i]
[ul]
Collect [color=gold]12 wood[/color] [img=icons/wood.png height=24]
Press [icon=confirm] to light the [glow=6 color=#FF8000]beacon[/glow]
[/ul]
[table=2][cell]Wood[/cell][cell]12[/cell][cell]Stone[/cell][cell]4[/cell][/table]
[url=map]Open the map[/url]
]]
local page = graphics2d.newRichText(markup, {size = 28, maxWidth = 600})
```

Any other tag name runs a text effect of that name. Malformed markup raises an error that starts with its line and column, counted from 1, such as `Rich text markup at line 2, column 6: The tag "[/b]" closes "[i]", which is still open.`. The problem after the place is one of these:

| Problem | Cause |
| --- | --- |
| `The markup "[<text>]" is not a closing tag.` | A tag starts with `[/` but holds more than a name. |
| `The markup "[<text>]" has no tag name.` | The brackets hold no name. |
| `The markup "[<name>]" is neither a tag nor a registered text effect.` | The name is no tag and no effect. |
| `A tag has no closing bracket.` | A `[` never closes. |
| `The tag "[<name>]" has a quote that never ends.` | A quoted value never closes. |
| `The attribute "<key>" of "[<name>]" needs a value.` | An attribute has no `=value`. |
| `The tag "[<name>]" has no attribute named "<key>".` | The tag takes no such attribute. |
| `The tag "[<name>]" needs a value, as in "[<name>=...]".` | The tag needs a value. |
| `The tag "[<name>]" takes no value.` | The tag takes no value. |
| `The tag "[<name>]" is never closed.` | A tag stays open at the end. |
| `The tag "[/<name>]" closes nothing.` | A closing tag has no opening tag. |
| `The tag "[/<name>]" closes "[<other>]", which is still open.` | Tags close out of order. |
| `The value "<text>" is not a number for the <what> of "[<name>]".` | A number is expected. |
| `The value "<text>" is not a color. Use a name such as "red" or "#RRGGBB" or "#AARRGGBB".` | A color is expected. |
| `The value "<text>" is not a pair of numbers like "2,3" for "[<name>]".` | The offset of a shadow is wrong. |
| `The size of "[<name>]" must be positive.` | A size is 0 or less. |
| `The "valign" of "[<name>]" must be "top", "center", "baseline" or "bottom".` | An image or icon has another alignment. |
| `The alpha of "[alpha]" must be from 0 to 1.` | The alpha is out of range. |
| `The speed of "[speed]" must be positive.` | The speed is 0 or less. |
| `A "[pause]" cannot last a negative time.` | The pause is negative. |
| `The "align" of "[p]" must be "start", "end", "left", "center", "right" or "fill".` | A paragraph has another alignment. |
| `The "dir" of "[p]" must be "auto", "leftToRight" or "rightToLeft".` | A paragraph has another direction. |
| `The type of "[ol]" must be "1", "a", "A", "i" or "I".` | A numbered list has another type. |
| `The width of "[hr]" is a percentage such as "50%".` | A rule has another width. |
| `The region of "[img]" needs "x,y,width,height".` | An image region is wrong. |
| `A "[table]" needs a whole number of columns from 1 to 64.` | The column count is out of range. |
| `Tables cannot be nested.` | A table holds a table. |
| `A "[cell]" goes directly inside a "[table]".` | A cell is outside a table. |
| `Only "[cell]" tags go directly inside a "[table]", not "[<name>]".` | A table holds another tag. |
| `Text inside a "[table]" must be inside a "[cell]".` | A table holds loose text. |
| `A "[dropcap]" must start its paragraph.` | Text comes before the drop cap. |
| `A "[dropcap]" needs some text.` | The drop cap is empty. |
| `A "[dropcap]" holds plain text, not "[<name>]".` | The drop cap holds a tag. |
| `A "[dropcap]" cannot hold a line break.` | The drop cap holds a line break. |

## Effects

Effects move and color the glyphs of their tag every frame: their offset, color and visibility. Rich text keeps the laid out glyphs and applies the effects to a copy. So the time alone decides where a glyph is, and the same time always gives the same picture. Effects nest, and the outer one runs first. These effects are built in:

| Effect | Attributes | Motion |
| --- | --- | --- |
| `[wave]` | `amp` (20), `freq` (5) | Glyphs bob up and down in a wave along the line. |
| `[shake]` | `rate` (20), `level` (5) | Glyphs jump to a new random offset `rate` times a second. |
| `[tornado]` | `radius` (10), `freq` (1) | Glyphs circle around their place. |
| `[fade]` | `start` (0), `length` (10) | Glyphs fade out over `length` characters from the character `start` of the tag. |
| `[rainbow]` | `freq` (1), `sat` (0.8), `val` (0.8), `speed` (1) | Glyphs cycle through the hues. |
| `[pulse]` | `freq` (1), `color` (`#40FFFFFF`), `ease` (-2) | Glyphs pulse toward their color multiplied by `color`. |

An attribute of the wrong kind raises an error when the effect runs, such as `The "amp" of "[wave]" must be a number, not "high".` or `The "color" of "[pulse]" must be a color such as "red" or "#RRGGBB", not "shiny".`.

Apps register their own effects from Lua with `graphics2d.registerTextEffect(name, effect)`, whose function receives each glyph and the attributes of the tag. From C++ they use `text::RichTextRegistry::registerEffect`, whose function receives a `text::Effect::Glyph` and its `text::Effect::Parameters`. An effect sees the index of the glyph inside its tag, which counts every character from the first one of the tag, spaces and images included. It also sees its character in the whole text, its pen position on the baseline and the time. Effects run while the text builds the picture of the moment, so an effect cannot change or lay out the text it runs on. Setting its markup, options, width or scale, updating it or measuring it from inside the effect raises `A text effect cannot change or lay out the rich text it runs on.`.

```lua
local graphics2d = require('haylen.graphics2d')

graphics2d.registerTextEffect('ghost', function(glyph, attributes)
    local speed = attributes.speed or 2
    local alpha = 0.5 + 0.5 * math.sin(glyph.time * speed + glyph.index)
    glyph.color = string.format('#%02XFFFFFF', math.floor(alpha * 255))
    glyph.offsetY = glyph.offsetY - alpha * 4
end)

local whisper = graphics2d.newRichText('Did you hear [ghost speed=3]that[/ghost]?', {size = 32})
```

## Typewriter reveal

The typewriter reveal shows text one character at a time. Characters count in reading order: one per cluster, spaces included, and one per image or icon. A list marker shows with the first character of its item. Right-to-left text appears from the right, and a mixed line reveals every run in the order it is read, whatever its place on screen.

A reveal speed in characters per second, `revealSpeed` in the options of rich text or of `ui.richText`, shows the characters one by one as time passes. The tag `[pause=seconds]` holds the reveal before the next character, and `[speed=factor]` changes its pace inside the tag. Hidden characters keep their place, so the text never reflows while it appears, and backgrounds, underlines and strikes grow with the revealed text.

The properties `text.visibleCharacters` and `text.visibleRatio` read and set how much shows. Setting them moves a running reveal there. A dialogue box skips to the end by setting `visibleCharacters` to -1 when the player presses a button while `text.revealing` is `true`.

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local line = graphics2d.newRichText('Hello, traveler.[pause=0.6] [speed=0.5]The sea is calm today.[/speed]', {size = 32, maxWidth = 700, revealSpeed = 30})

scene.push({
    update = function(self, dt)
        line:update(dt)
        if input.keyPressed('space') and line.revealing then
            line.visibleCharacters = -1
        end
    end,
    renderUi = function(self)
        graphics2d.beginScreen()
        line:draw(290, 820)
    end,
})
```

## Drawing in 2D

The function `graphics2d.newRichText(markup, options)` makes a `RichText` that an app keeps, updates and draws every frame. The function `graphics2d.drawRichText` draws markup once, which suits text that changes every frame. The options of a `RichText` are also properties, such as `text.color`, `text.bold` or `text.family`. Setting one lays the text out again and starts its reveal over. Both functions place the top-left corner of the block at the position.

Rich text, `text::RichText`, parses BBCode markup into a document. It lays the document out with a font family into glyphs, filled boxes, images and hit areas, and caches the layout by width and scale. Every frame, its effects and its typewriter reveal work on a copy. The method `Renderer::drawRichText` hands that frame to the text painter, which turns it into instances in layers: backgrounds, glows, shadows, images, glyphs, and then underlines and strikes. Neighbouring instances of one program and texture share a draw item. So a paragraph in one font is one draw call, and each fallback font, bitmap page or image adds its own.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local family = graphics.newFontFamily({regular = assets.font('fonts/serif.ttf'), bold = assets.font('fonts/serif-bold.ttf')})
local sign = graphics2d.newRichText('[center][b]Harbor[/b]\n[size=70%][wave amp=10]Ferries at noon[/wave][/size][/center]', {family = family, size = 40, maxWidth = 400})
local camera = graphics2d.newCamera()

scene.push({
    update = function(self, dt)
        sign:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        sign:draw(-200, -300, {layer = 4})
    end,
})
```

## Rich text in the UI

A `ui.richText` node draws markup in the family of its theme font role. The function `ui.addFont(name, family)` registers a family under a name. When a theme role names it, the node gets real bold, italic and mono faces and fallbacks. The labels and other components of the role draw with the same faces and fallbacks. The node starts in the style of its role, bold or italic when the role asks for it. It draws at the size of the role, the size of its em square, like the labels of the role.

- Its links are items that take the focus. The pointer, the keyboard and gamepads move to them and activate them, each link is one stop even when it wraps, and the node reports `link` and `linkHover`.
- Its hints show as tooltips, and its images load through the UI like `ui.image`.
- Its paragraphs read in the direction of their first strong letter unless the markup sets `[p dir]`. Start and end follow the [direction of the node](../lua-api/ui.md#right-to-left-interfaces), and its text is shaped for the language of the node.

The UI draws the text of every component through the 2D renderer, from a callback of the draw list. The text draws at its place among the ImGui meshes and inside the clip of its window. Labels and buttons draw with `Renderer::drawText`, and `richText` nodes with `Renderer::drawRichText`.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local ui = require('haylen.ui')

ui.addFont('story', graphics.newFontFamily({regular = assets.font('fonts/serif.ttf'), bold = assets.font('fonts/serif-bold.ttf')}))
ui.mount(ui.card{padding = 24,
    ui.richText{id = 'quest', text = '[b]New quest:[/b] find the [url=map]lost map[/url].', revealSpeed = 40, onLink = function(event)
        print('open', event.link)
    end},
})
```

## C++

The text types live in `haylen::text` under `engine/include/haylen/text/`:

- The class `text::Font` is the interface of both kinds of font, with `text::TrueTypeFont` and `text::BitmapFont` behind it. The methods `BitmapFont::parse` and `BitmapFont::describeGrid` read BMFont files and grids.
- The method `Font::shape` shapes one run into `Font::ShapedGlyph` values. The methods `Font::layout` and `FontFamily::layout` lay plain text out into a cached `text::Layout` of glyphs, characters and lines.
- The class `text::FontFamily` selects faces and resolves fallbacks.
- The types `text::Direction` and `text::Alignment` set the direction and alignment of a `text::Style`. The style also owns their names, `Style::alignmentFromName`, `Style::directionName` and the tables behind them, for Lua, markup and GUIs.
- The method `text::RichText::parse` reads markup into a `text::RichTextDocument` of paragraphs, runs, objects, links, hints and effects. A `text::RichText` made from markup, `text::RichTextOptions` and the `text::RichTextRegistry` of effects and icons lays it out into a `text::Layout` and animates it.
- The method `graphics2d::Renderer::drawText` draws plain text with a font or a family, and `graphics2d::Renderer::drawRichText` draws the frame of this moment.
- The registry of an engine belongs to `plugins::TextPlugin`, which also registers the `bitmapFont` and `gridFont` asset types. Its `getImage` loads the images of `[img]` tags for rich text from Lua and keeps each one while frames draw it. Its `getDrawnText` keeps the rich text of every markup and options that `graphics2d.drawRichText` and `graphics2d.measureRichText` use, while frames keep using it.

```cpp
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/plugins/TextPlugin.hpp"
#include "haylen/text/RichText.hpp"

const std::shared_ptr<haylen::text::RichTextRegistry>& registry = engine.getPlugin<haylen::plugins::TextPlugin>().getRegistry();
registry->registerEffect("blink", [](haylen::text::Effect::Glyph& glyph, const haylen::text::Effect::Parameters& parameters) {
    glyph.visible = static_cast<int>(glyph.time * parameters.getNumber("rate", 2.0F)) % 2 == 0;
});
haylen::text::RichText banner("[b]Night 3[/b] [blink]begins[/blink]", {.family = registry->getDefaultFamily(), .size = 48.0F}, registry);
banner.update(deltaSeconds);
engine.getRenderer2D().drawRichText(banner, {48.0F, 48.0F});

haylen::text::FontFamily world({.regular = engine.getDefaultFont(), .fallbacks = {arabicFont, devanagariFont}});
const haylen::text::Style arabic{.size = 32.0F, .maxWidth = 600.0F, .direction = haylen::text::Direction::RightToLeft, .language = "ar"};
const std::shared_ptr<const haylen::text::Layout> laid = world.layout("مرحبا 42 (Harbor)", arabic);
engine.getRenderer2D().drawText(world, "مرحبا 42 (Harbor)", {48.0F, 160.0F}, arabic);
```

## Limits

- Bitmap fonts draw one glyph per code point, so Arabic, Indic and other scripts that join or combine their letters need a TrueType or OpenType font.
- Lao, Khmer and Burmese lines break only at spaces and punctuation, since the engine has a phrase model for Thai alone.
- Every line is shaped as part of its paragraph and never shaped again after it wraps. So a letter at the end of a wrapped line keeps the joining form it had before the break.
- Glyphs draw from their outlines, so emoji come from a fallback font that holds them as outlines, such as Noto Emoji, in the color of the text. The images and color layers of color emoji fonts do not show. An emoji sequence joined by zero-width joiners stays one character, which draws as one glyph only where the font has a ligature for it.
- Vertical text and the ruby annotations of Japanese are not laid out.
- Text has no tab stops, so a tab takes no room, and columns of text belong in a `[table]` or in separate draws.
- Text drawn by `haylen.imgui` goes through Dear ImGui, which neither shapes nor orders it, so debug windows show complex scripts unshaped.
