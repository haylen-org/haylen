-- The tests of the sample in menu order. Each module returns a scene class that takes its entry.
return {
    {id = 'sizes', title = 'TrueType and OpenType sizes', description = 'A TrueType and an OpenType font from 12 to 96 units, and one word zoomed from 8 to 320, all from one distance field atlas.', module = 'tests.sizes'},
    {id = 'sdf-effects', title = 'Outline, shadow and glow', description = 'Outlines, shadows with blur and glows drawn from the distance field of the font.', module = 'tests.sdf-effects'},
    {id = 'families', title = 'Font families', description = 'A family with real bold, italic and mono faces next to families whose bold and italic are synthesized.', module = 'tests.families'},
    {id = 'fallback', title = 'Fallback fonts', description = 'Japanese, Chinese and symbols drawn by fallback fonts that the family resolves character by character.', module = 'tests.fallback'},
    {id = 'bitmap', title = 'Bitmap fonts', description = 'A BMFont in the text format, a colored BMFont in the binary format and a grid font of LCD digits.', module = 'tests.bitmap'},
    {id = 'alignment', title = 'Alignment', description = 'Left, center, right and fill alignment, and the anchor point of a text block.', module = 'tests.alignment'},
    {id = 'wrapping', title = 'Wrapping', description = 'Words that wrap at a width, lines that break between Chinese and Japanese characters, long words and line spacing.', module = 'tests.wrapping'},
    {id = 'rich-text', title = 'Rich text tags', description = 'Every tag of the markup: styles, colors, sizes, outlines, links, hints, images, icons, paragraphs, lists, rules, tables and drop caps.', module = 'tests.rich-text'},
    {id = 'typewriter', title = 'Effects and typewriter', description = 'The built-in effects with their attributes, and a dialogue revealed like a typewriter with pauses and speed changes.', module = 'tests.typewriter'},
    {id = 'custom-effects', title = 'Custom effects', description = 'Text effects and inline icons registered from Lua.', module = 'tests.custom-effects'},
    {id = 'measuring', title = 'Measuring text', description = 'Text sizes, glyph quads, ascent, baseline and line height, glyph metrics and kerning, and rich text layouts.', module = 'tests.measuring'},
}
