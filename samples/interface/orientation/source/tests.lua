-- The tests of the sample in menu order. Each module returns a scene class that takes its entry.
return {
    {id = 'current', title = 'Orientation and its event', description = 'The orientation of the screen, the size of the window and the events that announce every turn.', module = 'tests.current'},
    {id = 'lock', title = 'Locking the orientation', description = 'Locking the screen in portrait, in landscape or letting it turn, and where each platform allows it.', module = 'tests.lock'},
    {id = 'adaptive', title = 'Adaptive layout', description = 'A screen that stacks its parts in portrait and places them side by side in landscape.', module = 'tests.adaptive'},
    {id = 'scaling', title = 'Design resolution and scaling', description = 'How "fit", "fill", "stretch", "expand" and "pixelPerfect" map the design resolution onto screens of every shape.', module = 'tests.scaling'},
}
