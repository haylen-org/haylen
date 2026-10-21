# 🌳 Nodes

The module `haylen.nodes2d` keeps composed 2D objects as retained trees, such as a tank with its turret, a character with a sword in its hand or a unit with a bar over its head. This guide explains when a tree is the right tool, how a tree updates and draws, how nodes work with the other modules, and how to keep large trees fast. The [`haylen.nodes2d` reference](../lua-api/nodes2d.md) lists the complete API.

## When to use a tree

Draw calls of [`haylen.graphics2d`](../lua-api/graphics2d.md) suit things an app draws differently every frame, and sprite batches suit many independent sprites of one texture. A tree suits objects made of parts that move together: each node has a transform relative to its parent, and a color, a visibility and a layer that its descendants inherit, so moving, tinting or hiding a node moves, tints or hides its whole subtree. A node shows at most one thing, a sprite, a block of text, a nine-slice frame or particles, and a part that shows nothing groups its children.

A tree costs two calls from Lua per frame however many nodes it holds: `tree:update(dt)` brings the world state of the changed nodes up to date, and `tree:draw()` records every node in C++ and leaves out the nodes outside the view of the canvas.

## How a tree works

Nodes live in the arrays of their tree in the order of a walk from the roots, parents before children, which is also the order they draw in. A node is a handle that keeps its tree alive, and every member of a destroyed node raises `The node was destroyed.`, so code that keeps a node after it went fails at once instead of drawing a stale object. A node reports `'destroyed'` to its listeners right before it goes, and a listener ends with its owner like every listener of the engine.

The world transform of a node is the transform of its parent times its own, made of `x`, `y`, `rotation`, `scaleX` and `scaleY`, and `node:toWorld` and `node:toLocal` convert points between the two. The update recomputes only the nodes that changed and their descendants, so a still scene costs almost nothing.

## Prefabs and descriptions

A description is a table, or the JSON of a `.prefab` file, with the local state of a node, what it shows and its children. The function `assets.load` reads a `.prefab` file into a checked `Prefab` on a worker thread, `tree:instantiate` builds a subtree from it with overrides, and `nodes2d.save` writes a subtree back into a description, so levels and editors keep their objects as data.

## Nodes and the other modules

- **Physics.** A node with a `body` of [`haylen.physics2d`](../lua-api/physics2d.md) takes the position and rotation of the body on every update, between the last two steps of a world that interpolates, so a physics object and its parts draw smoothly at any frame rate.
- **Tweens.** The number and color properties of a node are native properties, so [`haylen.tween`](../lua-api/tween.md) animates them in C++ without a call into Lua per frame.
- **Particles.** A node that shows particles advances them on update and emits them where the node is, turned and scaled with it.
- **Rendering.** The draw gathers neighbouring sprite nodes of one texture and draw order into one batch, as the [rendering guide](../graphics/rendering.md#node-trees) explains, so a tree of the sprites of an atlas costs a few draw items however deep it is.

## Performance

A tree keeps its nodes in contiguous arrays and updates large trees on the job system of the engine, and the draw tests only the bounds of a culled node. The [performance section](../lua-api/nodes2d.md#performance) of the reference gives the times of a tree of 100,000 sprite nodes. Bulk reads and writes of transforms, `tree:readTransforms` and `tree:writeTransforms`, move many nodes through a float buffer of [`haylen.collections`](../lua-api/collections.md#float-buffers) in one call when an app simulates them itself.

## C++

The same tree is `haylen::nodes2d::NodeTree` in `haylen/2d/nodes/NodeTree.hpp`, with `Node` handles, `NodeProperties` for new nodes, `NodeContent` for what a node shows, `NodeSerializer` for descriptions and `Prefab` for checked descriptions. The [C++ section](../lua-api/nodes2d.md#c) of the reference shows a scene that builds and draws a tree.
