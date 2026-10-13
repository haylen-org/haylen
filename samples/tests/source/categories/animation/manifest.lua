-- The tests of the category in menu order.
return {
    prefix = 'ANI',
    title = 'Animation',
    description = 'Frame data with boxes, points and events, animated batches, state graphs and skeletons.',
    tests = {
        {code = 'ANI-001', title = 'Frame data', description = 'A punch from an atlas whose hitbox, hand and pivot follow the flips, turns and scale of the fighter, with frame events and a version that steps by ticks.', module = 'frame-data'},
        {code = 'ANI-002', title = 'Animated crowd', description = 'Thousands of fighters of one animated batch punching at their own speeds, advanced and drawn with one call each, with waves of jabs that end on their own.', module = 'crowd'},
        {code = 'ANI-003', title = 'State graph', description = 'A walker whose idle, run and attack clips a state graph picks from its speed, facing and an attack trigger, with directional states and the states it entered.', module = 'state-graph'},
        {code = 'ANI-004', title = 'Skeletons', description = 'A cutout puppet with walk and idle crossfading, a wave on a second track, a cape mesh that flaps, an arm that reaches for the pointer, skins and footstep events.', module = 'skeleton'},
    },
}
