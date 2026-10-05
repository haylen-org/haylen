-- The tests of the category in menu order.
return {
    prefix = 'LOC',
    title = 'Localization',
    description = 'Translated text in six languages with arguments, plurals, nested keys, a fallback language, the best match for the device, fonts per language and right-to-left layouts.',
    tests = {
        {code = 'LOC-001', title = 'Switching languages', description = 'A title screen whose every text changes the moment the language does.', module = 'switch'},
        {code = 'LOC-002', title = 'Arguments', description = 'Names, numbers and booleans in placeholders, and escaped braces.', module = 'arguments'},
        {code = 'LOC-003', title = 'Plurals', description = 'The "zero", "one" and "other" forms picked by count, with the rules of each language.', module = 'plurals'},
        {code = 'LOC-004', title = 'Nested keys', description = 'Groups of keys in the language files, read with dotted keys.', module = 'nested'},
        {code = 'LOC-005', title = 'Fallback language', description = 'Keys a language lacks, taken from the fallback language, and keys no language has.', module = 'fallback'},
        {code = 'LOC-006', title = 'Best match', description = 'The device language and other tags matched to the languages of the app.', module = 'best-match'},
        {code = 'LOC-007', title = 'Fonts per language', description = 'One font family whose fallbacks draw Japanese in every component.', module = 'fonts'},
        {code = 'LOC-008', title = 'Layout follows the text', description = 'Buttons and paragraphs that measure again when the language changes.', module = 'relayout'},
        {code = 'LOC-009', title = 'Right-to-left interface', description = 'Arabic mirrors the whole interface, and nodes with a direction of their own keep it.', module = 'direction'},
    },
}
