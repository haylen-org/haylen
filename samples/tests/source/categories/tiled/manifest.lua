-- The tests of the category in menu order.
return {
    prefix = 'TLD',
    title = 'Tiled',
    description = 'Tiled maps of every orientation with their layers, objects, properties, collision, spawning, y sorting, ray casts and worlds.',
    tests = {
        {code = 'TLD-001', title = 'Orientations', description = 'Orthogonal, isometric, staggered, hexagonal and oblique maps, with the cell under the pointer.', module = 'orientations'},
        {code = 'TLD-002', title = 'Infinite maps', description = 'A map stored in chunks that reach negative cells, panned with the pointer, the keys or a stick.', module = 'infinite'},
        {code = 'TLD-003', title = 'Tile animations', description = 'Water, lava, torches, coins and slimes whose tiles play their frames on map time.', module = 'animations'},
        {code = 'TLD-004', title = 'Image layers', description = 'A sky, clouds and hills that repeat across the view and scroll at their own parallax.', module = 'image-layers'},
        {code = 'TLD-005', title = 'Group layers', description = 'Nested groups that pass their offset, tint, opacity and parallax on to their layers.', module = 'groups'},
        {code = 'TLD-006', title = 'Objects and templates', description = 'Every object shape and object templates with their overrides, picked under the pointer.', module = 'objects'},
        {code = 'TLD-007', title = 'Properties', description = 'Custom properties of every type on the map, its layers, its objects and its tiles.', module = 'properties'},
        {code = 'TLD-008', title = 'Collision', description = 'Physics bodies built from tile collision shapes and collision objects, with sensors and filters.', module = 'collision'},
        {code = 'TLD-009', title = 'Spawning', description = 'Entities created from objects by factories keyed by object class.', module = 'spawning'},
        {code = 'TLD-010', title = 'Y sorting', description = 'A character walking behind and in front of trees, fences and lamps sorted by their feet.', module = 'ysort'},
        {code = 'TLD-011', title = 'Ray casts', description = 'Rays against the cells of a tile layer and the shapes of an object layer, without physics.', module = 'raycasts'},
        {code = 'TLD-012', title = 'Worlds', description = 'A world file that places listed maps and maps found by a file name pattern.', module = 'worlds'},
    },
}
