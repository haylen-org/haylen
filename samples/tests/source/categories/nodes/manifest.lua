-- The tests of the category in menu order.
return {
    prefix = 'NOD',
    title = 'Nodes',
    description = 'Retained trees of 2D nodes with parenting, culling, prefabs and saved descriptions.',
    tests = {
        {code = 'NOD-001', title = 'Node tree', description = 'Tanks of one prefab whose turrets aim at the pointer and wheels turn, culled as the camera pans, added, destroyed, hidden and saved.', module = 'tree'},
    },
}
