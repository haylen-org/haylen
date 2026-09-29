-- The tests of the sample in menu order. Each module returns the scene class of its test.
return {
    {id = 'orientations', title = 'Orientations', description = 'Orthogonal, isometric, staggered, hexagonal and oblique maps, with the cell under the pointer.', module = 'tests.orientations'},
    {id = 'infinite', title = 'Infinite maps', description = 'A map stored in chunks that reach negative cells, panned with the pointer, the keys or a stick.', module = 'tests.infinite'},
    {id = 'animations', title = 'Tile animations', description = 'Water, lava, torches, coins and slimes whose tiles play their frames on map time.', module = 'tests.animations'},
    {id = 'image-layers', title = 'Image layers', description = 'A sky, clouds and hills that repeat across the view and scroll at their own parallax.', module = 'tests.image-layers'},
    {id = 'groups', title = 'Group layers', description = 'Nested groups that pass their offset, tint, opacity and parallax on to their layers.', module = 'tests.groups'},
    {id = 'objects', title = 'Objects and templates', description = 'Every object shape and object templates with their overrides, picked under the pointer.', module = 'tests.objects'},
    {id = 'properties', title = 'Properties', description = 'Custom properties of every type on the map, its layers, its objects and its tiles.', module = 'tests.properties'},
    {id = 'collision', title = 'Collision', description = 'Physics bodies built from tile collision shapes and collision objects, with sensors and filters.', module = 'tests.collision'},
    {id = 'spawning', title = 'Spawning', description = 'Entities created from objects by factories keyed by object class.', module = 'tests.spawning'},
    {id = 'ysort', title = 'Y sorting', description = 'A character walking behind and in front of trees, fences and lamps sorted by their feet.', module = 'tests.ysort'},
    {id = 'raycasts', title = 'Ray casts', description = 'Rays against the cells of a tile layer and the shapes of an object layer, without physics.', module = 'tests.raycasts'},
    {id = 'worlds', title = 'Worlds', description = 'A world file that places listed maps and maps found by a file name pattern.', module = 'tests.worlds'},
}
