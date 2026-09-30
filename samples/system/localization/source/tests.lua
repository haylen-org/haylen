-- The tests of the sample in menu order. Each module returns a scene class that takes its entry.
return {
    {id = 'switch', title = 'Switching languages', description = 'A title screen whose every text changes the moment the language does.', module = 'tests.switch'},
    {id = 'arguments', title = 'Arguments', description = 'Names, numbers and booleans in placeholders, and escaped braces.', module = 'tests.arguments'},
    {id = 'plurals', title = 'Plurals', description = 'The "zero", "one" and "other" forms picked by count, with each language\'s rules.', module = 'tests.plurals'},
    {id = 'nested', title = 'Nested keys', description = 'Groups of keys in the language files, read with dotted keys.', module = 'tests.nested'},
    {id = 'fallback', title = 'Fallback language', description = 'Keys a language lacks, taken from the fallback language, and keys no language has.', module = 'tests.fallback'},
    {id = 'best-match', title = 'Best match', description = 'The device language and other tags matched to the languages of the app.', module = 'tests.best-match'},
    {id = 'fonts', title = 'Fonts per language', description = 'One font family whose fallback draws Japanese in every component.', module = 'tests.fonts'},
    {id = 'relayout', title = 'Layout follows the text', description = 'Buttons and paragraphs that measure again when the language changes.', module = 'tests.relayout'},
    {id = 'direction', title = 'Right-to-left interface', description = 'Arabic mirrors the whole interface, and nodes with a direction of their own keep it.', module = 'tests.direction'},
}
