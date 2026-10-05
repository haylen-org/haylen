-- The tests of the category in menu order.
return {
    prefix = 'ORI',
    title = 'Orientation',
    description = 'The orientation of the screen with its events and its lock, a layout that adapts to the shape of the screen, and the scaling of the design resolution.',
    tests = {
        {code = 'ORI-001', title = 'Orientation and its event', description = 'The orientation of the screen, the size of the window and the events that announce every turn.', module = 'current'},
        {code = 'ORI-002', title = 'Locking the orientation', description = 'Locking the screen in portrait, in landscape or letting it turn, and where each platform allows it.', module = 'lock'},
        {code = 'ORI-003', title = 'Adaptive layout', description = 'A screen that stacks its parts in portrait and places them side by side in landscape.', module = 'adaptive'},
        {code = 'ORI-004', title = 'Design resolution and scaling', description = 'How "fit", "fill", "stretch", "expand", "pixelPerfect", "fitWidth" and "none" map the design resolution onto screens of every shape.', module = 'scaling'},
    },
}
