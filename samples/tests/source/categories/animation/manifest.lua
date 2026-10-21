-- The tests of the category in menu order.
return {
    prefix = 'ANI',
    title = 'Animation',
    description = 'Frame data with boxes, points and events, animated batches, state graphs and skeletons.',
    tests = {
        {code = 'ANI-001', title = 'Frame data', description = 'A punch from an atlas whose hitbox, hand and pivot follow the flips, turns and speed of the fighter, with frame events and a version on ticks.', module = 'frame-data'},
        {code = 'ANI-002', title = 'Animated crowd', description = 'Up to 8,000 fighters of one animated batch punching at their own speeds, advanced and drawn with one call each, with waves of jabs.', module = 'crowd'},
        {code = 'ANI-003', title = 'State graph', description = 'A walker whose idle, run and attack clips a state graph picks from its speed, facing and an attack trigger, with the graph lit as it runs.', module = 'state-graph'},
        {code = 'ANI-004', title = 'Skeletons', description = 'A cutout puppet that crossfades walk and idle, waves on a second track, flaps a cape mesh, reaches for the pointer and swaps skins.', module = 'skeleton'},
    },
}
