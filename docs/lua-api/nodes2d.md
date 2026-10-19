# haylen.nodes2d

The module `haylen.nodes2d` keeps a retained tree of 2D nodes. Each node has a local transform relative to its parent, so children follow their parents, such as a turret on a tank, a sword in a hand or a bar over a unit, and a color, a visibility and a layer that its descendants inherit. A node shows at most one thing: a sprite, a block of text, a nine-slice frame or particles. The tree updates every world transform in one pass and draws every node in one call, leaving out the nodes outside the view, so a scene of thousands of composed objects costs two calls from Lua per frame. Subtrees load from `.prefab` files and from descriptions, and save back to descriptions. The [nodes guide](../2d/nodes.md) explains when to use a tree and how it works with the other modules.

```lua
local nodes2d = require('haylen.nodes2d')
```

## How a tree works

- Nodes live in the arrays of their tree in the order of a walk from the roots, parents before their children and siblings in the order they were added, which is also the order they draw in. A node at the root has no parent.
- The world transform of a node is the world transform of its parent times its own transform, made of `x`, `y`, `rotation`, `scaleX` and `scaleY`. Its world color is the product of its color and the colors of its ancestors, a hidden node hides its descendants, and its world layer adds its `layer` to the layers of its ancestors.
- [`tree:update(dt)`](#treeupdatedt) brings the world state of every node whose local state changed, or whose ancestor's did, up to date, moves the nodes that follow bodies and advances the particles the nodes hold. [`tree:draw()`](#treedraw) first applies the changes made since that update, so a node moved after the update still draws in its new place. The functions `node:toWorld`, `node:toLocal`, `node:worldTransform` and the `world` properties read the state of the last update or draw.
- The node places what it shows. A sprite keeps its own `x`, `y`, `rotation`, `scaleX` and `scaleY` inside the node, so they offset it from the node, and the world transform of the node places the result. Text and frames sit on the node. The world color of the node multiplies their colors, its world layer adds to the layer of their draw order, and its `depth` and `sortOffset` add to theirs. Sprites, text and frames take the rotation and the scale of the world transform, so a node under a parent with an unequal scale and a rotation shows them without the shear such a transform holds.
- A draw leaves out every node whose bounds lie outside the area the open canvas shows, which [`tree.drawnCount`](#nodetree) and `tree.culledCount` report. Particles are never left out, and draw where their emitter puts them.
- Trees of 4096 nodes or more update the subtrees of their roots on the worker pool, gathered into groups of at least 1024 nodes, while the frame thread takes its share.

## Functions

### nodes2d.newTree()

Creates an empty `NodeTree`.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local nodes2d = require('haylen.nodes2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local tree = nodes2d.newTree()
local tank = tree:add({name = 'tank', x = 0, y = 0, sprite = graphics2d.newSprite(graphics.whiteTexture(), {width = 80, height = 48, color = '#FF3A7D44'})})
local turret = tank:add({name = 'turret', x = 10, sprite = graphics2d.newSprite(graphics.whiteTexture(), {width = 60, height = 12, pivotX = 0, color = '#FF2B5B33'})})

scene.push({
    update = function(self, dt)
        tank.rotation = tank.rotation + dt * 0.3
        turret.rotation = turret.rotation - dt
        tree:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        tree:draw()
        local x, y = turret:toWorld(60, 0)
        graphics2d.drawCircle(x, y, 4, '#FFFFFF40')
    end,
})
```

### nodes2d.newPrefab(description)

Creates a `Prefab` from a [description](#descriptions) table, which it checks at once. A `.prefab` file in the package loads as a prefab with `assets.load(path)`, checked on a worker thread. A description with an error raises a message that names the node by its path of names and the key at fault, such as `The key "speed" of the node "tank/turret" is unknown.`.

```lua
local nodes2d = require('haylen.nodes2d')

local marker = nodes2d.newPrefab({name = 'marker', children = {{name = 'label', y = -24, text = {text = 'Here', size = 20, anchor = {0.5, 1}}}}})
local tree = nodes2d.newTree()
tree:instantiate(marker, {x = 200, y = 100})
```

### nodes2d.save(node)

Returns the [description](#descriptions) of a node and its descendants as a table, which `tree:load` and `nodes2d.newPrefab` read back and the `json` module of Varn writes to a file. Values that equal their defaults are left out. Textures appear by the package path they were loaded from, fonts by the path given in their description, and particles by the path of their effect file, so content made in code cannot be saved and raises an error that names the node, such as `The node "hero/aura" shows particles that no effect file names, so it cannot be saved.`.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local json = require('json')
local nodes2d = require('haylen.nodes2d')

local tree = nodes2d.newTree()
local flag = tree:add({name = 'flag', x = 40, sprite = graphics2d.newSprite(assets.texture('props/flag.png'))})
print(json.encode(nodes2d.save(flag)))
```

## Descriptions

A description is a table, or the JSON of a `.prefab` file, with the local state of a node, what it shows and its `children`, a list of descriptions:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `name` | string | `''` | The name that paths and `find` use. |
| `x`, `y` | number | `0` | The position relative to the parent. |
| `rotation` | number | `0` | The rotation in radians. |
| `scaleX`, `scaleY` | number | `1` | The scale. |
| `color` | color | `'#FFFFFFFF'` | The color, which multiplies the colors of the descendants. |
| `visible` | boolean | `true` | Whether the node and its descendants show. |
| `layer` | integer | `0` | Added to the layers of the ancestors. |
| `depth`, `sortOffset` | number | `0` | Added to the depth and the sort offset of what the node shows. |
| `sprite` | table | none | A sprite: `texture` (its package path), `filter`, `wrap`, `source` (`[x, y, width, height]`), `x`, `y`, `width`, `height`, `scaleX`, `scaleY`, `pivotX`, `pivotY`, `rotation`, `color`, `flipHorizontal`, `flipVertical` and `layer`. |
| `text` | table | none | A block of text: `text`, `font` (a package path, the default font when absent), `size`, `color`, `outlineWidth`, `outlineColor`, `shadowOffset` (`[x, y]`), `shadowColor`, `shadowBlur`, `align`, `maxWidth`, `maxHeight`, `minScale`, `wrap`, `lineSpacing`, `anchor` (`[x, y]`) and `layer`, where `minScale` fits the text into its box as the [text style](graphics2d.md#graphics2ddrawtextfont-text-x-y-style) describes. |
| `frame` | table | none | A nine-slice frame: `texture`, `filter`, `wrap`, `source`, `borders` (`[left, top, right, bottom]`), `fill`, `width`, `height`, `pivotX`, `pivotY`, `color`, `borderScale` and `layer`. |
| `particles` | string | none | The package path of a `.particles` effect file. |
| `children` | list | none | The descriptions of the children, in order. |

A node shows at most one of `sprite`, `text`, `frame` and `particles`, and more raise `The node "<path>" shows more than one of "sprite", "text", "frame" and "particles".`

```json
{
    "name": "tank",
    "sprite": {"texture": "units/tank.png", "filter": "linear", "pivotY": 0.7},
    "children": [
        {"name": "turret", "y": -8, "sprite": {"texture": "units/turret.png", "filter": "linear", "pivotX": 0.2}},
        {"name": "label", "y": -60, "text": {"text": "Tank", "size": 18, "anchor": [0.5, 1]}},
        {"name": "smoke", "x": -30, "particles": "effects/smoke.particles"}
    ]
}
```

## NodeTree

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `size` | integer | read | The number of nodes. |
| `drawnCount` | integer | read | The nodes the last draw drew. |
| `culledCount` | integer | read | The nodes the last draw left out because they lay outside the view. |

Two tree values compare equal with `==` when they refer to the same tree.

### tree:add(properties)

Adds a node at the root, after the roots the tree has, and returns its `Node`. The table `properties` takes these keys, and any other key raises `Unknown option "<key>".`:

| Key | Type | Meaning |
| --- | --- | --- |
| `name`, `x`, `y`, `rotation`, `scaleX`, `scaleY`, `color`, `visible`, `layer`, `depth`, `sortOffset` | | The local state, as in a [description](#descriptions). |
| `sprite` | Sprite | A [`Sprite`](graphics2d.md#sprite) that the node shows, copied into the node. |
| `text` | string or table | Text in the default font, or a table with `text`, `font` (a `Font`), the style keys of [`graphics2d.drawText`](graphics2d.md#graphics2ddrawtextfont-text-x-y-style) and the draw order keys. |
| `frame` | table | A frame with `slice` (a [`NineSlice`](graphics2d.md#nineslice)), `width`, `height`, `pivotX`, `pivotY` (both 0.5 by default), `color`, `borderScale` and the draw order keys. |
| `particles` | ParticleEmitter, ParticleSystem or string | Particles that the node places, or the package path of an effect file. |
| `body` | Body | A [`Body`](physics2d.md#body) that the node follows, as [`node.body`](#node) does. |
| `data` | table | The table of the node that [`node.data`](#node) returns. |
| `children` | list | Properties tables of children, added in order. |

A sprite without a texture raises `A sprite node needs a texture.`, a frame without a nine-slice raises `A frame needs the nine-slice it draws in "slice".`, more than one kind of content raises `A node shows one of "sprite", "text", "frame" and "particles".`, and a tree of 16777216 nodes raises `A node tree holds at most 16777216 nodes.`.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local nodes2d = require('haylen.nodes2d')

local tree = nodes2d.newTree()
local panel = graphics2d.newNineSlice(assets.texture('ui/panel.png'), {borders = {12, 12, 12, 12}})
local unit = tree:add({
    name = 'knight',
    x = 300,
    y = 200,
    sprite = graphics2d.newSprite(assets.texture('units/knight.png'), {pivotY = 1}),
    data = {health = 10},
    children = {
        {name = 'bar', y = -90, frame = {slice = panel, width = 64, height = 14}},
        {name = 'name', y = -110, text = {text = 'Knight', size = 18, anchor = {0.5, 1}}},
        {name = 'dust', particles = 'effects/dust.particles'},
    },
})
print(unit.data.health, tree.size)
```

### tree:instantiate(prefab, overrides, parent)

Builds the subtree of a `Prefab` under `parent`, a `Node` of the tree, or at the root without one, and returns its root. The keys of the table `overrides`, which takes the keys of a [description](#descriptions), replace the keys of the root description, such as its place and its name. Assets of the description load through `haylen.assets` and its cache.

```lua
local assets = require('haylen.assets')
local nodes2d = require('haylen.nodes2d')

local tree = nodes2d.newTree()
local tank = assets.load('prefabs/tank.prefab')
for index = 1, 5 do
    tree:instantiate(tank, {name = 'tank' .. index, x = index * 160, y = 300})
end
```

### tree:load(description, parent)

Builds the node of a [description](#descriptions) table and its descendants under `parent`, or at the root, and returns it, which reopens what `nodes2d.save` wrote.

```lua
local nodes2d = require('haylen.nodes2d')

local tree = nodes2d.newTree()
local board = tree:load({name = 'board', children = {{name = 'title', text = {text = 'Scores', size = 40}}}})
print(board:find('title').text)
```

### tree:find(path)

Returns the node at a path of names from the roots separated by slashes, such as `'tank/turret'`, or `nil`.

```lua
local nodes2d = require('haylen.nodes2d')

local tree = nodes2d.newTree()
tree:add({name = 'tank', children = {{name = 'turret'}}})
print(tree:find('tank/turret').path, tree:find('tank/wheel'))
```

### tree:roots()

Returns the nodes at the root, in order.

```lua
local nodes2d = require('haylen.nodes2d')

local tree = nodes2d.newTree()
tree:add({name = 'ground'})
tree:add({name = 'sky'})
for _, root in ipairs(tree:roots()) do
    print(root.name)
end
```

### tree:update(dt)

Brings the world state of the changed nodes up to date, moves the nodes that follow bodies between the last two steps of their worlds by `haylen.interpolation()`, and advances the particles of the nodes by `dt`, placed, turned and scaled with their nodes. The scene that calls it decides the process mode and the pause, like `emitter:update(dt)`.

```lua
local nodes2d = require('haylen.nodes2d')
local scene = require('haylen.scene')

local tree = nodes2d.newTree()
local wheel = tree:add({name = 'wheel'})

scene.push({
    update = function(self, dt)
        wheel.rotation = wheel.rotation + dt * 2
        tree:update(dt)
    end,
})
```

### tree:draw()

Draws every visible node into the open canvas, parents before their children, and leaves out the nodes whose bounds lie outside its view. Wrap it in [`graphics2d.pushLayerOffset`](graphics2d.md#graphics2dpushlayeroffsetoffset) to raise the whole tree above other draws.

```lua
local graphics2d = require('haylen.graphics2d')
local nodes2d = require('haylen.nodes2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local tree = nodes2d.newTree()
tree:add({text = 'Hello'})

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.pushLayerOffset(10)
        tree:draw()
        graphics2d.popLayerOffset()
        print(tree.drawnCount, tree.culledCount)
    end,
})
```

### tree:clear()

Destroys every node, calling the `destroyed` listeners of each one.

```lua
local nodes2d = require('haylen.nodes2d')

local tree = nodes2d.newTree()
tree:add({name = 'a'})
tree:clear()
print(tree.size)
```

### tree:writeTransforms(nodes, buffer)

Sets the `x`, `y` and `rotation` of each node of the list `nodes` from three values of the [`FloatBuffer`](collections.md#float-buffers), in order, which moves many nodes in one call. A buffer with fewer than three values for each node raises `The values hold three floats for each node: x, y and rotation.`.

### tree:readTransforms(nodes, buffer)

Copies the world position and rotation of each node into three values of the buffer, as of the last update.

```lua
local collections = require('haylen.collections')
local nodes2d = require('haylen.nodes2d')

local tree = nodes2d.newTree()
local units = {}
for index = 1, 100 do
    units[index] = tree:add({})
end
local places = collections.newFloatBuffer(300)
for index = 1, 100 do
    places[index * 3 - 2] = index * 20
end
tree:writeTransforms(units, places)
tree:update(0)
local world = collections.newFloatBuffer(300)
tree:readTransforms(units, world)
print(world[298])
```

## Node

A `Node` is a handle to a node of a tree. It keeps its tree alive, and every member of a destroyed node raises `The node was destroyed.`, except `valid`. Two node values compare equal with `==` when they refer to the same node. The number properties and `color` are native properties, so [tweens](tween.md) animate them without running Lua.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `name` | string | read and write | The name in paths. |
| `x`, `y`, `rotation`, `scaleX`, `scaleY` | number | read and write | The local transform. |
| `position` | Vec2 | read and write | The local position. |
| `color` | Color | read and write | The local color. |
| `visible` | boolean | read and write | Whether the node and its descendants show. |
| `layer` | integer | read and write | Added to the layers of the ancestors. |
| `depth`, `sortOffset` | number | read and write | Added to the depth and sort offset of what the node shows. |
| `worldColor`, `worldLayer`, `worldVisible` | Color, integer, boolean | read | The inherited state as of the last update. |
| `parent` | Node or nil | read and write | The parent, or `nil` at the root. Setting it moves the node and its subtree under another node of the tree, or to the root with `nil`, keeping its local transform. A node under itself or one of its descendants raises `A node cannot move under itself or under one of its descendants.`. |
| `childCount` | integer | read | The number of children. |
| `path` | string | read | The names from the root, such as `'tank/turret'`. |
| `content` | string | read | What the node shows: `'none'`, `'sprite'`, `'text'`, `'frame'` or `'particles'`. |
| `sprite` | Sprite | read and write | A copy of the sprite the node shows, or a new sprite to show. Reading a node without a sprite raises `The node shows no sprite.`. |
| `text` | string or nil | read and write | The text the node shows. Setting it keeps the font and the style, and a node without text shows it in the default font. |
| `body` | Body or nil | read and write | A body whose position and rotation in the world the node takes on every update, between the last two steps when its world interpolates, whatever its parent. The tree holds the body, which keeps its world alive, until the node stops following with `nil`. |
| `data` | table | read | A table for the app, the same for every handle of the node, which goes with the node. |
| `key` | integer | read | A number that names the node among every node its tree held. |
| `valid` | boolean | read | Whether the node exists. |

```lua
local nodes2d = require('haylen.nodes2d')
local tween = require('haylen.tween')

local tree = nodes2d.newTree()
local door = tree:add({name = 'door', text = 'Door'})
door.data.locked = true
tween.to(door, 0.5, {x = 120, color = '#FFFFC040'})
print(door.content, door.data.locked, door.valid)
```

### node:add(properties)

Adds a child after the children of the node, with the keys of [`tree:add`](#treeaddproperties), and returns it.

```lua
local nodes2d = require('haylen.nodes2d')

local tree = nodes2d.newTree()
local arm = tree:add({name = 'arm'})
local hand = arm:add({name = 'hand', x = 30})
print(hand.path)
```

### node:find(path)

Returns the descendant at a path of names below the node, or `nil`.

### node:children()

Returns the children of the node, in order.

```lua
local nodes2d = require('haylen.nodes2d')

local tree = nodes2d.newTree()
local menu = tree:add({name = 'menu', children = {{name = 'play'}, {name = 'quit'}}})
print(menu:find('quit').name, #menu:children())
```

### node:show(content)

Replaces what the node shows with one of the keys `sprite`, `text`, `frame` or `particles` of the table `content`, as `tree:add` takes them, or shows nothing without a table.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local nodes2d = require('haylen.nodes2d')

local tree = nodes2d.newTree()
local slot = tree:add({name = 'slot'})
slot:show({sprite = graphics2d.newSprite(graphics.whiteTexture(), {width = 32, height = 32})})
slot:show()
print(slot.content)
```

### node:toWorld(x, y) and node:toLocal(x, y)

Convert a point of the node to the world and back, through the world transform of the last update.

### node:worldTransform()

Returns the world transform of the node as a `Transform2D`.

```lua
local nodes2d = require('haylen.nodes2d')

local tree = nodes2d.newTree()
local base = tree:add({x = 100, rotation = math.pi / 2})
local tip = base:add({x = 50})
tree:update(0)
print(tip:toWorld(0, 0))
print(tip:toLocal(100, 50))
print(tip:worldTransform():apply(0, 0))
```

### node:destroy()

Destroys the node and its descendants, deepest first, calling their `destroyed` listeners.

### node:on(event, listener, options)

Listens to the event `'destroyed'`, which calls the listener with the node right before it goes, and returns a `Connection`. The table `options` takes `owner`, which ends the listener with its owner, and without one it takes the default owner. Another event raises `Unknown node event "<event>". Nodes report "destroyed".`.

```lua
local nodes2d = require('haylen.nodes2d')
local scene = require('haylen.scene')

local tree = nodes2d.newTree()
local crate = tree:add({name = 'crate', children = {{name = 'lid'}}})

scene.push({
    enter = function(self)
        crate:on('destroyed', function(node) print(node.name .. ' broke.') end, {owner = self})
        crate:destroy()
    end,
})
```

### Animating nodes

The function [`animator:apply(node)`](animation2d.md#animatorapplysprite) shows the frame of an animator on the sprite of a node.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local nodes2d = require('haylen.nodes2d')
local scene = require('haylen.scene')

local sheet = assets.texture('units/warrior.png')
local animator = animation2d.newAnimator()
animator:add('run', animation2d.fromGrid(sheet, {frameWidth = 32, frameHeight = 32, framesPerSecond = 12}))
animator:play('run')
local tree = nodes2d.newTree()
local warrior = tree:add({sprite = graphics2d.newSprite(sheet)})

scene.push({
    update = function(self, dt)
        animator:update(dt)
        animator:apply(warrior)
        tree:update(dt)
    end,
})
```

## Prefab

A `Prefab` is a checked description, from a `.prefab` file loaded with `assets.load` or from `nodes2d.newPrefab`. Its property `description` returns the description as a table. Two prefab values compare equal with `==` when they are the same prefab.

```lua
local assets = require('haylen.assets')

local tank = assets.load('prefabs/tank.prefab')
print(tank.description.name)
```

## Performance

The class `nodes2d::NodeTree` keeps its nodes in contiguous arrays and walks them in order, so an update touches only the nodes that changed and their descendants, and a draw records every node in C++ without a call from Lua per node. The command `haylen bench --suite nodes` times a tree of 100,000 sprite nodes, a thousand roots with 99 children each, on the headless host:

| Scene | Average ms per frame |
| --- | --- |
| Update after every root moved, one thread | 1.015 |
| Update after one root in a hundred moved, one thread | 0.189 |
| Update after every root moved, job system | 0.208 |
| Update after one root in a hundred moved, job system | 0.075 |
| Draw with every node in view | 2.388 |
| Draw with 361 nodes in view and the rest culled | 0.400 |

These are the numbers of an Apple M5 Pro in Release with 15 workers. A draw records the neighbouring sprite nodes of one texture and order as one batch, which converts its instances on the worker pool from 8,192 sprites on, and a culled node costs a test of its bounds.

## C++

The same tree is `haylen::nodes2d::NodeTree` in `haylen/2d/nodes/NodeTree.hpp`, which lives in a shared pointer that `NodeTree::create()` makes and its `Node` handles watch, so a handle reports `isValid()` false once its node or its tree is gone. The struct `NodeProperties` describes a new node, `NodeContent` what it shows, `NodeSerializer` reads and writes descriptions, and `Prefab` holds a checked description. The tree emits `destroyed` for every node that goes, and `onDestroyed(node, slot)` calls a slot for one node.

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
