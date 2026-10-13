# Spatial

The module `haylen.spatial2d` finds things in space without testing every entity: spatial structures that answer area, circle, point, ray and nearest queries, the grid algorithms of 2D games, screen picking, and overlap worlds that report which boxes reach each other. This guide explains which structure to choose, how the grid algorithms fit with maps and generators, and when an overlap world replaces physics. The [`haylen.spatial2d` reference](../lua-api/spatial2d.md) lists the complete API.

## Choosing a structure

Every structure stores Lua values by their bounds or positions, so a query returns the entities themselves:

| Structure | Best for |
| --- | --- |
| `HashGrid` | Many entries of similar size that move every frame, such as enemies and bullets. |
| `QuadTree` | Entries of very different sizes spread over a known area, such as the objects of a level. |
| `AabbTree` | Many moving entries of any size over an unbounded area, where small moves inside the enlarged box of an entry cost nothing. |
| `KdTree` | Points that rarely change, when nearest-neighbor queries dominate, such as resources or spawn points. |

A game with physics already has these queries in the world of [`haylen.physics2d`](../lua-api/physics2d.md), and uses a structure of its own for entities without bodies, such as pickups, sounds or the targets of an ability.

## Grid algorithms

A `CellGrid` is a grid of integer cells, the same type that the generators of [`haylen.procedural2d`](../lua-api/procedural2d.md) return, and the grid algorithms work on it directly: ray casts and walks across cells, Bresenham lines and circles, a symmetric field of view for roguelike vision, visibility polygons for line-of-sight lighting, flood fills, connected regions and union-find. They suit tile maps, fog of war, area effects and the checks of generated levels, such as whether every room of a dungeon connects.

## Screen picking

A camera of [`haylen.graphics2d`](../lua-api/graphics2d.md) maps the cursor or a touch into world points and rays with `spatial2d.screenRay`, and a structure answers which values lie under them, so a click selects a unit without a physics body.

## Overlap worlds

An overlap world keeps entities that each show one set of boxes, such as the body, hit and grab boxes of a frame of a fighter, and lists the pairs of boxes that reach each other every fixed step. Layers and masks of boxes choose what reaches what, a depth reach keeps entities on far rows of a belt apart, and mirrored entities flip their boxes, so fighting and brawler games resolve their hits by their own rules without physics bodies. The [overlap world reference](../lua-api/spatial2d.md#overlapworld) describes boxes, contacts and their order.

## C++

The C++ types live in `haylen/2d/spatial/` in the namespace `haylen::spatial2d`: `HashGrid`, `QuadTree`, `AabbTree`, `KdTree`, `CellGrid`, `UnionFind`, `OverlapWorld`, `ScreenPicker` and the algorithms `GridRay`, `Bresenham`, `FieldOfView`, `VisibilityPolygon`, `FloodFill` and `ConnectedComponents`.
