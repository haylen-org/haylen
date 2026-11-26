# 🌲 3D nodes

This guide explains 3D node trees, the retained 3D objects of the module `haylen.nodes3d`. It is for developers who build objects made of parts in 3D, such as a car with its wheels, a robot with its arms or a ship with its turrets. It covers what a tree is, how a node places itself relative to its parent, how a tree keeps its world state current at a low cost, and how to find, move, clone and listen to nodes. The [`haylen.nodes3d` reference](../lua-api/nodes3d.md) lists every function, option and error.

## What a 3D node tree is

A node tree holds nodes. A node is one part of an object. It has a parent, or none when it sits at the root of the tree, and it can have children. A child moves with its parent: when a car turns, its wheels turn with it, and each wheel can still spin on its own.

Each node has a local transform: a `position` in meters, a `rotation` as a quaternion and a `scale`. The local transform is relative to the parent. Combined with the transforms of every ancestor, it gives the world transform, the place of the node in the world. The axes and units follow the [3D conventions](coordinates.md): `y` points up, a node looks along negative `z`, and one unit is one meter.

```lua
local m = require('haylen.math')
local nodes3d = require('haylen.nodes3d')

local tree = nodes3d.newTree()
local car = tree:add({name = 'car', position = m.vec3(0, 0, -10)})
local wheel = car:add({name = 'wheel', position = m.vec3(0.8, 0.3, -1.2)})
car.rotation = m.fromAxisAngle(m.up(), 0.5)
print(wheel.worldPosition)
```

A tree places nodes and nothing else. It draws nothing by itself and holds no physics. What a node shows or simulates comes from the parts that belong to it, such as meshes, lights, cameras and bodies, which read the world state of the tree.

## World state that is always current

A node reads its world state through `worldMatrix`, `worldPosition`, `worldRotation`, `worldScale`, `worldVisible` and `worldEnabled`. These values are always current. When you change a node and then read a world value, the tree first computes the world state of what changed, so the read sees the change at once.

The tree only computes the subtrees that changed. It marks each node whose local state changes, and before the next read it walks only the subtrees under the highest marked nodes. A tree of a hundred thousand nodes where one wheel turned computes that wheel and its children, and nothing else. A tree where nothing changed costs almost nothing to read.

Every frame, after the scenes updated, the engine updates each tree that `nodes3d.newTree` made. The update does the same work as a read, but it splits large changes over the worker threads of the job system. A change of 4096 nodes or more runs in parallel, even inside one deep subtree, because the tree splits a subtree below its root. Call `tree:update()` to run that work earlier, such as in the middle of the update of a scene that moved many nodes and then reads them. The property `tree.updatedCount` tells how many nodes the last update computed.

```lua
local m = require('haylen.math')
local nodes3d = require('haylen.nodes3d')
local scene = require('haylen.scene')

local tree = nodes3d.newTree()
local fans = {}
for index = 1, 500 do
    fans[index] = tree:add({position = m.vec3(index * 2, 0, 0), children = {{name = 'blade'}}})
end

scene.push({
    update = function(self, dt)
        for _, fan in ipairs(fans) do
            fan.rotation = fan.rotation * m.fromAxisAngle(m.up(), dt)
        end
    end,
})
```

## World matrices, rotations and scales

The world matrix places a node exactly. A parent that is scaled more along one axis than another and a child that turns give the child a shear, a slant that turns a cube into a slanted box. The world matrix keeps that shear, so a point that `node:toWorld(point)` converts lands where the matrix puts it, and `node:toLocal(point)` converts it back.

The world rotation and the world scale combine the rotations and the scales of the chain of the node and leave the shear out, since a rotation and a scale cannot hold one. They are what a camera, a light or a sound needs: the direction a node faces and how large it is.

You can also set the world position and the world rotation. The tree then sets the local position or rotation that gives it. Under a parent scaled to nothing on an axis, the local position has no answer, so setting the world position raises an error.

`node:lookAt(target)` turns a node so its forward axis, negative `z`, points at a target in the world. Its up axis stays as close to `up` as it can, which is the world up axis unless you pass another.

```lua
local m = require('haylen.math')
local nodes3d = require('haylen.nodes3d')

local tree = nodes3d.newTree()
local tower = tree:add({name = 'tower', position = m.vec3(0, 0, 0), scale = m.vec3(2, 2, 2)})
local cannon = tower:add({name = 'cannon', position = m.vec3(0, 3, 0)})
cannon:lookAt(m.vec3(10, 0, -20))
local muzzle = cannon:toWorld(m.vec3(0, 0, -1.5))
print(muzzle, cannon.worldRotation)
```

## Which parts of the parent a node takes

A node takes the position, the rotation and the scale of its parent by default. Turn off `inheritPosition`, `inheritRotation` or `inheritScale` to leave one out. A part that the node does not take is a part of the world: with `inheritRotation` off, the node keeps its own rotation in the world however its parent turns.

- A compass needle on a ship keeps pointing north while the ship turns, with `inheritRotation` off.
- A health bar over a unit keeps its size while the unit grows, with `inheritScale` off.
- A node with all three off ignores its parent and still belongs to its subtree, so it goes when the subtree goes. Effects such as trails and smoke use it, since they stay in the world while their source moves on.

A node that takes every part multiplies the matrices, which keeps the shear above. A node that leaves a part out builds its matrix from the world position, rotation and scale it takes.

```lua
local m = require('haylen.math')
local nodes3d = require('haylen.nodes3d')

local tree = nodes3d.newTree()
local ship = tree:add({name = 'ship', rotation = m.fromAxisAngle(m.up(), 1.2)})
local compass = ship:add({name = 'compass', position = m.vec3(0, 2, 0), inheritRotation = false})
print(compass.worldRotation) -- the rotation of the world, not the ship's
```

## Visibility, the enabled state and layers

A node passes more than its transform to its descendants:

- Its `visible` hides the node and every descendant when it is `false`.
- Its `enabled` turns the node and every descendant off when it is `false`. A node that is off is hidden too, and what works on a node reads `worldEnabled` to know that it rests.

The results are `worldVisible` and `worldEnabled`. A node that is off keeps its own `enabled` flag, so turning the parent on again brings back each node as it was.

Other properties belong to the node alone:

- `layers` is a mask of 32 bits, `1` by default, that cameras and lights match with masks of their own, so a node shows only to those that share a bit with it.
- `renderOrder` draws the node before nodes of higher orders, for things such as a sky that must draw first.
- `frustumCulling` set to `false` keeps the node from being left out when it seems outside the view, for things whose bounds the renderer cannot know in advance.
- `static` says that a node never moves, so renderers may merge static nodes into batches and bakers may bake their light. Moving a static node later costs a rebuild of what took it in.

## Finding and moving nodes

Give nodes a `name` to find them later. A path joins the names from a root with slashes, such as `'car/wheel'`.

- `tree:find(path)` searches from the roots, and `node:find(path)` searches below a node. Both return `nil` when no node has that path.
- `tree:roots()` and `node:children()` return lists of nodes in order.
- `node.path` gives the path of a node, and `node.parent` its parent.

Tags group nodes across the tree, wherever they sit. A node can have many tags, and `tree:findTagged(tag)` returns every node with a tag, at the same cost however large the tree is.

To move a node and its subtree under another parent, set `node.parent`, or `nil` for the root. The node keeps its local transform, so it takes the same place relative to its new parent. Call `node:setParent(parent, {keepWorld = true})` to keep its place in the world instead, such as a part that falls off a car. A node cannot move under itself or under one of its descendants, and the tree refuses such a move without changing anything.

```lua
local m = require('haylen.math')
local nodes3d = require('haylen.nodes3d')

local tree = nodes3d.newTree()
local car = tree:add({name = 'car', position = m.vec3(5, 0, 0), children = {{name = 'door', position = m.vec3(1, 0.5, 0), tags = {'loose'}}}})
for _, part in ipairs(tree:findTagged('loose')) do
    part:setParent(nil, {keepWorld = true})
end
print(tree:find('door').worldPosition)
```

## Cloning nodes

`node:clone()` copies a node and its descendants: their names, local state, flags, tags and data. The copy goes under the same parent, or under the parent that the option `parent` names. Listeners are not copied. A node can even be cloned under one of its own descendants, since the clone reads the whole subtree before it adds anything.

The data table of each node is copied as a new table with the same keys and values, so changing a key of the copy leaves the original alone.

```lua
local m = require('haylen.math')
local nodes3d = require('haylen.nodes3d')

local tree = nodes3d.newTree()
local oak = tree:add({name = 'oak', data = {kind = 'oak'}})
for index = 1, 20 do
    local copy = oak:clone()
    copy.position = m.vec3(index * 4, 0, 0)
end
print(tree.size)
```

## Node handles and events

A `Node3D` value in Lua is a handle: a small reference to a node that lives inside the tree. Several handles can refer to the same node, and they compare equal with `==`. A handle keeps its tree alive.

The method `node:destroy()` removes a node with its whole subtree, deepest nodes first, and `tree:clear()` removes every node. From then on an old handle answers `false` for `valid` and raises `The node was destroyed.` for every other member. A new node may later reuse the place of a destroyed one inside the tree, and the old handle still refuses it, since each handle remembers which node it named.

A node tells listeners what happens to it with `node:on(event, listener, {owner = owner})`. A listener ends with its owner, like every listener of the engine, as the [subscription scopes](../core/lifecycle.md#subscription-scopes) explain.

| Event | When |
| --- | --- |
| `destroyed` | Right before the node goes. |
| `parentChanged` | After the node moved under another parent. |
| `childAdded` | After a child joined the node, which the listener receives. |
| `childRemoved` | After a child left the node, by a new parent or its destruction, which the listener receives. |
| `enabledChanged` | After the node turned on or off in the hierarchy, by its own flag, an ancestor or a new parent. |

Each node also has a `data` table for the app, such as the health of a unit. Every handle of the node returns the same table, and the table goes away with the node. A node that C++ made with data, such as a node of a glTF scene with its `extras`, starts its table from that data.

```lua
local nodes3d = require('haylen.nodes3d')
local scene = require('haylen.scene')

local tree = nodes3d.newTree()
local crate = tree:add({name = 'crate', data = {coins = 3}})
local lid = crate:add({name = 'lid'})

scene.push({
    enter = function(self)
        crate:on('childRemoved', function(child) print(child.name .. ' came off.') end, {owner = self})
        crate:on('destroyed', function(node) print('Coins in the crate: ' .. node.data.coins) end, {owner = self})
        lid.parent = nil
        crate:destroy()
    end,
})
```

## Components

A component gives a node a part that works on it, such as a body that moves it or an agent that walks it. Each kind of component is a component type with a name. A type says which properties its components take, with their kinds and defaults, and in which phases of the frame they run: with the fixed steps, before the scenes update or after them, as `nodes3d.componentTypes()` lists. A node holds at most one component of each type.

- `node:addComponent(type, options)` adds a component from a table of its properties and returns it. A `Vec3` or a `Quat` passes as itself.
- `node:component(type)` returns the component of a type, or `nil`, and `node:components()` lists them all.
- `component:get(name)` and `component:set(name, value)` read and change a property, and `component:remove()` takes the component off.

The components of a type live in arrays, one set for each tree, and one call runs all of them in a phase, so a thousand components cost one call and no call into Lua. A component goes with its node, a clone of the node gets a copy of it, and it rests while its node is off in the hierarchy. A tree runs its components while the game is not paused, and its `processMode` changes that, as [pause and process modes](../core/lifecycle.md#pause-and-process-modes) explains for scenes.

`node:saveComponents()` returns the components of a node as a table of the properties of each one by its type, and `node:loadComponents(table)` adds them to another node, which is how saved scenes recreate them.

Plugins add component types in C++ through the registry of the 3D nodes plugin, the same way the engine adds its own, as the section [From C++](#from-c) shows.

## Moving many nodes at once

Some apps simulate many objects themselves, such as a flock of birds. They can move all the nodes in one call. Write the position and the rotation of each node into a [float buffer](../lua-api/collections.md#float-buffers), seven values per node, `x`, `y`, `z` and the `x`, `y`, `z` and `w` of the rotation, and call `tree:writeTransforms(nodes, buffer)`. To read where they ended up, `tree:readTransforms(nodes, buffer)` fills a buffer with the world positions and rotations in the same layout. The layout is the one of [`world:readTransforms`](../lua-api/physics3d.md#worldreadtransformsbodies-buffer-first-interpolated) of 3D physics, so one buffer carries bodies to nodes.

```lua
local collections = require('haylen.collections')
local nodes3d = require('haylen.nodes3d')

local tree = nodes3d.newTree()
local birds = {}
for index = 1, 50 do
    birds[index] = tree:add({name = 'bird' .. index})
end
local places = collections.newFloatBuffer(50 * 7)
for index = 1, 50 do
    places[index * 7 - 6] = index
    places[index * 7 - 5] = 10 + math.sin(index)
    places[index * 7] = 1
end
tree:writeTransforms(birds, places)
```

## Nodes with tweens

The `position`, `x`, `y`, `z`, `rotation` and `scale` of a node and its `worldPosition` and `worldRotation` are native properties. So [`haylen.tween`](../lua-api/tween.md) animates them in C++, without a call into Lua in every frame, and a rotation turns along the shorter arc.

```lua
local m = require('haylen.math')
local nodes3d = require('haylen.nodes3d')
local tween = require('haylen.tween')

local tree = nodes3d.newTree()
local door = tree:add({name = 'door'})
tween.to(door, 0.8, {rotation = m.fromAxisAngle(m.up(), math.pi / 2)})
```

## Performance

A tree keeps its nodes in contiguous arrays in tree order, so a subtree is one run of the arrays. The state an update reads, the local and world transforms and the flags of each node, sits next to each other, and the names, tags and data sit apart. An update after a few changes sorts the changed nodes and computes their subtrees. An update after many changes scans the order once. Neither allocates once the tree has grown, so a frame that moves nodes allocates nothing in the tree.

The command `haylen engine bench --suite nodes` times a tree of 100,000 3D nodes on the headless host: a thousand roots with 99 children each. It reports the update after every root moved and after one root in a hundred moved, on one thread and on the job system, next to the rows of the 2D tree.

These are the numbers of a Linux machine in Release with 32 workers. When one root in a hundred moves, the tree computes only those ten subtrees, so the update costs a few hundredths of a millisecond on one thread as on the job system:

| Scene | Average ms per frame |
| --- | --- |
| Update after every root moved, one thread | 2.513 |
| Update after one root in a hundred moved, one thread | 0.025 |
| Update after every root moved, job system | 0.369 |
| Update after one root in a hundred moved, job system | 0.026 |

## From C++

The same tree is `haylen::nodes3d::NodeTree3D` in `haylen/3d/nodes/NodeTree3D.hpp`. It lives in a shared pointer that `NodeTree3D::create()` makes. A `Node3D` handle watches the tree and reports `isValid()` as `false` from the moment its node or the tree is gone.

- The struct `NodeProperties3D` describes a new node, with `Inheritance3D` for the parts of the parent it takes and `data` for a JSON value of the app.
- The method `update(jobs)` brings the tree up to date on a job system, and every world read of a `Node3D` brings it up to date on the calling thread.
- The method `on(node, event, slot)` and `Node3D::on` call a slot for the `NodeEvent3D` of one node. The signals `destroyed`, `parentChanged` and `enabledChanged` of the tree tell of every node.
- The signal `cloned` tells of every node that a clone copied, with its copy, so a system that keeps state for nodes copies its own.
- The methods `writeTransforms` and `readTransforms` move and read many nodes from spans of floats.
- A component type derives from `ComponentType3D`: it names its phases and the `Property` metadata of its properties, and makes for each tree a `ComponentPool3D`, which keeps the components of the tree in arrays by index and runs them in one call per phase with a `ComponentPool3D::Batch` of their nodes and of whether each node is on. A plugin registers it in `Nodes3DPlugin::getComponentTypes()`, and `Nodes3DPlugin::createTree()` makes a tree that the plugin runs. `Node3D::addComponent` returns a `Component3D` handle, and `NodeTree3D::getComponents()` returns the `ComponentStore3D` of the tree.

```cpp
#include <any>
#include <memory>

#include "haylen/3d/nodes/NodeTree3D.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Scene.hpp"
#include "haylen/math/Quat.hpp"

class Garage final : public haylen::core::Scene {
  public:
    void enter(haylen::core::Engine&, const std::any&) override {
        haylen::nodes3d::Node3D car = tree->add({.name = "car", .position = {0.0F, 0.0F, -10.0F}, .tags = {"vehicle"}});
        wheel = car.add({.name = "wheel", .position = {0.8F, 0.3F, -1.2F}});
    }

    void update(haylen::core::Engine& engine, float deltaSeconds) override {
        wheel.setRotation(wheel.getRotation() * haylen::math::Quat::fromAxisAngle({1.0F, 0.0F, 0.0F}, deltaSeconds * 6.0F));
        tree->update(&engine.getJobs());
    }

  private:
    std::shared_ptr<haylen::nodes3d::NodeTree3D> tree = haylen::nodes3d::NodeTree3D::create();
    haylen::nodes3d::Node3D wheel;
};
```
