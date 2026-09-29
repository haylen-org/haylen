-- The tests of the sample in menu order. Each module returns a scene class that takes its entry.
return {
    {id = 'transitions', title = 'Transition gallery', description = 'Every built-in effect, with pickers for the direction, the easing, the duration and the color.', module = 'tests.transitions'},
    {id = 'custom-effect', title = 'Custom effect', description = 'A transition effect written in Lua that draws both scenes itself.', module = 'tests.custom-effect'},
    {id = 'loading', title = 'Loading and errors', description = 'A fade as the loading screen, a custom loading view with progress, preloading and a failing load routed by onError.', module = 'tests.loading'},
    {id = 'stack', title = 'Stack and hooks', description = 'Push, pop, replace, popTo and popToRoot with the stack and every scene hook, from load to unload, shown on screen.', module = 'tests.stack'},
    {id = 'overlays', title = 'Transparent overlays', description = 'Overlay scenes that let the scenes below keep rendering while only the top one updates.', module = 'tests.overlays'},
    {id = 'pause', title = 'Pause and process modes', description = 'A paused world under a working pause menu, with pausable, whenPaused and always timers and tweens.', module = 'tests.pause'},
}
