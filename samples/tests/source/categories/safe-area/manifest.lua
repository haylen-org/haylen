-- The tests of the category in menu order.
return {
    prefix = 'SAF',
    title = 'Safe area',
    description = 'The anchors of the interface against the safe area and the whole screen, drawing edge to edge, the debug overlay and the simulated devices.',
    tests = {
        {code = 'SAF-001', title = 'Anchors', description = 'The 16 anchor presets with margins, against the safe area and against the whole screen.', module = 'anchors'},
        {code = 'SAF-002', title = 'Edge to edge', description = 'The app draws under the notch, the rounded corners and the home indicator while the controls stay in the safe area.', module = 'edge-to-edge'},
        {code = 'SAF-003', title = 'Debug overlay', description = 'The debug view that shades what lies outside the safe area and prints its insets.', module = 'debug-overlay'},
        {code = 'SAF-004', title = 'Device simulations', description = 'An iPhone with a notch or a dynamic island, an iPad, an Android phone with a gesture bar, a TV and custom insets, switched while the app runs.', module = 'simulations'},
    },
}
