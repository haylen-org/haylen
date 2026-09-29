-- The tests of the sample in menu order. Each module returns a scene class that takes its entry.
return {
    {id = 'values', title = 'Keys and values', description = 'Dotted keys set, read, removed, saved and loaded, next to the file they are saved in.', module = 'tests.values'},
    {id = 'engine', title = 'Engine settings', description = 'Bus volumes, mutes, fullscreen and the action map, captured into preferences and applied back.', module = 'tests.engine'},
    {id = 'settings', title = 'Settings screen', description = 'A complete settings screen whose choices persist across restarts, in three languages.', module = 'tests.settings'},
    {id = 'reset', title = 'Reset to defaults', description = 'Every stored value next to its default, and the way back to the defaults.', module = 'tests.reset'},
}
