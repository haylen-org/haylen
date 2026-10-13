-- The tests of the category in menu order.
return {
    prefix = 'ANI',
    title = 'Animation',
    description = 'Frame data with boxes, points and events, animated batches, state graphs and skeletons.',
    tests = {
        {code = 'ANI-001', title = 'Frame data', description = 'A punch from an atlas whose hitbox, hand and pivot follow the flips, turns and scale of the fighter, with frame events and a version that steps by ticks.', module = 'frame-data'},
    },
}
