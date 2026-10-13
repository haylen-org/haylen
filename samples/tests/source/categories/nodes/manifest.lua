-- The tests of the category in menu order.
return {
    prefix = 'NOD',
    title = 'Nodes',
    description = 'Retained trees of 2D nodes with parenting, culling, prefabs and saved descriptions.',
    tests = {
        {code = 'NOD-001', title = 'Node tree', description = 'A column of tanks from one prefab whose turrets aim at the pointer and whose wheels turn, culled as the camera pans, added, destroyed, hidden and saved.', module = 'tree'},
    },
}
