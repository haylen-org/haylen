-- The tests of the category in menu order.
return {
    prefix = 'GEN',
    title = 'Procedural generation',
    description = 'Worlds, people, critters, monsters and climbing creatures made from a seed and moved by code alone, with legs and bodies that bend on chains.',
    tests = {
        {code = 'GEN-001', title = 'World map', description = 'Islands from layered noise with biomes, rivers that run downhill to the sea, towns with generated names and roads that avoid water and mountains.', module = 'world-map'},
        {code = 'GEN-002', title = 'Characters', description = 'A crowd of people built from a seed, with their build, skin, hair, face, clothes and accessories, whose arms bend on chains to wave at the pointer.', module = 'characters'},
        {code = 'GEN-003', title = 'Critters', description = 'Bugs built from a seed with their bodies, colors, legs, antennae and wings, which scurry on legs that step in turns and flee the pointer.', module = 'critters'},
        {code = 'GEN-004', title = 'Monsters', description = 'Monsters built from a seed with their bodies, eyes, mouths, horns, spikes, legs and tentacles, which walk or float and roar when touched.', module = 'monsters'},
        {code = 'GEN-005', title = 'Spider', description = 'A spider with eight legs on chains that climbs the floor, walls, ceiling and rocks of a generated cave and leaps between rocks on a thread.', module = 'spider'},
        {code = 'GEN-006', title = 'Centipede', description = 'A centipede whose body follows its head on a chain around the rocks of a generated cave, up walls and across the ceiling, on dozens of stepping legs.', module = 'centipede'},
    },
}
