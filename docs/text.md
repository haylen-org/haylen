# Text

Haylen draws text from fonts of two kinds and styles it with BBCode markup. This guide explains fonts, font families and bitmap fonts, how text in every script is shaped, ordered in its direction and broken into lines, the markup of rich text with its effects and typewriter reveal, and how 2D drawing and the UI show it. The [`haylen.graphics` reference](lua-api/graphics.md#font) lists the font API, the [`haylen.graphics2d` reference](lua-api/graphics2d.md#richtext) the rich text API, and the [`haylen.ui` reference](lua-api/ui.md#uirichtextproperties) the `richText` component.

## Fonts

A TrueType or OpenType font renders through a signed distance field, a single-channel atlas of how far every pixel lies from the edge of a glyph. One atlas serves every size, and the text shader draws outlines, bolder strokes, blurred shadows and glows from the same field. A text size is the height of the em square of the font, the square its designer draws every glyph in, so faces and fallback fonts of one size line up whatever their proportions, and a line is as tall as the ascent, descent and line gap of the font at that size. The field reaches `spread` pixels past a glyph at the bake size, 8 by default, and that reach bounds every one of those effects: at a text size of 24 and the default bake size of 48, an outline, a glow and a blur together reach at most 4 pixels, a little less when small text smooths its edges over more of the field. They shrink together beyond that reach, so the box of a glyph never shows, and a font loaded with a larger `spread` reaches further. Outlined text draws every outline first and every fill over them, so a wide outline never covers the letters beside it. The bake size, the spread and the initial `atlasSize` must be positive and at most the maximum texture size of the device, since no atlas could hold a larger glyph. The atlas grows by doubling as new glyphs are used, and a grown atlas is a new texture, so text drawn earlier in the same frame keeps the image it was measured on.

Small text looks sharpest when its glyphs start on whole pixels, since a baseline between two pixel rows spreads every horizontal stroke over both. The style option `pixelSnap` moves a block of plain text so its left edge lands on a pixel column of the destination and puts the baseline of every line on a pixel row, in whatever units the canvas uses and at any zoom, so text that moves steps from pixel to pixel instead of shimmering. Text that turns, or that draws in a canvas whose view turns, keeps its exact place, because no pixel row runs along its baselines.

A bitmap font draws prepared images, the pages of a BMFont file from tools such as BMFont, Hiero or Littera, or an image of equal cells. It draws pixel for pixel at its native size and scales at other sizes, keeps the colors of its images, which the text color multiplies, and draws nothing for the characters it lacks. It has no distance field, so it takes no outline, glow or blur, and its shadow is the silhouette of its glyphs in the shadow color. Load BMFont files with nearest filtering for crisp pixel art.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local title = assets.font('fonts/title.ttf', {bakeSize = 64, spread = 12})
local pixel = assets.font('fonts/pixel.fnt', {filter = 'nearest'})
local digits = assets.load('fonts/digits.png', 'gridFont', {characters = '0123456789', cellWidth = 12, cellHeight = 16, filter = 'nearest'})

graphics2d.drawText(title, 'Island', 40, 40, {size = 96, outlineWidth = 6, outlineColor = '#FF402000'})
graphics2d.drawText(pixel, 'PRESS START', 40, 200, {size = pixel.nativeSize * 2})
graphics2d.drawText(digits, '1200', 40, 280, {size = 32, shadowOffset = {2, 2}, shadowColor = '#FF000000'})
```

Every function that takes a font takes either kind: `graphics2d.drawText`, `measureText`, rich text families and Tiled text objects. The functions `graphics2d.drawText` and `measureText` also take a font family, which draws text in every script its fonts cover.

## Font families

A family groups the faces of one typeface, regular, bold, italic, bold italic and mono, with fallback fonts for the characters its faces lack, such as an Arabic, Hebrew, Devanagari or Thai font, a CJK font for Chinese and Japanese or a symbol font for arrows and stars. Layout picks the face of each run from its style and each character, a letter with its marks as one unit, from the first font that has every code point of it: the face, then each fallback in order, and the face again when none has it, which draws its missing glyph box. Spaces, punctuation and digits keep the font of the text before them, so an Arabic sentence stays in the Arabic font up to its full stop. The fonts of the Noto family cover every script with a consistent look, and a fallback per script the game shows is enough.

A style the family has no face for is synthesized from the face it has. A distance field face grows its strokes on both sides by 3 percent of the text size and widens its advance to match, and leans italic glyphs by one fifth of their height around the baseline in the vertex shader. A bitmap face draws a bold glyph a second time one native pixel to the right and leans italic glyphs the same way. A fallback font synthesizes the bold and italic its run asks for. The tag `[code]` uses the mono face, or the regular faces when the family has none.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local story = graphics.newFontFamily({
    regular = assets.font('fonts/serif.ttf'),
    bold = assets.font('fonts/serif_bold.ttf'),
    italic = assets.font('fonts/serif_italic.ttf'),
    mono = assets.font('fonts/mono.ttf'),
    fallbacks = {assets.font('fonts/cjk.ttf')},
})
local face, syntheticBold, syntheticItalic = story:select({bold = true, italic = true})
```

The distance field of a glyph comes from its whole outline, the quadratic curves of TrueType fonts and the cubic curves of the `CFF` outlines of many `.otf` files alike, so both kinds draw cleanly at every size. Contours that overlap, as in the glyphs of many variable fonts, keep their shared inside whole.

## Scripts and directions

Every run of text is shaped by HarfBuzz before it is laid out: runs of one font, one script, one direction and one style shape together, with the whole paragraph around them as context. Shaping applies the OpenType features of the font, so Latin text takes its ligatures and kerning, Arabic, Persian and Urdu letters take the form their neighbours ask for and join, lam and alef merge, Devanagari consonants form conjuncts and place their vowel signs before or above them, and the marks of Thai, Hebrew and every other script sit on their letters. The `language` of a style or of rich text, a BCP 47 tag such as `'ar'`, `'fa'`, `'ur'`, `'hi'` or `'sr'`, picks the forms a language prefers where the font has them, such as the Urdu forms of some digits or the Serbian forms of some Cyrillic letters.

A character is a cluster, the code points shaping keeps together: a letter with its marks, a conjunct with its vowel signs, or a ligature. Characters are what the typewriter reveal counts, what text fields move the caret over, and what hit tests and selections cover, so a reveal or a caret never stops inside a letter.

Every paragraph runs through the Unicode Bidirectional Algorithm of SheenBidi. Its direction is `'auto'` by default, taken from its first strong letter, so an Arabic or Hebrew paragraph reads right to left and an English one left to right, and `direction = 'leftToRight'` or `'rightToLeft'` forces it. After a paragraph is broken into lines, every line is ordered for display on its own: runs that read right to left go from the right, numbers and Latin words inside them keep their own order, and brackets mirror so that `(USD)` reads correctly inside Arabic text. A paragraph that starts with a Latin word or a number but reads right to left needs a forced direction, or a right-to-left mark, U+200F, at its start.

Alignment names the sides of a paragraph by its direction. The alignment `'start'`, the default, lines text up where its lines begin, the left of left-to-right text and the right of right-to-left text, and `'end'` the other side. The alignments `'left'`, `'center'` and `'right'` name fixed sides, and `'fill'` stretches every wrapped line to both edges and leaves the last line at its start. In rich text, `[p dir=rightToLeft]` sets the direction of a block and `[p align=end]` its alignment, the indent, the list markers and the drop cap stand on the side the paragraph starts, and a table reads in the direction of its first paragraph, so its first column stands at the right of a right-to-left table. Backgrounds, underlines, strikes and link areas cover the shaped text after it is ordered, split where the direction changes, and grow with the reveal from the side their text starts.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local family = graphics.newFontFamily({
    regular = graphics2d.defaultFont(),
    fallbacks = {assets.font('fonts/noto_sans_arabic_regular.ttf'), assets.font('fonts/noto_sans_hebrew_regular.ttf'), assets.font('fonts/noto_sans_devanagari_regular.ttf'), assets.font('fonts/noto_sans_thai_regular.ttf')},
})

graphics2d.drawText(family, 'السعر 42 دولارًا (USD)', 1000, 100, {size = 32, anchor = {1, 0}})
graphics2d.drawText(family, 'Hebrew: שלום עולם', 100, 160, {size = 32})
graphics2d.drawText(family, 'Order 7 السعر', 1000, 220, {size = 32, direction = 'rightToLeft', anchor = {1, 0}})
local story = graphics2d.newRichText('[p dir=rightToLeft align=start][b]الرحلة[/b] بدأت في [u]الصباح[/u] الباكر.[/p]', {family = family, size = 30, maxWidth = 500, language = 'ar', revealSpeed = 20})
```

A bitmap font has no shaping tables, so it maps every code point to its own glyph and applies the kerning pairs of its file. It lays out and orders right-to-left text, with mirrored brackets, but it draws neither joined Arabic letters nor Indic conjuncts, which need a TrueType or OpenType font.

## Line breaking

Lines break where the Unicode Line Breaking Algorithm of libunibreak allows: after spaces and hyphens, between Chinese and Japanese characters, and never before a closing punctuation mark or inside a number. Every paragraph separator ends a paragraph: a line feed, a carriage return, CRLF, which counts as one, next line (U+0085), the paragraph separator U+2029 and the information separators U+001C to U+001E. A line separator (U+2028), a vertical tab or a form feed breaks the line inside its paragraph. Tabs and the other control characters draw nothing and take no room. The language tailors the quotation marks of English, German, Spanish, French and Russian text and the ambiguous punctuation of Chinese text. Thai writes no spaces between words, so a line of Thai breaks between the phrases the Thai model of BudouX finds, a small model that weighs the letters around every place of the text. A word wider than a whole line breaks between its characters, never inside a cluster. Lao, Khmer and Burmese also write without spaces, and their lines break only at spaces and punctuation because the engine has no model for them.

## Markup

Markup is text with tags in square brackets. Inline tags open with `[name]` or `[name=value attribute=value]` and close with `[/name]`, in reverse order of opening. Values and attributes may be quoted to hold spaces, and the values of `[url]` and `[hint]` run to the closing bracket. The tags `[lb]` and `[rb]` write `[` and `]`. A newline ends a paragraph, whether the markup ends its lines with LF, CRLF or any other [paragraph separator](#line-breaking), and `[br]` breaks a line without ending it. Colors are names such as `red`, `gold` or `transparent`, `#RRGGBB` or `#AARRGGBB`, like the rest of the engine. Sizes are pixels at a scale of 1.

| Tag | Meaning |
| --- | --- |
| `[b]`, `[i]`, `[u]`, `[s]` | Bold, italic, underline and strike. |
| `[code]` | The mono face of the family. |
| `[color=c]`, `[bgcolor=c]` | Text color and a background behind the text. |
| `[font=name]` | A font family that the host names: the `fonts` option in 2D and the fonts of `ui.addFont` in the UI. |
| `[size=24]`, `[size=150%]` | An absolute size, or a size relative to the text around it. |
| `[outline=3 color=c]` | An outline 3 pixels wide, black by default. |
| `[shadow=2,3 color=c blur=2]` | A shadow at an offset, 2,2 and half transparent black by default, softened by the blur. |
| `[glow=6 color=c]` | A glow that fades out 6 pixels past the text, white by default. |
| `[alpha=0.5]` | Multiplies the opacity of the text, images and icons inside. |
| `[url=payload]`, `[url]text[/url]` | A link, underlined unless the host turns that off, whose payload hit tests and the UI report. Without a payload the text is the payload. |
| `[hint=text]` | A hint the UI shows as a tooltip and `text:hintAt` reports. |
| `[img=path width=24 height=24 region=x,y,w,h color=c valign=center]` | An inline image of the package content, which keeps the shape of its region when only one side is given. Rich text that draws every frame, such as `graphics2d.drawRichText`, loads each image once and keeps it while frames draw it. |
| `[icon=name width= height= color= valign=]` | An icon that `graphics2d.registerTextIcon` registered, as tall as its text unless sized. |
| `[pause=0.5]` | Holds the typewriter reveal for half a second before the next character. |
| `[speed=2]` | Reveals the text inside twice as fast. |
| `[p align=center dir=rightToLeft indent=1]` | A paragraph block with an alignment (`start`, `end`, `left`, `center`, `right` or `fill`), a direction (`auto`, `leftToRight` or `rightToLeft`) and an indent in levels of one and a half times the base size, which stands on the side the paragraph starts. |
| `[center]`, `[left]`, `[right]`, `[fill]` | Paragraph blocks with an alignment. Fill stretches the spaces of every wrapped line to both edges. |
| `[ul bullet=*]`, `[ol type=1]` | Lists, where every paragraph inside is an item with a bullet or a number, counted as `1`, `a`, `A`, `i` or `I`. Lists nest and indent one level each. |
| `[hr width=50% height=2 color=c]` | A horizontal rule, centered unless its block aligns it. |
| `[table=3]` with `[cell bg=c border=c padding=4]` | A table of cells read row by row. Columns take the width their content wants and shrink proportionally to the room of their widest word when the table is too wide. |
| `[dropcap size=64 font=name color=c margin=4]W[/dropcap]` | A large first letter that the lines beside it flow around, three times the base size unless sized. It must start its paragraph. |

Block tags end the paragraph before them, and a newline right after an opening block tag or right before a closing one belongs to the markup, so blocks read naturally on lines of their own. Empty lines inside a list take no number.

```lua
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
```

Any other tag name runs a text effect of that name. Malformed markup raises an error that names its line and column, counted from 1, such as `Rich text markup at line 2, column 6: The tag "[/b]" closes "[i]", which is still open.`, `The tag "[b]" is never closed.`, `The value "blurple" is not a color.` or `The markup "[wiggle]" is neither a tag nor a registered text effect.`.

## Effects

Effects animate the glyphs of their tag every frame, changing their offset, color and visibility. Rich text keeps the laid out glyphs and applies the effects to a copy, so the time alone decides where a glyph is, and the same time always gives the same picture. Effects nest, and the outer one runs first. The built-in effects are:

| Effect | Attributes | Motion |
| --- | --- | --- |
| `[wave]` | `amp` (20), `freq` (5) | Glyphs bob up and down in a wave along the line. |
| `[shake]` | `rate` (20), `level` (5) | Glyphs jump to a new random offset `rate` times a second. |
| `[tornado]` | `radius` (10), `freq` (1) | Glyphs circle around their place. |
| `[fade]` | `start` (0), `length` (10) | Glyphs fade out over `length` characters from the character `start` of the tag. |
| `[rainbow]` | `freq` (1), `sat` (0.8), `val` (0.8), `speed` (1) | Glyphs cycle through the hues. |
| `[pulse]` | `freq` (1), `color` (`#40FFFFFF`), `ease` (-2) | Glyphs pulse toward their color multiplied by `color`. |

Apps register their own effects from Lua with `graphics2d.registerTextEffect(name, effect)`, whose function receives each glyph and the attributes of the tag, and from C++ with `text::RichTextRegistry::registerEffect`, whose function receives a `text::Effect::Glyph` and its `text::Effect::Parameters`. An effect sees the index of the glyph inside its tag, which counts every character from the first one of the tag, spaces and images included, its character in the whole text, its pen position on the baseline and the time. Effects run while the text builds the picture of the moment, so an effect cannot change or lay out the text it runs on: setting its markup, options, width or scale, updating it or measuring it from inside the effect raises `A text effect cannot change or lay out the rich text it runs on.`.

```lua
local graphics2d = require('haylen.graphics2d')

graphics2d.registerTextEffect('ghost', function(glyph, attributes)
    local speed = attributes.speed or 2
    local alpha = 0.5 + 0.5 * math.sin(glyph.time * speed + glyph.index)
    glyph.color = string.format('#%02XFFFFFF', math.floor(alpha * 255))
    glyph.offsetY = glyph.offsetY - alpha * 4
end)
```

## Typewriter reveal

Characters count in reading order, one per cluster, spaces included, and one per image or icon, and a list marker shows with the first character of its item. Right-to-left text appears from the right, and a mixed line reveals every run in the order it is read, whatever its place on screen. A reveal speed in characters per second, `reveal` in the options of rich text or of `ui.richText`, shows them one by one as time passes. The tag `[pause=seconds]` holds the reveal before the next character and `[speed=factor]` changes its pace inside the tag. Hidden characters keep their place, so the text never reflows while it appears, and backgrounds, underlines and strikes grow with the revealed text.

The properties `text.visibleCharacters` and `text.visibleRatio` read and set how much shows. Setting them moves a running reveal there, and a dialogue box skips to the end by setting `visibleCharacters` to -1 when the player presses a button while `text.revealing` is `true`.

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

## Layout

Rich text lays each paragraph out into lines. Lines wrap where the [line breaking](#line-breaking) rules allow and around images, and inside a word only when the word alone is wider than a line. Every line stands on one baseline, as tall as its tallest text and images, and lines follow each other by their height times the line spacing, so a line with a larger size or an image makes room for it. Images centered by default sit on the middle of the text of their style, `baseline` puts their bottom on the baseline, and `top` and `bottom` align them with the line.

A layout is cached by its width and scale, and a few widths stay cached, so a UI container that measures text at one width and draws it at another lays it out once for each, and `text:size(maxWidth)` measures at another width the same way. Changing the markup or the options lays it out again, and effects and the reveal never do. An image that is still loading takes no room, and the layout is built again once it arrives.

Plain text lays out the same way, as a document of one paragraph per line, where the carriage return of CRLF stays at the end of its line and draws nothing, so the characters of the layout keep counting the code points of the text. Every font and every family keeps its 512 most recent plain text layouts, by the text and the style fields that change the layout: size, wrap width, line spacing, alignment, direction, language, bold and italic. The size, the wrap width and the line spacing must be finite numbers. Text that `graphics2d.drawText` draws every frame, and the labels of the UI, shape once and then only place their glyphs. Color, outline, shadow, anchor and rotation apply when the text draws and never lay it out again.

## Drawing in 2D

The function `graphics2d.newRichText(markup, options)` makes a `RichText` that an app keeps, updates and draws every frame, and `graphics2d.drawRichText` draws markup once, which suits text that changes every frame. The options of a `RichText` are also properties, such as `text.color`, `text.bold` or `text.family`, and setting one lays the text out again and starts its reveal over. Both place the top-left corner of the block at the position. The renderer draws rich text in layers, backgrounds, glows, shadows, images, glyphs and then underlines and strikes, and neighbouring glyphs of one font share a draw call.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local assets = require('haylen.assets')
local scene = require('haylen.scene')

local family = graphics.newFontFamily({regular = assets.font('fonts/serif.ttf'), bold = assets.font('fonts/serif_bold.ttf')})
local sign = graphics2d.newRichText('[center][b]Harbor[/b]\n[size=70%][wave amp=10]Ferries at noon[/wave][/size][/center]', {family = family, size = 40, maxWidth = 400})

scene.push({
    update = function(self, dt)
        sign:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(graphics2d.newCamera())
        sign:draw(-200, -300, {layer = 4})
    end,
})
```

## Rich text in the UI

A `ui.richText` node draws markup in the family of its theme font role, which `ui.addFont(name, family)` gives real bold, italic and mono faces and fallbacks when a theme role names it, and the labels and other components of the role draw with the same faces and fallbacks. It starts in the style of its role, bold or italic when the role asks for it, and draws at the em size that lines its letters up with the labels of the role, since the widgets size a font by its height from ascent to descent. Its links are focusable items: the pointer, the keyboard and gamepads move to them and activate them, each link is one stop even when it wraps, and the node reports `link` and `linkHover`. Its hints show as tooltips, its images load through the UI like `ui.image`, and its text draws through the 2D renderer at its place among the other UI draws, inside the clip of its window. Its paragraphs read in the direction of their first strong letter unless the markup sets `[p dir]`, start and end follow the [direction of the node](lua-api/ui.md#right-to-left-interfaces), and its text is shaped for the language of the node.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local ui = require('haylen.ui')

ui.addFont('story', graphics.newFontFamily({regular = assets.font('fonts/serif.ttf'), bold = assets.font('fonts/serif_bold.ttf')}))
ui.mount(ui.card{padding = 24,
    ui.richText{id = 'quest', text = '[b]New quest:[/b] find the [url=map]lost map[/url].', revealSpeed = 40, onLink = function(event)
        print('open', event.link)
    end},
})
```

## C++

The text types live in `haylen::text` under `engine/include/haylen/text/`. The class `text::Font` is the interface of both kinds of font, with `text::TrueTypeFont` and `text::BitmapFont` behind it, and `BitmapFont::parse` and `BitmapFont::describeGrid` read BMFont files and grids. The method `Font::shape` shapes one run into `Font::ShapedGlyph` values, and `Font::layout` and `FontFamily::layout` lay plain text out into a cached `text::Layout` of glyphs, characters and lines. The class `text::FontFamily` selects faces and resolves fallbacks, and `text::Direction` and `text::Alignment` set the direction and alignment of a `text::Style`, which also owns their names (`Style::alignmentFromName`, `Style::directionName` and the tables behind them) for Lua, markup and GUIs. The method `text::RichText::parse` reads markup into a `text::RichTextDocument` of paragraphs, runs, objects, links, hints and effects, and a `text::RichText` made from markup, `text::RichTextOptions` and the `text::RichTextRegistry` of effects and icons lays it out into a `text::Layout` and animates it. The method `graphics2d::Renderer::drawText` draws plain text with a font or a family. The method `graphics2d::Renderer::drawRichText` draws the frame of this moment. The registry of an engine belongs to `plugins::TextPlugin`, which also registers the `bitmapFont` and `gridFont` asset types, and whose `getImage` loads the images of `[img]` tags for rich text from Lua, keeping each one while frames draw it.

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
- Every line is shaped as part of its paragraph and never shaped again after it wraps, so a letter at the end of a wrapped line keeps the joining form it had before the break.
- Glyphs draw from their outlines, so emoji come from a fallback font that holds them as outlines, such as Noto Emoji, in the color of the text. The images and color layers of color emoji fonts do not show, and an emoji sequence joined by zero-width joiners stays one character that draws as one glyph only where the font has a ligature for it.
- Vertical text and the ruby annotations of Japanese are not laid out.
- Text has no tab stops, so a tab takes no room, and columns of text belong in a `[table]` or in separate draws.
- Text drawn by `haylen.imgui` goes through Dear ImGui, which neither shapes nor orders it, so debug windows show complex scripts unshaped.
