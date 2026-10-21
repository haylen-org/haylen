# 🌳 Nodes

This guide explains node trees, the retained 2D objects of the module `haylen.nodes2d`. It is for developers who build objects made of parts, such as a tank with a turret, a hero with a sword or a unit with a health bar over its head. It covers when to use a tree, how a tree updates and draws, how to save and load parts of a tree, and how nodes work with physics, tweens, animation and particles. The [`haylen.nodes2d` reference](../lua-api/nodes2d.md) lists every function, option and error.

## What a node tree is

A node tree holds nodes. A node is one part of an object. It has a parent, or none when it sits at the root of the tree, and it can have children. A child moves with its parent: when a tank turns, its turret turns with it, and the turret can still turn on its own.

Each node has a local transform: a position `x` and `y`, a `rotation` in radians and a scale `scaleX` and `scaleY`. The local transform is relative to the parent. The tree combines it with the transforms of all the ancestors into a world transform, which places the node in the world.

A node shows at most one thing: a sprite, a block of text, a nine-slice frame or particles. A node that shows nothing still groups its children, so a whole object can move as one.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local nodes2d = require('haylen.nodes2d')

local tree = nodes2d.newTree()
local tank = tree:add({name = 'tank', x = 400, y = 300, sprite = graphics2d.newSprite(graphics.whiteTexture(), {width = 80, height = 48})})
local turret = tank:add({name = 'turret', sprite = graphics2d.newSprite(graphics.whiteTexture(), {width = 60, height = 12, pivotX = 0})})
tank.rotation = 0.5
turret.rotation = -0.2
```

## When to use a tree

Haylen has several ways to draw 2D objects. Pick the one that fits what you draw:

| Need | Use |
| --- | --- |
| Things that change shape every frame, such as a debug line or an aiming arc. | The draw functions of [`haylen.graphics2d`](../lua-api/graphics2d.md), called in every frame. |
| Many separate sprites of one texture, such as bullets, coins or grass. | A [sprite batch](../lua-api/graphics2d.md#spritebatch), or an animated batch of [`haylen.animation2d`](animation.md) when they animate. |
| Objects made of parts that move together, such as vehicles, characters with equipment and units with labels. | A node tree. |

A tree keeps its objects from one frame to the next. You change only what moved, and the tree does the rest in C++. However many nodes it holds, a tree costs two calls from Lua per frame: `tree:update(dt)` and `tree:draw()`.

## What a node passes to its children

A node passes more than its transform to its descendants:

- Its `color` multiplies the colors of its descendants. A root with the color `'#80FFFFFF'` fades the whole object to half.
- Its `visible` hides the node and every descendant when it is `false`.
- Its `layer` adds to the layers of its descendants, so a whole object can rise above another.

The results are the world values `worldColor`, `worldLayer` and `worldVisible` of each node.

```lua
local nodes2d = require('haylen.nodes2d')

local tree = nodes2d.newTree()
local ghost = tree:add({name = 'ghost', color = '#80FFFFFF', layer = 2})
local eyes = ghost:add({name = 'eyes', text = 'o o', layer = 1})
tree:update(0)
print(eyes.worldColor.a, eyes.worldLayer)
ghost.visible = false
tree:update(0)
print(eyes.worldVisible)
```

## Where content sits on a node

The node places what it shows. A sprite keeps its own `x`, `y`, `rotation`, `scaleX` and `scaleY`, which offset it from the node. Text and frames sit on the node itself. The world transform of the node then places the result.

The world color of the node multiplies the color of its content. Its world layer adds to the layer of the content, and its `depth` and `sortOffset` add to those of the content, as the [draw order](../graphics/rendering.md#draw-order) uses them.

Sprites, text and frames take the rotation and the scale of the world transform. A parent that is turned and scaled more along one axis than the other makes a shear, a slant that would turn a rectangle into a parallelogram. Content never shows that shear, so a sprite under such a parent stays a rectangle.

## Updating and drawing

Call `tree:update(dt)` in the update of your scene and `tree:draw()` in its render.

- The update recomputes the world state of the nodes whose local state changed, and of their descendants. Nodes that did not change cost almost nothing, so a still scene is cheap. The update also moves the nodes that follow physics bodies and advances the particles that nodes show.
- The draw first applies the changes made since the last update, so a node you move after the update still draws in its new place. Then it draws every visible node into the open canvas.

Nodes draw in tree order: parents before their children, and siblings in the order you added them. A node that comes later draws on top of the nodes before it, unless their layers say otherwise.

The draw leaves out every node whose bounds lie outside the area the canvas shows. This is called culling. The properties `tree.drawnCount` and `tree.culledCount` tell how many nodes the last draw drew and how many it left out. Particles are never left out, since they can fly far from their node.

The scene that calls `tree:update` decides when the tree runs, so a tree pauses with its scene. To raise a whole tree above other draws, wrap its draw in [`graphics2d.pushLayerOffset`](../lua-api/graphics2d.md#graphics2dpushlayeroffsetoffset).

```lua
local graphics2d = require('haylen.graphics2d')
local nodes2d = require('haylen.nodes2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local tree = nodes2d.newTree()
local sign = tree:add({name = 'sign', text = 'Shop'})

scene.push({
    update = function(self, dt)
        sign.rotation = sign.rotation + dt
        tree:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.pushLayerOffset(10)
        tree:draw()
        graphics2d.popLayerOffset()
    end,
})
```

## Local and world points

Use `node:toWorld(x, y)` to turn a point of a node into a point of the world, such as the tip of a gun barrel. Use `node:toLocal(x, y)` for the other way, such as a mouse click on a part. The function `node:worldTransform()` returns the whole world transform as a `Transform2D`.

These functions read the state of the last update or draw. Call `tree:update` first when you need the new state in the same frame.

```lua
local nodes2d = require('haylen.nodes2d')

local tree = nodes2d.newTree()
local tank = tree:add({x = 100, y = 100, rotation = math.pi / 2})
local barrel = tank:add({x = 40})
tree:update(0)
local muzzleX, muzzleY = barrel:toWorld(20, 0)
print(muzzleX, muzzleY)
```

## Finding and moving nodes

Give nodes a `name` to find them later. A path joins the names from a root with slashes, such as `'tank/turret'`.

- `tree:find(path)` searches from the roots, and `node:find(path)` searches below a node. Both return `nil` when no node has that path.
- `tree:roots()` and `node:children()` return lists of nodes in order.
- `node.path` gives the path of a node, and `node.parent` its parent.

To move a node and its subtree under another parent, set `node.parent`. Set it to `nil` to move the node to the root. The node keeps its local transform, so it takes the same place relative to its new parent.

```lua
local nodes2d = require('haylen.nodes2d')

local tree = nodes2d.newTree()
local hero = tree:add({name = 'hero', children = {{name = 'hand', x = 20}}})
local chest = tree:add({name = 'chest', x = 300})
local sword = chest:add({name = 'sword'})
sword.parent = tree:find('hero/hand')
print(sword.path)
```

## Node handles

A `Node` value in Lua is a handle: a small reference to a node that lives inside the tree. Several handles can refer to the same node, and they compare equal with `==`. A handle keeps its tree alive.

A node goes away with `node:destroy()`, which also destroys its descendants, deepest first, or with `tree:clear()`. After that, every member of an old handle raises `The node was destroyed.`, except `valid`, which turns `false`. Code that keeps a node too long fails at once instead of drawing a stale object.

To clean up your own state when a node goes, listen to its `'destroyed'` event. The listener runs right before the node goes, and it ends with its owner, like every listener of the engine, as the [subscription scopes](../core/lifecycle.md#subscription-scopes) explain.

Each node also has a `data` table for the app, such as the health of a unit. Every handle of the node returns the same table, and the table goes away with the node.

```lua
local nodes2d = require('haylen.nodes2d')
local scene = require('haylen.scene')

local tree = nodes2d.newTree()
local crate = tree:add({name = 'crate', data = {coins = 3}})

scene.push({
    enter = function(self)
        crate:on('destroyed', function(node) print('The crate held ' .. node.data.coins .. ' coins.') end, {owner = self})
        crate:destroy()
        print(crate.valid)
    end,
})
```

## Prefabs and descriptions

A description is a plain table that describes a node: its local state, what it shows and its children. A `.prefab` file holds the same description as JSON. With descriptions, levels and editors keep their objects as data instead of code.

- The function `assets.load('prefabs/tank.prefab')` reads a `.prefab` file and checks it on a worker thread. The result is a `Prefab`, a description that is known to be valid.
- The function `nodes2d.newPrefab(description)` makes a `Prefab` from a table.
- The method `tree:instantiate(prefab, overrides)` builds a new subtree from a prefab. The overrides replace keys of its root, such as its place and its name.
- The method `tree:load(description)` builds a subtree straight from a table.
- The function `nodes2d.save(node)` turns a node and its descendants back into a description.

A description names its assets by their paths in the package, such as `'units/tank.png'`. So only content that came from a file can be saved. A texture drawn in code or particles made from an options table have no path, and saving them raises an error that names the node.

```json
{
    "name": "tank",
    "sprite": {"texture": "units/tank.png", "filter": "linear"},
    "children": [
        {"name": "turret", "y": -8, "sprite": {"texture": "units/turret.png", "filter": "linear", "pivotX": 0.2}},
        {"name": "label", "y": -60, "text": {"text": "Tank", "size": 18, "anchor": [0.5, 1]}}
    ]
}
```

```lua
local assets = require('haylen.assets')
local json = require('json')
local nodes2d = require('haylen.nodes2d')

local tree = nodes2d.newTree()
local tank = assets.load('prefabs/tank.prefab')
for index = 1, 3 do
    tree:instantiate(tank, {name = 'tank' .. index, x = index * 160, y = 300})
end
print(json.encode(nodes2d.save(tree:find('tank2'))))
```

## Nodes with physics

A node can follow a body of [`haylen.physics2d`](physics.md). Pass the body as `body` when you add the node, or set `node.body` later. On every update the node takes the position and the rotation of the body in the world, whatever its parent is.

When the world of the body interpolates between fixed steps, as [smooth drawing](physics.md#smooth-drawing-between-fixed-steps) explains, the node takes the place between the last two steps. So a physics object and its parts draw smoothly at any frame rate. The tree holds the body, which keeps its world alive, until you set `node.body` to `nil`.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local nodes2d = require('haylen.nodes2d')
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local ball = world:createBody({x = 200, y = 0})
ball:addCircle(20)
local tree = nodes2d.newTree()
tree:add({name = 'ball', body = ball, sprite = graphics2d.newSprite(graphics.whiteTexture(), {width = 40, height = 40})})
```

## Nodes with tweens and animation

The number properties of a node and its `color` are native properties. So [`haylen.tween`](../lua-api/tween.md) animates them in C++, without a call into Lua in every frame.

An animator of [`haylen.animation2d`](animation.md) shows its current frame on the sprite of a node with `animator:apply(node)`, as on any sprite.

```lua
local nodes2d = require('haylen.nodes2d')
local tween = require('haylen.tween')

local tree = nodes2d.newTree()
local door = tree:add({name = 'door', text = 'Door'})
tween.to(door, 0.5, {x = 120, color = '#FFFFC040'})
```

## Nodes with particles

A node can show particles of [`haylen.particles2d`](particles.md): an emitter, a particle system or the package path of a `.particles` file. The node places the particles. They spawn where the node is, turned and scaled with it, and `tree:update(dt)` advances them, so you do not update the emitter yourself.

```lua
local nodes2d = require('haylen.nodes2d')

local tree = nodes2d.newTree()
local ship = tree:add({name = 'ship', x = 300, y = 200})
ship:add({name = 'exhaust', x = -30, particles = 'effects/exhaust.particles'})
```

## Moving many nodes at once

Some apps simulate many objects themselves, such as a flock of birds. They can move all the nodes in one call. Write the `x`, `y` and `rotation` of each node into a [float buffer](../lua-api/collections.md#float-buffers), three values per node, and call `tree:writeTransforms(nodes, buffer)`. The method `tree:readTransforms(nodes, buffer)` copies the world positions and rotations back the same way.

```lua
local collections = require('haylen.collections')
local nodes2d = require('haylen.nodes2d')

local tree = nodes2d.newTree()
local birds = {}
for index = 1, 50 do
    birds[index] = tree:add({name = 'bird' .. index})
end
local places = collections.newFloatBuffer(150)
for index = 1, 50 do
    places[index * 3 - 2] = index * 12
    places[index * 3 - 1] = math.sin(index) * 40
end
tree:writeTransforms(birds, places)
```

## Performance

A tree keeps its nodes in contiguous arrays in tree order and walks them in order. An update touches only the nodes that changed and their descendants. A tree of 4096 nodes or more updates the subtrees of its roots on the worker pool, in groups of at least 1024 nodes, while the frame thread does its share.

A draw records every node in C++ without a call from Lua per node, and a culled node costs only a test of its bounds. The draw joins neighbouring sprite nodes of one texture and draw order into one batch, as the [rendering guide](../graphics/rendering.md#node-trees) explains, so a tree of the sprites of one atlas costs a few draw items however deep it is. A batch converts its sprites on the worker pool from 8,192 sprites on.

The command `haylen bench --suite nodes` times a tree of 100,000 sprite nodes on the headless host: a thousand roots with 99 children each. These are the numbers of an Apple M5 Pro in Release with 15 workers:

| Scene | Average ms per frame |
| --- | --- |
| Update after every root moved, one thread | 1.015 |
| Update after one root in a hundred moved, one thread | 0.189 |
| Update after every root moved, job system | 0.208 |
| Update after one root in a hundred moved, job system | 0.075 |
| Draw with every node in view | 2.388 |
| Draw with 361 nodes in view and the rest culled | 0.400 |

## From C++

The same tree is `haylen::nodes2d::NodeTree` in `haylen/2d/nodes/NodeTree.hpp`. It lives in a shared pointer that `NodeTree::create()` makes. Its `Node` handles watch the tree, so a handle reports `isValid()` as `false` once its node or its tree is gone.

- The struct `NodeProperties` describes a new node, and `NodeContent` what it shows.
- The class `NodeSerializer` reads and writes descriptions, and `Prefab` holds a checked description.
- The tree emits `destroyed` for every node that goes, and `onDestroyed(node, slot)` calls a slot for one node.

```cpp
#include <memory>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/nodes/NodeTree.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Scene.hpp"

class Garage final : public haylen::core::Scene {
  public:
    void enter(haylen::core::Engine& engine) override {
        haylen::nodes2d::Node tank = tree->add({.name = "tank", .position = {400.0F, 300.0F}});
        turret = tank.add({.name = "turret", .position = {0.0F, -8.0F}, .content = {.value = haylen::nodes2d::NodeContent::Text{.font = engine.getDefaultFont(), .content = "Turret"}}});
    }

    void update(haylen::core::Engine& engine, float deltaSeconds) override {
        turret.setRotation(turret.getRotation() + deltaSeconds);
        tree->update(deltaSeconds, 1.0F, &engine.getJobs());
    }

    void render(haylen::core::Engine& engine) override {
        engine.getRenderer2D().beginWorld(camera);
        tree->draw(engine.getRenderer2D());
    }

  private:
    std::shared_ptr<haylen::nodes2d::NodeTree> tree = haylen::nodes2d::NodeTree::create();
    haylen::nodes2d::Node turret;
    haylen::graphics2d::Camera camera;
};
```
