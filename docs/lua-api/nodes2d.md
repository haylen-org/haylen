# 📖 `haylen.nodes2d`

The module `haylen.nodes2d` keeps retained trees of 2D nodes, for objects made of parts that move together. The [nodes guide](../2d/nodes.md) explains how a tree works, when to use one and how nodes work with physics, tweens, animation and particles.

```lua
local nodes2d = require('haylen.nodes2d')
```

## Functions

### nodes2d.newTree()

Creates an empty [`NodeTree`](#nodetree).

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

Creates a [`Prefab`](#prefab) from a [description](#descriptions) table and checks it at once. A `.prefab` file loads as a prefab with `assets.load(path)` instead, checked on a worker thread. A description with a mistake raises an error that names the node by its path of names and names the key at fault, such as `The key "speed" of the node "tank/turret" is unknown.`. The [errors of descriptions](#errors-of-descriptions) list every message.

```lua
local nodes2d = require('haylen.nodes2d')

local marker = nodes2d.newPrefab({name = 'marker', children = {{name = 'label', y = -24, text = {text = 'Here', size = 20, anchor = {0.5, 1}}}}})
local tree = nodes2d.newTree()
tree:instantiate(marker, {x = 200, y = 100})
```

### nodes2d.save(node)

Returns the [description](#descriptions) of a node and its descendants as a table. The functions `tree:load` and `nodes2d.newPrefab` read it back, and the `json` module of Varn writes it to a file. Values equal to their defaults are left out. A texture appears by the package path it was loaded from, a font by the path its description gave and particles by the path of their effect file.

Content made in code has no path, so it cannot be saved. It raises one of these errors, which name the node:

- `The node "<path>" shows a texture that no package path names, so it cannot be saved.`
- `The node "<path>" shows text in a font that no package path names, so it cannot be saved.`
- `The node "<path>" shows a frame of separate pieces, which only a source and borders describe, so it cannot be saved.`
- `The node "<path>" shows particles that no effect file names, so it cannot be saved.`

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

A description is a table, or the JSON of a `.prefab` file, with the local state of a node, what it shows and its `children`. These are its keys:

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
| `text` | table | none | A block of text: `text`, `font` (a package path, the default font when absent), `size`, `color`, `outlineWidth`, `outlineColor`, `shadowOffset` (`[x, y]`), `shadowColor`, `shadowBlur`, `align`, `maxWidth`, `maxHeight`, `minScale`, `wrap`, `lineSpacing`, `anchor` (`[x, y]`) and `layer`. The key `minScale` fits the text into its box as the [text style](graphics2d.md#graphics2ddrawtextfont-text-x-y-style) describes. |
| `frame` | table | none | A nine-slice frame: `texture`, `filter`, `wrap`, `source`, `borders` (`[left, top, right, bottom]`), `fill`, `width`, `height`, `pivotX`, `pivotY`, `color`, `borderScale` and `layer`. |
| `particles` | string | none | The package path of a `.particles` effect file. |
| `children` | list | none | The descriptions of the children, in order. |

A node shows at most one of `sprite`, `text`, `frame` and `particles`.

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

### Errors of descriptions

Every error names the node by its path of names, written here as `<path>`, and the key at fault, written as `<key>`. A key of a sprite, text or frame names its part too, such as `The key "fill" of the frame of the node "<path>"`.

- `The description of the node "<path>" must be an object.` and `The sprite of the node "<path>" must be an object.`, with `text` and `frame` in place of `sprite`.
- `The key "<key>" of the node "<path>" is unknown.`
- `The key "<key>" of the node "<path>" must be <kind>.` for a value of the wrong type, where `<kind>` is `a number`, `true or false`, `a string`, `a color such as "#FFFFFFFF"`, `a list of two numbers` or `a list of four numbers: x, y, width and height`.
- `The key "filter" of the sprite of the node "<path>" must be "nearest" or "linear".` and `The key "wrap" of the sprite of the node "<path>" must be "clamp", "repeat" or "mirror".`, the same for frames.
- `The key "children" of the node "<path>" must be a list of descriptions.`
- `The node "<path>" shows more than one of "sprite", "text", "frame" and "particles".`
- `The sprite of the node "<path>" needs the path of its texture.` and `The frame of the node "<path>" needs the path of its texture.`
- `The key "minScale" of the text of the node "<path>" must be above 0 and at most 1.`
- `The key "align" of the text of the node "<path>" must be "start", "end", "left", "center", "right" or "fill".`
- `The key "fill" of the frame of the node "<path>" must be "stretch" or "tile".`
- `The particles of the node "<path>" need the path of their effect file.`

## NodeTree

A `NodeTree` holds nodes, which `nodes2d.newTree` creates. Two tree values compare equal with `==` when they refer to the same tree.

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `size` | integer | read | The number of nodes. |
| `drawnCount` | integer | read | The nodes the last draw drew. |
| `culledCount` | integer | read | The nodes the last draw left out because they lay outside the view. |

### tree:add(properties)

Adds a node at the root, after the roots the tree already has, and returns its [`Node`](#node). The table `properties` takes these keys:

| Key | Type | Meaning |
| --- | --- | --- |
| `name`, `x`, `y`, `rotation`, `scaleX`, `scaleY`, `color`, `visible`, `layer`, `depth`, `sortOffset` | | The local state, as in a [description](#descriptions). |
| `sprite` | Sprite | A [`Sprite`](graphics2d.md#sprite) that the node shows. The node keeps a copy. |
| `text` | string or table | Text in the default font, or a table with `text`, `font` (a `Font`), the style keys of [`graphics2d.drawText`](graphics2d.md#graphics2ddrawtextfont-text-x-y-style) and the draw order keys. |
| `frame` | table | A frame with `slice` (a [`NineSlice`](graphics2d.md#nineslice)), `width`, `height`, `pivotX` and `pivotY` (both `0.5` by default), `color`, `borderScale` and the draw order keys. |
| `particles` | ParticleEmitter, ParticleSystem or string | Particles that the node places, or the package path of an effect file. |
| `body` | Body | A [`Body`](physics2d.md#body) that the node follows, as [`node.body`](#node) does. |
| `data` | table | The table that [`node.data`](#node) returns. |
| `children` | list | Properties tables of children, added in order. |

These errors can come up:

- Any other key raises `Unknown option "<key>".`
- A sprite without a texture raises `A sprite node needs a texture.`
- A frame without a nine-slice raises `A frame needs the nine-slice it draws in "slice".`, a nine-slice without a texture raises `A frame node needs a nine-slice with a texture.` and a negative size or a border scale that is not positive raises `A frame node needs a size of at least 0 and a positive border scale.`
- More than one kind of content raises `A node shows one of "sprite", "text", "frame" and "particles".`
- A tree holds at most 16,777,216 nodes, and one more raises `A node tree holds at most 16777216 nodes.`

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

Builds the subtree of a [`Prefab`](#prefab) and returns its root. The node goes under `parent`, a `Node` of the tree, or at the root without one. The optional table `overrides` takes the keys of a [description](#descriptions) and replaces those keys of the root description, such as its place and its name. Overrides that are not a table of keys raise `The overrides of a prefab must be an object of the keys of a description.`. The assets of the description load through `haylen.assets` and its cache.

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

Builds the node of a [description](#descriptions) table and its descendants under `parent`, or at the root without one, and returns it. It reopens what `nodes2d.save` wrote.

```lua
local nodes2d = require('haylen.nodes2d')

local tree = nodes2d.newTree()
local board = tree:load({name = 'board', children = {{name = 'title', text = {text = 'Scores', size = 40}}}})
print(board:find('title').text)
```

### tree:find(path)

Returns the node at a path of names from the roots, separated by slashes, such as `'tank/turret'`, or `nil` when no node has that path.

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

Brings the world state of the changed nodes up to date. It also moves the nodes that follow bodies to the place between the last two steps of their worlds, by `haylen.interpolation()`, and advances the particles of the nodes by `dt` seconds. The scene that calls it decides the process mode and the pause, as for `emitter:update(dt)`.

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

Destroys every node and calls the `destroyed` listeners of each one.

```lua
local nodes2d = require('haylen.nodes2d')

local tree = nodes2d.newTree()
tree:add({name = 'a'})
tree:clear()
print(tree.size)
```

### tree:writeTransforms(nodes, buffer)

Sets the `x`, `y` and `rotation` of each node of the list `nodes` from three values of the [`FloatBuffer`](collections.md#float-buffers), in order. It moves many nodes in one call. A buffer with fewer than three values for each node raises `The values hold three floats for each node: x, y and rotation.`.

### tree:readTransforms(nodes, buffer)

Copies the world position and rotation of each node of the list `nodes` into three values of the buffer, as of the last update. A buffer with fewer than three values for each node raises `The values hold three floats for each node: x, y and rotation.`.

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

A `Node` is a handle to a node of a tree, which `tree:add`, `node:add`, `tree:find` and the other functions of a tree return. It keeps its tree alive. Every member of a destroyed node raises `The node was destroyed.`, except `valid`. Two node values compare equal with `==` when they refer to the same node.

### Properties

The number properties and `color` are native properties, so [tweens](tween.md) animate them without running Lua.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `name` | string | read and write | The name in paths. |
| `x`, `y`, `rotation`, `scaleX`, `scaleY` | number | read and write | The local transform. |
| `position` | Vec2 | read and write | The local position. |
| `color` | Color | read and write | The local color. |
| `visible` | boolean | read and write | Whether the node and its descendants show. |
| `layer` | integer | read and write | Added to the layers of the ancestors. |
| `depth`, `sortOffset` | number | read and write | Added to the depth and the sort offset of what the node shows. |
| `worldColor`, `worldLayer`, `worldVisible` | Color, integer, boolean | read | The inherited state as of the last update. |
| `parent` | Node or nil | read and write | The parent, or `nil` at the root. Setting it moves the node and its subtree under another node of the tree, or to the root with `nil`, and keeps its local transform. A node set under itself or under one of its descendants raises `A node cannot move under itself or under one of its descendants.`. |
| `childCount` | integer | read | The number of children. |
| `path` | string | read | The names from the root, such as `'tank/turret'`. |
| `content` | string | read | What the node shows: `'none'`, `'sprite'`, `'text'`, `'frame'` or `'particles'`. |
| `sprite` | Sprite | read and write | Reading returns a copy of the sprite the node shows, and writing shows a new sprite. Reading a node without a sprite raises `The node shows no sprite.`. |
| `text` | string or nil | read and write | The text the node shows, or `nil`. Setting it keeps the font and the style, and a node without text shows it in the default font. |
| `body` | Body or nil | read and write | A body whose position and rotation in the world the node takes on every update, between the last two steps when its world interpolates, whatever its parent. The tree holds the body, which keeps its world alive, until the node stops following with `nil`. |
| `data` | table | read | A table for the app, the same for every handle of the node, which goes with the node. |
| `key` | integer | read | A number that names the node among every node its tree has held. |
| `valid` | boolean | read | Whether the node exists. |

```lua
local nodes2d = require('haylen.nodes2d')
local tween = require('haylen.tween')

local tree = nodes2d.newTree()
local door = tree:add({name = 'door', text = 'Door'})
door.data.locked = true
tween.to(door, 0.5, {x = 120, color = '#FFFFC040'})
print(door.content, door.data.locked, door.valid, door.key)
```

### node:add(properties)

Adds a child after the children the node already has, with the keys of [`tree:add`](#treeaddproperties), and returns it.

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
print(menu:find('quit').name, #menu:children(), menu.childCount)
```

### node:show(content)

Replaces what the node shows with one of the keys `sprite`, `text`, `frame` or `particles` of the table `content`, as `tree:add` takes them. Without a table, the node shows nothing. Any other key raises `Unknown option "<key>".`

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

### node:toWorld(x, y)

Returns the `x` and `y` in the world of a point of the node, through the world transform of the last update.

### node:toLocal(x, y)

Returns the `x` and `y` in the node of a point of the world, through the world transform of the last update.

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

Destroys the node and its descendants, deepest first, and calls their `destroyed` listeners.

### node:on(event, listener, options)

Listens to the event `'destroyed'`, which calls the listener with the node right before it goes, and returns a `Connection`. The optional table `options` takes `owner`, which ends the listener with its owner. Without one, the listener takes the default owner. Another event raises `Unknown node event "<event>". Nodes report "destroyed".`.

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

The function [`animator:apply(node)`](animation2d.md#animatorapplysprite) of `haylen.animation2d` shows the current frame of an animator on the sprite of a node.

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

A `Prefab` is a checked description. It comes from a `.prefab` file loaded with `assets.load` or from `nodes2d.newPrefab`. Two prefab values compare equal with `==` when they are the same prefab.

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `description` | table | read | The [description](#descriptions) as a table. |

```lua
local assets = require('haylen.assets')

local tank = assets.load('prefabs/tank.prefab')
print(tank.description.name)
```
