# 🧭 Navigation

This guide shows how characters find their way and move through the world with the module `haylen.navigation2d`. It covers paths over tile grids, waypoint graphs and navigation meshes, the fields that guide many units at once, steering that makes motion smooth and crowds that never overlap. It explains how to pick a method, how the parts combine, how to keep long searches off the frame and what each part costs. Read it before you add enemies that chase, units that obey clicks or crowds that walk a town. The [`haylen.navigation2d` reference](../lua-api/navigation2d.md) lists every function, option and error.

## Planning and moving

Navigation has two jobs:

- Planning decides where to go: a route around walls from here to there. Grids, graphs and meshes plan routes, and Dijkstra maps and flow fields plan for many units at once.
- Moving decides how to go: speeding up, turning smoothly, slowing down at the goal and keeping clear of others. Steering agents move one character, and crowds move many together.

A unit usually does both. It plans a route once, when its goal changes, and it moves along the route every frame. Planning is the expensive part, so it happens rarely. Moving is cheap, so it happens every frame.

## Pick a method

| The game has | Use |
| --- | --- |
| A tile map and one unit at a time that plans a route, with terrain costs | [`grid:findPath`](../lua-api/navigation2d.md#gridfindpathstartx-starty-goalx-goaly-options) of a `NavGrid`, with A\* by default and weighted A\* to trade length for speed. |
| Large open grids where every cell costs the same | `grid:findPath` with jump point search. |
| Many long searches on a large grid that changes a little at a time | [`grid:hierarchicalPathfinder`](../lua-api/navigation2d.md#gridhierarchicalpathfinderoptions), which plans over clusters and updates only the clusters that changed. |
| Many units that head to the same goals, as in strategy and tower defense games | [`grid:flowField`](../lua-api/navigation2d.md#gridflowfieldgoals-options), which every unit reads in constant time. |
| Monsters that chase or flee the player and weigh several goals | [`grid:dijkstraMap`](../lua-api/navigation2d.md#griddijkstramapsources-options), whose `map:flee` turns a chase into an escape. |
| Waypoints that a designer places, roads, and doors that open and close | A [`NavGraph`](../lua-api/navigation2d.md#navgraph). |
| Open levels without a grid, with paths that hug corners and keep a radius clear | A [`NavMesh`](../lua-api/navigation2d.md#navmesh). |
| Smooth motion of one character toward a target | A [`SteeringAgent`](../lua-api/navigation2d.md#steeringagent), with seek, flee, arrive, flocking, obstacle avoidance and wander. |
| Hundreds or thousands of agents that must not overlap | A [`Crowd`](../lua-api/navigation2d.md#crowd), which solves optimal reciprocal collision avoidance for every agent each step. |

The choice between a grid, a graph and a mesh follows the shape of the world. A world made of tiles fits a grid, since its cells are the cells of the map. A world where only a few places matter, such as the stops of a bus line or the rooms of a house, fits a graph. A world of free shapes, such as a field with rocks and buildings, fits a mesh, whose paths are short and straight and keep large agents away from walls.

## Grids

A [`NavGrid`](../lua-api/navigation2d.md#navgrid) has one cell per tile. Each cell is walkable or blocked and has a cost, the price of entering it. A cost of 1 is plain ground, and higher costs make paths go around, such as a swamp of cost 4 next to a road of cost 1. A path never enters a blocked cell, and cells outside the grid count as blocked.

Cells count from 0 by column and row, like the cells of Tiled. Hexagonal and staggered grids use the same offset coordinates as the hexagonal and staggered maps of Tiled. So `map:worldToCell` of [`haylen.tiled`](../lua-api/tiled.md#mapworldtocellx-y) gives the cell of any world point.

```lua
local assets = require('haylen.assets')
local navigation2d = require('haylen.navigation2d')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local grid = navigation2d.newGrid(map.width, map.height)

-- Every cell of the water layer is blocked, and every cell of the sand layer is slow.
for row = 0, map.height - 1 do
    for column = 0, map.width - 1 do
        if map:tile('water', column, row) ~= 0 then
            grid:setWalkable(column, row, false)
        elseif map:tile('sand', column, row) ~= 0 then
            grid:setCost(column, row, 2)
        end
    end
end

local hero = {x = 100, y = 140}
local column, row = map:worldToCell(hero.x, hero.y)
local path, cost = grid:findPath(column, row, 40, 25)
print(path and #path, cost)
```

### How a search works

A path search starts at the start cell and grows outward, always from the cell whose path so far plus its guess of the distance left is lowest. This is A\*. The guess is called the heuristic. A heuristic that never guesses too long finds the cheapest path. The default heuristic is exact on open ground, so A\* finds the cheapest path and visits few cells.

Three options trade the quality of the path for speed:

- A `weight` above 1 makes the search trust its guess more. It visits fewer cells, and the path costs at most `weight` times the cheapest one.
- Jump point search, `jumpPoint = true`, skips over runs of open ground. It finds paths as cheap as A\* and visits far fewer cells on large open maps. It needs a square or staggered grid where every cell costs 1.
- A hierarchical path finder, described below, plans over large blocks of the grid first.

The property `grid.expandedCount` tells how many cells the last search visited, so the app can measure what an option saves.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(256, 256)
for row = 0, 200 do
    grid:setWalkable(128, row, false)
end

grid:findPath(0, 0, 255, 0)
local plain = grid.expandedCount
grid:findPath(0, 0, 255, 0, {jumpPoint = true})
print('A* visited ' .. plain .. ' cells, jump point search ' .. grid.expandedCount .. '.')
```

### Smooth paths

A grid path walks from cell to cell, so it zigzags on diagonals. The option `smooth = true` keeps only the cells where the path must turn, and a unit walks straight between them. [`grid:lineOfSight`](../lua-api/navigation2d.md#gridlineofsightfromx-fromy-tox-toy) tells whether a straight walk between two cells is clear, such as to skip the path entirely when the goal is in view. Smoothing looks at walls only, so a smoothed path may cross costly cells that the search avoided.

### Large grids that change

A [hierarchical path finder](../lua-api/navigation2d.md#hierarchicalpathfinder) splits the grid into square clusters, such as 16 by 16 cells, and finds the entrances between them. A search first plans over the small graph of entrances, and then finds the cells only along the chosen clusters. Long searches on large grids get much faster, and the paths stay within a few percent of the cheapest one.

When cells change, such as a wall that the player builds, call `router:update` with the cells that changed. It rebuilds only the clusters around them, so a change costs little however large the grid is.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(512, 512)
local router = grid:hierarchicalPathfinder({clusterSize = 32})

-- The player builds a wall, and only the clusters around it rebuild.
for column = 100, 140 do
    grid:setWalkable(column, 300, false)
end
router:update(100, 300, 140, 300)

local path = router:findPath(0, 0, 511, 511)
print(path and #path)
```

## Many units, one goal

A path search answers one unit. When many units head to the same place, as the creeps of a tower defense game do, one search per unit wastes work. A field answers every unit at once: it is computed once for the whole grid, and each unit reads its own cell.

- A [flow field](../lua-api/navigation2d.md#flowfield) stores, for every cell, the step toward the nearest goal. A unit reads the direction of its cell and moves that way. A thousand units cost one field and a thousand reads.
- A [Dijkstra map](../lua-api/navigation2d.md#dijkstramap) stores, for every cell, the cost of the walk to the nearest source. A unit steps downhill, toward lower values. Each source can carry a value, so a pile of gold that should pull harder than a lever gets a lower value.

Compute a field when the goals or the walls change, not every frame.

```lua
local navigation2d = require('haylen.navigation2d')
local scene = require('haylen.scene')

local tile = 32
local grid = navigation2d.newGrid(40, 30)
for row = 5, 25 do
    grid:setWalkable(20, row, false)
end
local base = {39, 15}
local field = grid:flowField({base})

local creeps = {}
for index = 1, 300 do
    creeps[index] = {x = 16 + (index % 5) * 8, y = 16 + index % 30 * tile}
end

scene.push({
    update = function(self, dt)
        for _, creep in ipairs(creeps) do
            local direction = field:direction(creep.x // tile, creep.y // tile)
            creep.x = creep.x + direction.x * 60 * dt
            creep.y = creep.y + direction.y * 60 * dt
        end
    end,
})
```

### Chase and flee

Dijkstra maps suit the monsters of a roguelike. A map whose source is the player makes a monster chase: it steps downhill. A flee map makes it run away. A flee map is not just the chase map turned upside down, which would send a monster into the nearest corner. [`map:flee`](../lua-api/navigation2d.md#mapfleecoefficient) scales the values and walks the map again, so the lowest cells are the far ones with open space beyond, and a fleeing monster runs along escape routes instead.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(30, 20)
local player = {x = 10, y = 10}
local goblin = {x = 14, y = 10, scared = true}

local danger = grid:dijkstraMap({player})
if goblin.scared then
    danger:flee()
end
local x, y = danger:next(goblin.x, goblin.y)
if x then
    goblin.x, goblin.y = x, y
end
print(goblin.x, goblin.y)
```

## Waypoint graphs

A [`NavGraph`](../lua-api/navigation2d.md#navgraph) is a set of points with connections between them. The designer places the points, such as the doors of a building, the stops of a train or the corners of a race track, and connects the ones a character may walk between. The app picks the id of each point, so ids can match the objects of a map.

A connection may go one way, such as a ledge to jump down from. A point has a weight that makes it costlier to pass, such as a guarded door. A point can be turned off with `graph:setEnabled`, and paths avoid it until it is turned on again, which suits doors and bridges that open and close.

```lua
local navigation2d = require('haylen.navigation2d')

local house = navigation2d.newGraph()
house:addPoint(1, 0, 0)
house:addPoint(2, 200, 0)
house:addPoint(3, 200, 150)
house:addPoint(4, 0, 150)
house:addPoint(5, 100, 75, 4)
house:connect(1, 2)
house:connect(2, 3)
house:connect(3, 4)
house:connect(4, 1)
house:connect(1, 5)
house:connect(5, 3)

-- The hall through point 5 is shorter, but its weight makes the walk around cheaper.
local route = house:findPath(1, 3)
print(table.concat(route, ' > '))

-- A locked door takes point 2 out until it opens again.
house:setEnabled(2, false)
route = house:findPath(1, 3)
print(table.concat(route, ' > '))
```

A character joins the graph at the point closest to it, from [`graph:closestPoint`](../lua-api/navigation2d.md#graphclosestpointx-y-includedisabled), and leaves it at the point closest to its goal. [`graph:distances`](../lua-api/navigation2d.md#graphdistancessource) gives the cost from one point to every other, such as to find the nearest open shop.

## Navigation meshes

A [`NavMesh`](../lua-api/navigation2d.md#navmesh) covers the walkable area of a level with triangles. Give it the outline of the area and the outlines of the obstacles, and it builds the triangles itself. The outlines come from anywhere: the collision objects of a Tiled map through [`map:objectOutlines`](../lua-api/tiled.md#mapobjectoutlineslayer), the shapes of physics bodies, or polygons that code makes. Obstacles may overlap each other and cross the outline.

A path through a mesh goes from triangle to triangle and is then pulled tight like a string, so it turns only at the corners of obstacles. Its points are world positions, not cells, so units walk straight lines between them.

A mesh path also knows the size of the agent. Give `mesh:findPath` the radius of the agent, and the path keeps that far from every wall. It skips passages narrower than the agent, and it turns a little away from each corner, so a large unit never scrapes along a wall. An agent pressed against a wall, or sent to a point on one, first steps to a place where it has room, so a click next to a wall still gives a path.

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local navigation2d = require('haylen.navigation2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local mesh = navigation2d.newNavMesh({{0, 0}, {1200, 0}, {1200, 800}, {0, 800}})
mesh:addObstacle({{300, 150}, {500, 150}, {500, 450}, {300, 450}})
mesh:addObstacle({{700, 300}, {900, 250}, {950, 600}, {650, 650}})

local ogre = navigation2d.newAgent({x = 100, y = 100, maxSpeed = 140})
local radius = 24

scene.push({
    update = function(self, dt)
        if input.mousePressed() then
            -- A click on an obstacle moves to the closest walkable point instead.
            local x, y = mesh:closestPoint(camera:screenToWorld(input.mousePosition()))
            self.route = mesh:findPath(ogre.position.x, ogre.position.y, x, y, radius)
            self.next = 2
        end
        local point = self.route and self.route[self.next]
        if point then
            local last = self.next == #self.route
            ogre:apply(last and ogre:arrive(point, 80) or ogre:seek(point), dt)
            if not last and (ogre.position - point):length() < 12 then
                self.next = self.next + 1
            end
        end
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawCircle(ogre.position.x, ogre.position.y, radius, '#FFB06040')
    end,
})
```

Moving an obstacle marks the mesh as dirty, and the next query rebuilds it. A rebuild costs much more than a query, so move obstacles rarely, such as a door that opens, and keep moving things, such as other characters, to a crowd instead. A large level builds in the background with [`navigation2d.buildNavMeshAsync`](../lua-api/navigation2d.md#navigation2dbuildnavmeshasyncboundary-obstacles).

## Steering

A path is a list of points. Walking it with fixed steps looks stiff: the unit turns sharply and stops at once. A [`SteeringAgent`](../lua-api/navigation2d.md#steeringagent) moves like a body with momentum. Each behavior returns the change of velocity it wants, the app adds the behaviors it uses, and `agent:apply` turns the velocity toward the sum, no faster than `maxForce` allows.

| Behavior | Use |
| --- | --- |
| `agent:seek` | Head for a point at full speed, such as the next point of a path. |
| `agent:arrive` | Head for a point and slow down to stop on it, such as the end of a path. |
| `agent:flee` | Run from a point. |
| `agent:separation`, `agent:alignment` and `agent:cohesion` | Flocking: keep apart, head the same way and stay together. |
| `agent:avoid` | Swerve around circles in the way, such as rocks. |
| `agent:wander` | Drift around smoothly with no goal, such as a fly or an idle animal. |

Weights mix the behaviors. A bird that seeks food with weight 1, keeps apart with weight 1.5 and follows the flock with weight 0.3 stays in its flock without bumping into the others.

```lua
local navigation2d = require('haylen.navigation2d')
local scene = require('haylen.scene')

local birds = {}
for index = 1, 30 do
    birds[index] = navigation2d.newAgent({x = index * 13 % 200, y = index * 29 % 200, maxSpeed = 110, seed = index})
end
local food = {600, 300}

scene.push({
    update = function(self, dt)
        local positions, velocities = {}, {}
        for index, bird in ipairs(birds) do
            positions[index] = bird.position
            velocities[index] = bird.velocity
        end
        for _, bird in ipairs(birds) do
            local force = bird:seek(food) + bird:separation(positions, 30) * 1.5 + bird:alignment(velocities) * 0.3 + bird:cohesion(positions) * 0.2
            bird:apply(force, dt)
        end
    end,
})
```

Each behavior is a call from Lua, so steering suits tens or a few hundred agents. Larger groups belong in a crowd, which moves every agent in C++.

## Crowds

A [`Crowd`](../lua-api/navigation2d.md#crowd) moves many agents and keeps them from overlapping. It uses optimal reciprocal collision avoidance, ORCA. Each step, every agent looks at its nearest neighbors and picks the velocity closest to the one it wants among those that will not hit a neighbor soon. Both agents of a pair share the effort, so they slide past each other without the jitter of agents that each dodge the whole way. Agents also keep clear of static obstacles, polygons or walls that `crowd:addObstacle` adds.

The crowd only avoids. Where an agent wants to go comes from the app, in one of two ways:

- `crowd:setTarget` gives a point. The agent heads for it at full speed and stops on it.
- `crowd:setPreferredVelocity` gives a velocity, such as the direction of a flow field times a speed, every frame.

A crowd steps in C++ and spreads its work over the job system, so thousands of agents cost far less than steering them from Lua. Its steps are deterministic, so a replay moves every agent the same way.

Agents that all want the same spot, or that meet head on in a perfect circle, can stop in a standoff where each one politely waits for the others. ORCA allows it, because nobody collides. Give the targets a small random spread, or a little `separation`, and the standoff breaks.

### Recipe: a crowd that follows a path

A squad of soldiers walks to a point that the player clicks. The grid plans one path for the whole squad. Each soldier walks the points of the path one by one, and the crowd keeps the soldiers apart on the way. A small spread of each target keeps them from piling onto one spot.

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local navigation2d = require('haylen.navigation2d')
local scene = require('haylen.scene')

local tile = 32
local grid = navigation2d.newGrid(50, 30)
for row = 0, 20 do
    grid:setWalkable(25, row, false)
end

local camera = graphics2d.newCamera()
local crowd = navigation2d.newCrowd({separation = 0.2})
local squad = {}
for index = 1, 24 do
    local id = crowd:addAgent({x = 64 + (index % 6) * 20, y = 64 + (index // 6) * 20, radius = 8, maxSpeed = 100})
    squad[index] = {id = id, next = 1}
end

local function center(cell, index)
    -- Each soldier aims a little off the center of the cell, so they never all want one spot.
    local angle = index * 2.4
    return cell.x * tile + tile / 2 + math.cos(angle) * 6, cell.y * tile + tile / 2 + math.sin(angle) * 6
end

local route = nil

scene.push({
    update = function(self, dt)
        if input.mousePressed() then
            local x, y = camera:screenToWorld(input.mousePosition())
            local first = crowd:position(squad[1].id)
            route = grid:findPath(first.x // tile, first.y // tile, x // tile, y // tile, {smooth = true})
            for _, soldier in ipairs(squad) do
                soldier.next = 1
            end
        end

        if route then
            for index, soldier in ipairs(squad) do
                local cell = route[soldier.next]
                if cell then
                    local x, y = center(cell, index)
                    crowd:setTarget(soldier.id, x, y)
                    local position = crowd:position(soldier.id)
                    local close = (position.x - x) ^ 2 + (position.y - y) ^ 2 < 24 * 24
                    if close and soldier.next < #route then
                        soldier.next = soldier.next + 1
                    end
                end
            end
        end

        crowd:step(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        for _, soldier in ipairs(squad) do
            local position = crowd:position(soldier.id)
            graphics2d.drawCircle(position.x, position.y, 8, '#FF60C0FF')
        end
    end,
})
```

When the units of a crowd start in many places and head to one goal, a flow field fits better than one path. Each frame, set the preferred velocity of each agent to the direction of the field at its cell times its speed, and step the crowd.

```lua
local navigation2d = require('haylen.navigation2d')
local scene = require('haylen.scene')

local tile = 32
local grid = navigation2d.newGrid(40, 30)
local field = grid:flowField({{38, 15}})
local crowd = navigation2d.newCrowd()
local zombies = {}
for index = 1, 500 do
    zombies[index] = crowd:addAgent({x = 20 + index % 25 * 24, y = 20 + index // 25 * 24, radius = 6, maxSpeed = 40})
end

scene.push({
    update = function(self, dt)
        for _, id in ipairs(zombies) do
            local position = crowd:position(id)
            local direction = field:direction(position.x // tile, position.y // tile)
            crowd:setPreferredVelocity(id, direction.x * 40, direction.y * 40)
        end
        crowd:step(dt)
    end,
})
```

## Off the frame

A long search on a large map can take longer than a frame. Every expensive operation has a version whose name ends in `Async`: path searches, Dijkstra maps, flow fields, hierarchical path finders and the build of a navigation mesh. Each copies what it needs, runs on a worker thread and returns a promise. A wrong argument still raises at once, in the call, so mistakes show up where they happen.

A grid copies its cells once for background work and shares the copy between calls until a cell changes. So many units that ask for paths in the same frame copy the grid once.

Start the work with [`scene.spawn`](../lua-api/scene.md#scenespawnowner-fn), and the task ends with the scene that owns it. A path that arrives after the scene left never touches a unit that is gone.

```lua
local navigation2d = require('haylen.navigation2d')
local scene = require('haylen.scene')

local level = {}

function level:enter()
    self.grid = navigation2d.newGrid(1024, 1024)
    self.unit = {x = 0, y = 0}
    scene.spawn(self, function()
        local path = self.grid:findPathAsync(0, 0, 1000, 900, {jumpPoint = true}):await()
        self.unit.path = path
        print(path and #path or 'No path.')
    end)
end

scene.push(level)
```

The search works on a copy, so cells that change after the call do not affect it. A path can be stale when it arrives, and a unit that meets a new wall plans again.

## Costs

Every algorithm runs in C++, so one call costs far less than the same work in Lua. What matters is how the cost grows:

| Method | Grows with | Advice |
| --- | --- | --- |
| `grid:findPath` | The cells it visits, which `grid.expandedCount` reports. The search buffers stay with the grid, so repeated searches allocate nothing. | Use jump point search on open grids with even costs, and a `weight` when a slightly longer path is fine. |
| Hierarchical path finder | Building: the whole grid. Each search: the entrances on the way plus the cells of the chosen clusters. `router:update`: the changed clusters and their neighbors. | Build it once, with the asynchronous version for large maps, and update it after changes. |
| `grid:flowField` and `grid:dijkstraMap` | Every reachable cell of the grid, once per computation. Reading a cell costs one call. | Compute again only when the goals or the walls change. |
| `NavGraph` searches | The points and connections it visits. `graph:closestPoint` checks every point. | Keep graphs to the places that matter. |
| `NavMesh` | Building: the polygons and their corners, far more than a query. Queries: the triangles along the way. | Build in the background, and move obstacles rarely. |
| `SteeringAgent` | One Lua call per behavior per agent per frame. | Steer tens or hundreds of agents, and use a crowd for more. |
| `Crowd` | The agents times their `maxNeighbors`, spread over the job system. | Lower `neighborDistance` and `maxNeighbors` for dense crowds. |

The command `haylen bench --suite algorithms` times A\*, weighted A\*, jump point search, flow fields, hierarchical path finding, navigation mesh builds and paths, and crowd steps on the machine, as [the bench command](../tools/build.md#bench) describes.

## C++

The C++ types live in `haylen/2d/navigation/` in the namespace `haylen::navigation2d`: `Grid` with `GridSearch`, `DijkstraMap`, `FlowField`, `HierarchicalPathfinder`, `Graph` with `GraphSearch`, `NavMesh`, `SteeringAgent`, `Wanderer` and `Crowd`.
