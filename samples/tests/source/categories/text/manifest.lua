-- The tests of the category in menu order.
return {
    prefix = 'TXT',
    title = 'Text and fonts',
    description = 'TrueType, OpenType and bitmap fonts, families and fallbacks, complex scripts, wrapping, rich text, effects and measuring.',
    tests = {
        {code = 'TXT-001', title = 'TrueType and OpenType sizes', description = 'A TrueType and an OpenType font from 12 to 96 units, and one word zoomed from 8 to 320, all from one distance field atlas.', module = 'sizes'},
        {code = 'TXT-002', title = 'Outline, shadow and glow', description = 'Outlines, shadows with blur and glows drawn from the distance field of the font.', module = 'sdf-effects'},
        {code = 'TXT-003', title = 'Font families', description = 'A family with real bold, italic and mono faces next to families whose bold and italic are synthesized.', module = 'families'},
        {code = 'TXT-004', title = 'Fallback fonts', description = 'Japanese, Chinese and symbols drawn by fallback fonts that the family resolves character by character.', module = 'fallback'},
        {code = 'TXT-005', title = 'Bitmap fonts', description = 'A bitmap font in the text format, a colored bitmap font in the binary format and a grid font of LCD digits.', module = 'bitmap'},
        {code = 'TXT-006', title = 'Alignment', description = 'Left, center, right and fill alignment, and the anchor point of a text block.', module = 'alignment'},
        {code = 'TXT-007', title = 'Complex scripts and right-to-left text', description = 'Arabic, Persian, Urdu, Hebrew, Hindi and Thai, right-to-left paragraphs with English and numbers inside, wrapping in every script and a right-to-left typewriter.', module = 'complex-scripts'},
        {code = 'TXT-008', title = 'Wrapping', description = 'Words that wrap at a width, lines that break between Chinese and Japanese characters, long words and line spacing.', module = 'wrapping'},
        {code = 'TXT-009', title = 'Rich text tags', description = 'Every tag of the markup: styles, colors, sizes, outlines, links, hints, images, icons, paragraphs, lists, rules, tables and drop caps.', module = 'rich-text'},
        {code = 'TXT-010', title = 'Effects and typewriter', description = 'The built-in effects with their attributes, and a dialogue revealed like a typewriter with pauses and speed changes.', module = 'typewriter'},
        {code = 'TXT-011', title = 'Custom effects', description = 'Text effects and inline icons registered from Lua.', module = 'custom-effects'},
        {code = 'TXT-012', title = 'Measuring text', description = 'Text sizes, glyph quads, ascent, baseline and line height, glyph metrics, shaped advances with kerning, and rich text layouts.', module = 'measuring'},
    },
}
