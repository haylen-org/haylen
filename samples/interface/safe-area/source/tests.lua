-- The tests of the sample in menu order. Each module returns a scene class that takes its entry.
return {
    {id = 'anchors', title = 'Anchors', description = 'The 16 anchor presets with margins, against the safe area and against the whole screen.', module = 'tests.anchors'},
    {id = 'edge-to-edge', title = 'Edge to edge', description = 'The app draws under the notch, the rounded corners and the home indicator while the controls stay in the safe area.', module = 'tests.edge-to-edge'},
    {id = 'debug-overlay', title = 'Debug overlay', description = 'The debug view that shades what lies outside the safe area and prints its insets.', module = 'tests.debug-overlay'},
    {id = 'simulations', title = 'Device simulations', description = 'An iPhone with a notch or a dynamic island, an iPad, an Android phone with a gesture bar, a TV and custom insets, switched while the app runs.', module = 'tests.simulations'},
}
