-- Node tree: tanks instantiated from one prefab drive along the roads of a meadow longer than the view, their turrets aim at the pointer and their road wheels turn inside their tracks, while the camera pans so the tree culls what it does not show. Every tank keeps a slot of its lane, and the tanks of a lane drive at one speed and come back at the other end, so they never meet. Tanks are added and destroyed, the turrets hidden with their barrels and pennants, and a tank saved to a description and loaded back as a copy.
local assets = require('haylen.assets')
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local nodes2d = require('haylen.nodes2d')
local ui = require('haylen.ui')

local Pointer = require('harness.pointer')
local Test = require('harness.test')

local NodeTree = haylen.class('NodeTree', Test)

NodeTree.road = 6000
-- The tanks of a lane come back this far past the ends of the road, where the camera never looks.
NodeTree.margin = 240
NodeTree.cell = 60
NodeTree.lanes = {{row = 3, direction = 1, speed = 70}, {row = 6, direction = -1, speed = 55}, {row = 9, direction = 1, speed = 85}}
NodeTree.world = 600
NodeTree.slots = 32
NodeTree.starting = 24
-- The hull stands this far above the middle of its road, so its track runs on the road.
NodeTree.lift = 18
-- The tanks, wheels, barrels and pennants move in one call, eight nodes of three values for each tank.
NodeTree.movingPaths = {'', 'wheel1', 'wheel2', 'wheel3', 'wheel4', 'wheel5', 'turret/barrel', 'turret/flag'}
NodeTree.grass = {2, 2, 128, 128}
NodeTree.flowers = {134, 2, 128, 128}
-- The tiles of the roads, from the corner set of the paths of the Tiled tests: the row of cells above the line of a road holds its top edge and the row below it its bottom edge.
NodeTree.roadTop = {2 + 12 * 132, 2 + 5 * 132, 128, 128}
NodeTree.roadBottom = {2 + 3 * 132, 2 + 5 * 132, 128, 128}

NodeTree.code = [[
local tank = tree:instantiate(prefab, {name = 'tank1', x = x, y = y})  -- The subtree of tank.prefab.
tree:writeTransforms(moving, transforms)  -- Tanks, wheels, barrels and pennants in one call.
tree:update(dt)  -- One pass over every changed node.
tree:draw()  -- One call that culls and batches every node.
local description = nodes2d.save(tank)
local copy = tree:load(description)]]

assets.defineGroups({groups = {nodes = {'nodes/', {path = 'tiled/tiles/terrain.png', options = {filter = 'linear'}}, {path = 'tiled/tiles/transitions.png', options = {filter = 'linear'}}}}})

-- Spreads the slots of a lane in the order that keeps the tanks farthest apart: halves, then quarters, then eighths.
function NodeTree.slotOrder()
    local order = {}
    local bits = math.floor(math.log(NodeTree.slots, 2) + 0.5)
    for index = 0, NodeTree.slots - 1 do
        local reversed = 0
        for bit = 0, bits - 1 do
            reversed = reversed | ((index >> bit) & 1) << (bits - 1 - bit)
        end
        order[#order + 1] = reversed
    end
    return order
end

-- Counts the nodes below a description.
function NodeTree.countParts(description)
    local count = 0
    for _, child in ipairs(description.children or {}) do
        count = count + 1 + NodeTree.countParts(child)
    end
    return count
end

function NodeTree:load(context)
    context:preload('nodes'):await()
end

function NodeTree:unload()
    assets.unloadGroup('nodes')
end

function NodeTree:enter()
    self.tree = nodes2d.newTree()
    self.prefab = assets.load('nodes/tank.prefab')
    self.wheelRadius = self.prefab.description.children[1].sprite.width / 2
    self.loop = NodeTree.road + NodeTree.margin * 2
    self.order = NodeTree.slotOrder()
    self.phases = {0, 0, 0}
    self.taken = {{}, {}, {}}
    self.counts = {0, 0, 0}
    self.tanks = {}
    self.values = {}
    self.time = 0
    self.created = 0
    self.copies = 0
    self.destroyedCount = 0
    self.pointer = Pointer()
    self:buildGround()
    for _ = 1, NodeTree.starting do
        self:addTank()
    end
    self:loadActions({actions = Pointer.actions})
    self:frame{
        code = NodeTree.code,
        play = true,
        hint = 'Move the pointer or the right stick to aim the turrets. The camera pans along the roads, and the tanks it leaves behind are culled.',
        controls = {
            ui.button{id = 'add', text = 'Add a tank', onClick = function() self:addTank() end},
            ui.button{id = 'destroy', text = 'Destroy a tank', onClick = function() self:destroyTank() end},
            ui.toggle{id = 'turrets', text = 'Show the turrets', checked = true, onChange = function(event) self:showTurrets(event.checked) end},
            ui.button{id = 'save', text = 'Save a tank and load a copy', onClick = function() self:copyTank() end},
            ui.label{id = 'saved', text = 'Nothing saved yet.', color = 'textMuted'},
        },
    }
end

-- The meadow is a layer of grass with a few flowers, and the roads are a second layer of the corner tiles of a path, so their edges wander into the grass.
function NodeTree:buildGround()
    local cell = NodeTree.cell
    self.grass = graphics2d.newTileLayer(assets.texture('tiled/tiles/terrain.png', {filter = 'linear'}), {cellWidth = cell, cellHeight = cell})
    self.grass:defineFrame(1, {source = NodeTree.grass, width = cell, height = cell})
    self.grass:defineFrame(2, {source = NodeTree.flowers, width = cell, height = cell})
    self.roads = graphics2d.newTileLayer(assets.texture('tiled/tiles/transitions.png', {filter = 'linear'}), {cellWidth = cell, cellHeight = cell})
    self.roads:defineFrame(1, {source = NodeTree.roadTop, width = cell, height = cell})
    self.roads:defineFrame(2, {source = NodeTree.roadBottom, width = cell, height = cell})

    local first, last = math.floor(-NodeTree.margin / cell) - 2, math.ceil((NodeTree.road + NodeTree.margin) / cell) + 2
    local random = m.random(7)
    for row = -40, 50 do
        for column = first, last do
            self.grass:setCell(column, row, random:chance(0.06) and 2 or 1)
        end
    end
    for _, lane in ipairs(NodeTree.lanes) do
        for column = first, last do
            self.roads:setCell(column, lane.row - 1, 1)
            self.roads:setCell(column, lane.row, 2)
        end
    end
end

-- Puts a tank, or a description loaded as a copy, in the free slot that keeps it farthest from the tanks of the lane with the fewest tanks.
function NodeTree:place(create)
    local lane = 1
    for index = 2, #NodeTree.lanes do
        if self.counts[index] < self.counts[lane] then
            lane = index
        end
    end
    local slot
    for _, candidate in ipairs(self.order) do
        if not self.taken[lane][candidate] then
            slot = candidate
            break
        end
    end
    if not slot then
        return nil
    end

    local rule = NodeTree.lanes[lane]
    local tank = create(rule.row * NodeTree.cell - NodeTree.lift)
    tank.data.lane, tank.data.slot = lane, slot
    tank.scaleX = rule.direction
    tank:find('tag').scaleX = rule.direction
    self.taken[lane][slot] = tank
    self.counts[lane] = self.counts[lane] + 1
    self.tanks[#self.tanks + 1] = tank
    tank:on('destroyed', function(node)
        self.destroyedCount = self.destroyedCount + 1
        self:log('Destroyed %s.', node.name)
    end, {owner = self})
    self:collectMoving()
    return tank
end

function NodeTree:addTank()
    self.created = self.created + 1
    local number = self.created
    local tank = self:place(function(y) return self.tree:instantiate(self.prefab, {name = 'tank' .. number, y = y}) end)
    if not tank then
        self.created = self.created - 1
        self:set('saved', {text = 'Every slot of the roads holds a tank.'})
        return
    end
    tank:find('tag/label').text = 'Tank ' .. number
end

function NodeTree:destroyTank()
    local tank = table.remove(self.tanks)
    if not tank then
        return
    end
    self.taken[tank.data.lane][tank.data.slot] = nil
    self.counts[tank.data.lane] = self.counts[tank.data.lane] - 1
    tank:destroy()
    self:collectMoving()
end

function NodeTree:showTurrets(shown)
    for _, tank in ipairs(self.tanks) do
        tank:find('turret').visible = shown
    end
end

-- Saves the first tank to a description and loads it back as a copy in a free slot, with its parts, its look and a label of its own.
function NodeTree:copyTank()
    local source = self.tanks[1]
    if not source then
        return
    end
    local description = nodes2d.save(source)
    self.copies = self.copies + 1
    description.name = 'copy' .. self.copies
    local copy = self:place(function(y)
        description.y = y
        return self.tree:load(description)
    end)
    if not copy then
        self.copies = self.copies - 1
        self:set('saved', {text = 'Every slot of the roads holds a tank.'})
        return
    end
    copy:find('tag/label').text = 'Copy ' .. self.copies
    copy:find('turret').visible = source:find('turret').visible
    self:set('saved', {text = string.format('Saved %s with its %d parts into a description and loaded it back as copy %d.', source:find('tag/label').text:lower(), NodeTree.countParts(description), self.copies)})
end

-- Lists the nodes that move every frame, in the order of the values that move them.
function NodeTree:collectMoving()
    self.moving = {}
    self.parts = {}
    for _, tank in ipairs(self.tanks) do
        for _, path in ipairs(NodeTree.movingPaths) do
            local node = path == '' and tank or tank:find(path)
            local mount = node.parent
            self.moving[#self.moving + 1] = node
            self.parts[#self.parts + 1] = {node.x, node.y, mount and mount.x or 0, mount and mount.y or 0}
        end
    end
    self.transforms = collections.newFloatBuffer(math.max(1, #self.moving * 3))
    for index = #self.moving * 3 + 1, #self.values do
        self.values[index] = nil
    end
end

function NodeTree:update(dt)
    NodeTree.super.update(self, dt)
    if not self.area then
        return
    end
    self.time = self.time + dt

    -- The world fits the height of the play area, and the camera pans along the road so tanks leave and enter the view.
    local zoom = math.min(1.25, self.area.height / NodeTree.world)
    local viewWidth, viewHeight = self.area.width / zoom, self.area.height / zoom
    self.camera.zoom = {zoom, zoom}
    self.camera.position = {(math.sin(self.time * 0.12) * 0.5 + 0.5) * (NodeTree.road - viewWidth), (NodeTree.world - viewHeight) / 2}
    self.pointer:update(dt, self)

    -- The turrets aim at the pointer while it is over the play area, and sweep the sky while it is not.
    local aimX, aimY = self.pointer.worldX, self.pointer.worldY
    if not self.stage:contains({self.pointer.x, self.pointer.y}) then
        aimX, aimY = self.camera.position.x + viewWidth * (0.5 + math.sin(self.time * 0.7) * 0.4), -200
    end

    for lane, rule in ipairs(NodeTree.lanes) do
        self.phases[lane] = (self.phases[lane] + rule.speed * rule.direction * dt) % self.loop
    end
    local values, spacing = self.values, self.loop / NodeTree.slots
    for index, tank in ipairs(self.tanks) do
        local rule = NodeTree.lanes[tank.data.lane]
        local x = (self.phases[tank.data.lane] + tank.data.slot * spacing) % self.loop - NodeTree.margin
        local y = rule.row * NodeTree.cell - NodeTree.lift
        local base = (index - 1) * 8
        local spin = x / self.wheelRadius * rule.direction
        values[base * 3 + 1], values[base * 3 + 2], values[base * 3 + 3] = x, y, 0
        for wheel = 1, 5 do
            local part = self.parts[base + 1 + wheel]
            values[(base + wheel) * 3 + 1], values[(base + wheel) * 3 + 2], values[(base + wheel) * 3 + 3] = part[1], part[2], spin
        end

        -- The barrel turns toward the pointer in the space of its turret, which mirrors with the tank, within the reach of its mount.
        local barrel, flag = self.parts[base + 7], self.parts[base + 8]
        local localX = (aimX - x) * rule.direction - barrel[3] - barrel[1]
        local localY = aimY - y - barrel[4] - barrel[2]
        values[(base + 6) * 3 + 1], values[(base + 6) * 3 + 2], values[(base + 6) * 3 + 3] = barrel[1], barrel[2], m.clamp(math.atan(localY, localX), -1.2, 0.4)
        values[(base + 7) * 3 + 1], values[(base + 7) * 3 + 2], values[(base + 7) * 3 + 3] = flag[1], flag[2], math.sin(self.time * 6 + x * 0.01) * 0.15
    end
    if #self.moving > 0 then
        self.transforms:set(1, values)
        self.tree:writeTransforms(self.moving, self.transforms)
    end
    self.tree:update(dt)

    local line = string.format('%d nodes: %d drawn and %d culled outside the view, and the whole frame takes %d draw calls.', self.tree.size, self.tree.drawnCount, self.tree.culledCount, graphics2d.stats().drawCalls)
    if self.destroyedCount > 0 then
        line = line .. string.format(' Tanks destroyed: %d.', self.destroyedCount)
    end
    self:status(line)
end

function NodeTree:draw(area)
    self.grass:draw({layer = -20})
    self.roads:draw({layer = -19})
    self.tree:draw()
end

function NodeTree:renderUi()
    self.pointer:draw()
end

return NodeTree
