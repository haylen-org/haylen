# Haylen Fonts

Haylen Fonts is a Lua sample of text in Haylen: TrueType and OpenType fonts drawn from one signed distance field at any size, outlines, blurred shadows and glows, font families with real and synthesized styles, fallback fonts for Chinese, Japanese and symbols, BMFont bitmap fonts in the text and binary formats and grid fonts, alignment and wrapping, complex scripts and right-to-left text, every tag and effect of rich text with a typewriter reveal, custom effects from Lua and text measurement. A menu lists one test per feature, each test is a scene with a Back button, and Escape, the east or Back button of a gamepad, the Menu button of an Apple TV remote or the Back button of an Android device returns to the menu.

| Test | What it shows |
| --- | --- |
| TrueType and OpenType sizes | Crimson Text (`.ttf`) and Fira Sans (`.otf`) from 12 to 96 units, and one word zoomed from 8 to 320 by a slider or a pulse, with the bake size and atlas pages of each font. |
| Outline, shadow and glow | The options `outlineWidth`, `shadowOffset` with `shadowBlur` and the `[glow]` tag of rich text on Lilita One, baked with a wider spread, driven by sliders, with the reach of each effect in distance field units from `font:toDistance`. |
| Font families | A family with real bold, italic and bold italic faces and a mono face, next to families of one TrueType face and one bitmap face whose styles are synthesized, with what `family:select` reports. |
| Fallback fonts | Japanese, Chinese and symbols drawn by M PLUS 1p and Noto Sans Symbols 2 as fallbacks, styles on fallback characters, a family without fallbacks and a character no font has, with `family:resolve`. |
| Bitmap fonts | Haylen Pixel as a BMFont in the text format at whole multiples of its size, tinted and with a shadow, the same font in the binary format with gold glyphs that keep their colors, synthesized bold and italic, and a clock and a counter in a grid font of LCD digits. |
| Alignment | The option `align` with `left`, `center`, `right` and `fill`, `anchor` with rotation, and rich text paragraphs with their own alignment. |
| Complex scripts and right-to-left text | Arabic, Persian, Urdu, Hebrew, Hindi and Thai drawn with `graphics2d.drawText` from a family whose Noto fallbacks HarfBuzz shapes, a mixed Arabic line with a number and English words in the direction a segmented control picks, a right-to-left rich text paragraph with bold, colored and underlined spans that wraps at a width from a slider, Thai wrapping between phrases, and a typewriter that reveals Arabic from the right. |
| Wrapping | English wrapping after spaces, Japanese breaking between characters, a word longer than its line and line spacing, with sliders and a sweeping width. |
| Rich text tags | Every tag: `b`, `i`, `u`, `s`, `code`, `color`, `bgcolor`, `alpha`, `font`, `size`, `outline`, `shadow`, `glow`, `url`, `hint`, `img`, `icon`, `lb`, `rb`, `br`, `center`, `right`, `p` with `align` and `indent`, `ul`, `ol`, `hr`, `table` with `cell` and `dropcap`, and `linkAt` and `hintAt` under the pointer. |
| Effects and typewriter | The effects `wave`, `shake`, `tornado`, `fade`, `rainbow` and `pulse` with their attributes and nested, and a dialogue revealed with `pause` and `speed` tags, skipped and replayed, at a speed from a slider. |
| Custom effects | Six effects registered with `graphics2d.registerTextEffect`, nested with a built-in one, and icons registered with `graphics2d.registerTextIcon`. |
| Measuring text | The functions `measureText` and `font:layout` with the quad under the pointer, `ascent` and `lineHeight`, `font:glyph` metrics, the kerned advances of `font:shape`, and `measureRichText` with the links and images of `text:layout`. |

## Controls

The mouse, touch, the keyboard, gamepads and TV remotes reach the controls in the panel on the right of a test, where the arrow keys, the directional pad and the Siri Remote move the focus and left and right move a focused slider. The hint line at the bottom of each test lists what it adds.

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 make.py run graphics/fonts` |
| macOS app | `python3 make.py run graphics/fonts --platform macos` |
| iPhone and iPad simulator | `python3 make.py run graphics/fonts --platform ios-simulator` |
| Apple TV simulator | `python3 make.py run graphics/fonts --platform tvos-simulator` |
| Android device or emulator | `python3 make.py run graphics/fonts --platform android` |
| Browser | `python3 make.py run graphics/fonts --platform web` |

## Package layout

```text
fonts/
  app.json               Window, design resolution of 1920 by 1080 and identifier.
  source/
    main.lua             Adds the Back button of gamepads to uiCancel and opens the menu.
    tests.lua            The tests in menu order.
    sample.lua           The frame of every test with its Back button, controls and hints, and the way back to the menu.
    fonts.lua            The fonts and families of the sample, loaded on first use.
    scenes/menu.lua      The menu.
    tests/               One scene per test.
  content/
    fonts/               The TrueType and OpenType fonts with their licenses, including Noto Sans Arabic, Hebrew, Devanagari and Thai, Haylen Pixel and the LCD digits.
    images/              The pictures of rich text and the gamepad prompts.
    CREDITS.md           Authors and licenses.
  tools/                 The generator of Haylen Pixel, the LCD digits and the pictures, which is not part of the package.
```

The command `python3 samples/graphics/fonts/tools/generate_content.py` draws Haylen Pixel, writes it as a text BMFont and as a binary BMFont with gold glyphs, and draws the LCD digits and the pictures again.
