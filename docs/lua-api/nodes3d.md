# 📖 `haylen.nodes3d`

The module `haylen.nodes3d` keeps retained trees of 3D nodes, for objects made of parts that move together in 3D space. Positions are in meters and rotations are quaternions, as the [3D conventions](../3d/coordinates.md) say. The [3D nodes guide](../3d/nodes.md) explains how a tree keeps its world state current, which parts of a parent a node takes and how nodes are found, moved, cloned and listened to.

```lua
local nodes3d = require('haylen.nodes3d')
```

## Functions

### nodes3d.newTree()

Creates an empty [`NodeTree3D`](#nodetree3d).

```lua
local m = require('haylen.math')
local nodes3d = require('haylen.nodes3d')
local scene = require('haylen.scene')

local tree = nodes3d.newTree()
local mill = tree:add({name = 'mill', position = m.vec3(0, 0, -20)})
local sails = mill:add({name = 'sails', position = m.vec3(0, 8, 1)})

scene.push({
    update = function(self, dt)
        sails.rotation = sails.rotation * m.fromAxisAngle(m.vec3(0, 0, 1), dt)
        tree:update()
    end,
})
```

## NodeTree3D

A `NodeTree3D` holds nodes, which `nodes3d.newTree` creates. Two tree values compare equal with `==` when they refer to the same tree.

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `size` | integer | read | The number of nodes. |
| `updatedCount` | integer | read | The nodes that the last update with changes to apply computed again, which a read of world state also counts. |

### tree:add(properties)

Adds a node at the root, after the roots the tree already has, and returns its [`Node3D`](#node3d). The table `properties` takes these keys, all optional:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `name` | string | `''` | The name in paths. |
| `position` | Vec3 | `(0, 0, 0)` | The local position in meters. |
| `rotation` | Quat | no rotation | The local rotation, normalized as it is set. |
| `scale` | Vec3 | `(1, 1, 1)` | The local scale. |
| `visible` | boolean | `true` | Whether the node and its descendants show. |
| `enabled` | boolean | `true` | Whether the node and its descendants are on. |
| `layers` | integer | `1` | The mask of 32 bits that cameras and lights match with their own masks. |
| `static` | boolean | `false` | Whether the node never moves, so renderers may batch it and bakers may bake it. |
| `frustumCulling` | boolean | `true` | Whether the node may be left out when it seems outside the view. |
| `renderOrder` | integer | `0` | Nodes of lower orders draw first. |
| `inheritPosition`, `inheritRotation`, `inheritScale` | boolean | `true` | Which parts of the world transform of the parent the node takes. |
| `tags` | list of strings | none | The tags of the node, which [`tree:findTagged`](#treefindtaggedtag) finds. |
| `data` | table | a new table | The table that [`node.data`](#node3d) returns. |
| `children` | list | none | Properties tables of children, added in order. |

These errors can come up:

- Any other key raises `Unknown option "<key>".`
- A rotation whose length is zero raises `The quaternion holds no rotation, because its length is zero or not a finite number.`
- A tree holds at most 16,777,216 nodes, and one more raises `A 3D node tree holds at most 16777216 nodes.`

```lua
local m = require('haylen.math')
local nodes3d = require('haylen.nodes3d')

local tree = nodes3d.newTree()
local robot = tree:add({
    name = 'robot',
    position = m.vec3(0, 0, -5),
    tags = {'unit'},
    data = {health = 10},
    children = {
        {name = 'head', position = m.vec3(0, 1.6, 0)},
        {name = 'arm', position = m.vec3(0.4, 1.2, 0), children = {{name = 'hand', position = m.vec3(0, -0.6, 0)}}},
        {name = 'shadow', inheritRotation = false, renderOrder = -1},
    },
})
print(robot.data.health, tree.size)
```

### tree:find(path)

Returns the node at a path of names from the roots separated by slashes, such as `'robot/arm/hand'`, or `nil`.

### tree:findByKey(key)

Returns the node whose [`key`](#node3d) is `key`, or `nil` once that node is gone.

### tree:findTagged(tag)

Returns a list of every node with the tag, in no particular order, or an empty list.

```lua
local nodes3d = require('haylen.nodes3d')

local tree = nodes3d.newTree()
tree:add({name = 'goblin', tags = {'enemy'}})
tree:add({name = 'bat', tags = {'enemy', 'flying'}})
for _, enemy in ipairs(tree:findTagged('enemy')) do
    print(enemy.name)
end
```

### tree:roots()

Returns a list of the roots, in order.

### tree:update()

Computes the world state of every node whose local state changed, and of its descendants, on the worker threads of the job system when 4096 nodes or more changed. Reads of world state are current without it, and it suits the frame of an app that moves many nodes, once after they moved.

### tree:clear()

Destroys every node, deepest first, and calls their listeners.

### tree:writeTransforms(nodes, buffer, first)

Sets the local position and rotation of every node of the list `nodes` from the `FloatBuffer` `buffer`, seven floats for each node in order, `x`, `y`, `z` and the `x`, `y`, `z` and `w` of its rotation, from the position `first`, 1 unless the call names another. A rotation is normalized. The layout is the one of [`world:readTransforms`](physics3d.md#worldreadtransformsbodies-buffer-first-interpolated) of 3D physics.

- A node that is gone or belongs to another tree raises `The node was destroyed.` or `The node belongs to another tree.`, and a rotation of length zero raises its error, before any node moves.
- A buffer without seven values for each node from `first` raises `Node3D transforms take seven floats for each node.`.
- A `first` outside the buffer raises an error about that argument.

### tree:readTransforms(nodes, buffer, first)

Copies the world position and rotation of every node of the list into the buffer, in the layout of [`tree:writeTransforms`](#treewritetransformsnodes-buffer-first), with its errors.

```lua
local collections = require('haylen.collections')
local m = require('haylen.math')
local nodes3d = require('haylen.nodes3d')

local tree = nodes3d.newTree()
local flock = {}
for index = 1, 100 do
    flock[index] = tree:add({position = m.vec3(index, 10, 0)})
end
local places = collections.newFloatBuffer(#flock * 7)
tree:readTransforms(flock, places)
for index = 1, #flock do
    places[index * 7 - 5] = places[index * 7 - 5] + math.sin(index) * 0.1
end
tree:writeTransforms(flock, places)
```

## Node3D

A `Node3D` is a handle to a node of a tree, which `tree:add`, `node:add`, `tree:find` and the other functions of a tree return. It keeps its tree alive. Every member of a destroyed node raises `The node was destroyed.`, except `valid`. Two node values compare equal with `==` when they refer to the same node.

### Properties

The properties `position`, `x`, `y`, `z`, `rotation`, `scale`, `worldPosition` and `worldRotation` are native properties, so [tweens](tween.md) animate them without running Lua. The world properties are always current: a read first computes what changed.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `name` | string | read and write | The name in paths. |
| `position` | Vec3 | read and write | The local position in meters. |
| `x`, `y`, `z` | number | read and write | The parts of the local position. |
| `rotation` | Quat | read and write | The local rotation. Setting a rotation of length zero raises `The quaternion holds no rotation, because its length is zero or not a finite number.`. |
| `scale` | Vec3 | read and write | The local scale. |
| `transform` | Transform3D | read and write | The local position, rotation and scale together. |
| `visible` | boolean | read and write | Whether the node and its descendants show. |
| `enabled` | boolean | read and write | Whether the node and its descendants are on. A node that is off is hidden too. |
| `layers` | integer | read and write | The mask of 32 bits that cameras and lights match. |
| `static` | boolean | read and write | Whether the node never moves. |
| `frustumCulling` | boolean | read and write | Whether the node may be left out when it seems outside the view. |
| `renderOrder` | integer | read and write | Nodes of lower orders draw first. |
| `inheritPosition`, `inheritRotation`, `inheritScale` | boolean | read and write | Which parts of the world transform of the parent the node takes. A part it does not take is a part of the world. |
| `parent` | Node3D or nil | read and write | The parent, or `nil` at the root. Setting it moves the node and its subtree under another node of the tree, or to the root with `nil`, and keeps its local transform. A node set under itself or under one of its descendants raises `A node cannot move under itself or under one of its descendants.`. |
| `childCount` | integer | read | The number of children. |
| `path` | string | read | The names from the root, such as `'robot/arm'`. |
| `tags` | list of strings | read | The tags of the node. |
| `data` | table | read | A table for the app, the same for every handle of the node, which goes with the node. A node that C++ made with data starts it from that data, and data that is neither a JSON object nor an array raises an error that names the node. |
| `worldMatrix` | Mat4 | read | The matrix that places the node in the world exactly. |
| `worldPosition` | Vec3 | read and write | The position in the world. Setting it under a parent scaled to nothing raises `A node under a parent scaled to nothing has no local position for a world position.`. |
| `worldRotation` | Quat | read and write | The rotation in the world, the rotations of the chain combined. |
| `worldScale` | Vec3 | read | The scales of the chain combined. |
| `worldVisible` | boolean | read | Whether the node and every ancestor are visible and on. |
| `worldEnabled` | boolean | read | Whether the node and every ancestor are on. |
| `key` | integer | read | A number that names the node among every node its tree has held. |
| `valid` | boolean | read | Whether the node exists. |

```lua
local m = require('haylen.math')
local nodes3d = require('haylen.nodes3d')
local tween = require('haylen.tween')

local tree = nodes3d.newTree()
local lamp = tree:add({name = 'lamp', position = m.vec3(0, 3, 0), layers = 3})
local bulb = lamp:add({name = 'bulb', position = m.vec3(0, 0.2, 0)})
bulb.data.watts = 60
lamp.scale = m.vec3(2, 2, 2)
print(bulb.worldPosition, bulb.worldScale, bulb.worldVisible, bulb.path)
lamp.enabled = false
print(bulb.worldEnabled, bulb.enabled)
tween.to(lamp, 1, {rotation = m.fromAxisAngle(m.up(), math.pi)})
```

### node:add(properties)

Adds a child after the children the node already has, with the keys of [`tree:add`](#treeaddproperties), and returns it.

### node:find(path)

Returns the descendant at a path of names below the node, or `nil`.

### node:children()

Returns a list of the children, in order.

### node:setParent(parent, options)

Moves the node and its subtree under `parent`, a node of the same tree, or to the root with `nil`, after the children the parent has. The optional table `options` takes `keepWorld`, which keeps the place of the node in the world instead of its local transform. A move under itself or one of its descendants raises `A node cannot move under itself or under one of its descendants.`, and a move with `keepWorld` under a parent scaled to nothing raises `A node cannot keep its place in the world under a parent scaled to nothing.`, both before anything changes.

```lua
local m = require('haylen.math')
local nodes3d = require('haylen.nodes3d')

local tree = nodes3d.newTree()
local truck = tree:add({name = 'truck', position = m.vec3(12, 0, 0)})
local crate = truck:add({name = 'crate', position = m.vec3(0, 1, -2)})
crate:setParent(nil, {keepWorld = true})
print(crate.position)
```

### node:clone(options)

Copies the node and its descendants with their names, local state, flags, tags and data, and returns the copy of the node. The data table of each node is copied as a new table that holds the same values. The copy goes under the parent of the node, after its children, unless the optional table `options` names another `parent`, a node of the tree or `false` for the root. Listeners are not copied.

```lua
local m = require('haylen.math')
local nodes3d = require('haylen.nodes3d')

local tree = nodes3d.newTree()
local post = tree:add({name = 'post', children = {{name = 'lamp', position = m.vec3(0, 4, 0)}}})
for index = 1, 10 do
    local copy = post:clone({parent = false})
    copy.x = index * 6
end
print(#tree:roots())
```

### node:addTag(tag)

Adds the tag to the node. A tag the node has already changes nothing.

### node:removeTag(tag)

Removes the tag from the node. A tag the node lacks changes nothing.

### node:hasTag(tag)

Returns whether the node has the tag.

### node:toWorld(point)

Returns the `Vec3` of the world where a point of the space of the node lies, through the world matrix.

### node:toLocal(point)

Returns the point of the space of the node at a point of the world. A node scaled to nothing raises `A node scaled to nothing has no local space to convert a point into.`.

```lua
local m = require('haylen.math')
local nodes3d = require('haylen.nodes3d')

local tree = nodes3d.newTree()
local gun = tree:add({position = m.vec3(2, 1, 0), rotation = m.fromAxisAngle(m.up(), 0.4)})
local muzzle = gun:toWorld(m.vec3(0, 0, -1.2))
print(muzzle, gun:toLocal(muzzle))
```

### node:lookAt(target, up)

Turns the node in the world so its forward axis, negative `z`, points at the `Vec3` `target`, with its up axis as close to `up`, `m.up()` unless the call names another, as it can. A target at the place of the node, which a point within the rounding of `vec3:approximately` counts as, raises `A node cannot look at a target at its own place in the world. Give it a target away from the node.`, and a target straight along `up` from the node raises the error of [`m.lookRotation`](math.md#mlookrotationdirection-up).

### node:destroy()

Destroys the node and its descendants, deepest first, and calls their listeners.

### node:on(event, listener, options)

Listens to an event of the node and returns a `Connection`. The listener receives the node, or the child that joined or left for `childAdded` and `childRemoved`. The optional table `options` takes `owner`, which ends the listener with its owner. Without one, the listener takes the default owner. Another event raises `Unknown node event "<event>". Nodes report "destroyed", "parentChanged", "childAdded", "childRemoved" and "enabledChanged".`.

| Event | When |
| --- | --- |
| `destroyed` | Right before the node goes, after its descendants. |
| `parentChanged` | After the node moved under another parent. |
| `childAdded` | After a child joined the node, by `add`, a clone or a new parent. |
| `childRemoved` | After a child left the node, by a new parent or its destruction. |
| `enabledChanged` | After the node turned on or off in the hierarchy, by its own flag, an ancestor or a new parent. |

```lua
local nodes3d = require('haylen.nodes3d')
local scene = require('haylen.scene')

local tree = nodes3d.newTree()
local cart = tree:add({name = 'cart'})

scene.push({
    enter = function(self)
        cart:on('childAdded', function(child) print(child.name .. ' got on.') end, {owner = self})
        cart:on('enabledChanged', function(node) print(node.name .. ' on: ' .. tostring(node.worldEnabled)) end, {owner = self})
        cart:add({name = 'rider'})
        cart.enabled = false
    end,
})
```
