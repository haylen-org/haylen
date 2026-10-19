-- The tests of the category in menu order.
return {
    prefix = 'CMP',
    title = 'Components',
    description = 'Small parts of a game written in Lua, each in one file that copies as it is into another project, with a test that shows it working.',
    tests = {
        {code = 'CMP-001', title = 'Label up', description = 'Damage, criticals, heals, gold and misses that pop out of a target, rise, stack and fade, from one pool that allocates nothing per frame.', module = 'label-up-test'},
        {code = 'CMP-002', title = 'Health bar', description = 'Bars that ease to the health with a trail of the health just lost, colors by the share left, ticks, a shield, a flash on hits and a bar that hides while it is full.', module = 'health-bar-test'},
        {code = 'CMP-003', title = 'Entities, components and systems', description = 'Hundreds of balls whose behavior comes only from their components and the systems that run over them, with systems switched off and components taken away while it runs.', module = 'ecs-test'},
        {code = 'CMP-004', title = 'Composition', description = 'Four entities built from the same small behaviors, which follow, wander, circle, bob, shoot and take hits, with behaviors given to an entity and taken away while it runs.', module = 'composition-test'},
        {code = 'CMP-005', title = 'Charts', description = 'Line, area, bar, stacked bar, pie, donut, scatter, radar, histogram, candlestick and heat map charts with data that is empty, negative, huge, missing, a single value or all equal.', module = 'charts-test'},
    },
}
