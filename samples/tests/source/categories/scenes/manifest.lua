-- The tests of the category in menu order.
return {
    prefix = 'SCN',
    title = 'Scenes and transitions',
    description = 'The scene stack with every transition effect, custom effects, loading with progress and errors, every hook, transparent overlays and the game pause.',
    tests = {
        {code = 'SCN-001', title = 'Transition gallery', description = 'Every built-in effect, with pickers for the direction, the easing, the duration and the color.', module = 'transitions'},
        {code = 'SCN-002', title = 'Custom effect', description = 'A transition effect written in Lua that draws both scenes itself.', module = 'custom-effect'},
        {code = 'SCN-003', title = 'Loading and errors', description = 'A fade as the loading screen, a custom loading view with progress, preloading and a failing load routed by "onError".', module = 'loading'},
        {code = 'SCN-004', title = 'Stack and hooks', description = 'Push, pop, replace and "popTo" with the stack and every scene hook, from "load" to "unload", shown on screen.', module = 'stack'},
        {code = 'SCN-005', title = 'Transparent overlays', description = 'Overlay scenes that let the scenes below keep rendering while only the top one updates.', module = 'overlays'},
        {code = 'SCN-006', title = 'Pause and process modes', description = 'A paused world under a working pause menu, with "pausable", "whenPaused" and "always" timers and tweens.', module = 'pause'},
        {code = 'SCN-007', title = 'Loading screen that fades out', description = 'A full-screen loading screen over a map that raises its terrain while it loads, fading out over the finished map, with the events of the change as they come.', module = 'loading-screen'},
    },
}
