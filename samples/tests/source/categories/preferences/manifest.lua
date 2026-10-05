-- The tests of the category in menu order.
return {
    prefix = 'PRF',
    title = 'Preferences',
    description = 'Choices of the player kept between sessions with "haylen.preferences": dotted keys, the settings of the engine, a settings screen in three languages and the way back to the defaults.',
    tests = {
        {code = 'PRF-001', title = 'Keys and values', description = 'Dotted keys set, read, removed, saved and loaded, next to the file they are saved in.', module = 'values'},
        {code = 'PRF-002', title = 'Engine settings', description = 'Bus volumes, mutes, fullscreen and the action map, captured into preferences and applied back.', module = 'engine'},
        {code = 'PRF-003', title = 'Settings screen', description = 'A complete settings screen whose choices persist across restarts, in three languages.', module = 'settings-screen'},
        {code = 'PRF-004', title = 'Reset to defaults', description = 'Every stored value next to its default, and the way back to the defaults.', module = 'reset'},
    },
}
