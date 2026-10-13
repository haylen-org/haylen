-- Node tree: tanks instantiated from one prefab drive back and forth, their turrets aim at the pointer through their own space and their wheels turn, while the camera pans over a road longer than the view so the tree culls what it does not show. Tanks are added and destroyed, the turrets hidden with their barrels and flags, and a tank saved to a description and loaded back as a copy.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local nodes2d = require('haylen.nodes2d')
local ui = require('haylen.ui')

local Test = require('harness.test')

local NodeTree = haylen.class('NodeTree', Test)

NodeTree.road = 6000
NodeTree.lanes = 4
NodeTree.laneHeight = 150
NodeTree.wheelRadius = 12

NodeTree.code = [[
local tank = tree:instantiate(prefab, {x = x, y = y})  -- The subtree of tank.prefab.
tank:find('turret').rotation = math.atan(dy, dx)  -- Children follow their parent.
tree:update(dt)  -- One pass over every changed node.
tree:draw()  -- One call that culls and batches every node.]]

assets.defineGroups({groups = {nodes = {{path = 'nodes/'}}}})

function NodeTree:load(context)
    context:preload('nodes'):await()
end

function NodeTree:unload()
    assets.unloadGroup('nodes')
end

function NodeTree:enter()
    self.tree = nodes2d.newTree()
    self.prefab = assets.load('nodes/tank.prefab')
    self.tanks = {}
    self.time = 0
    self.destroyedCount = 0
    for index = 1, 24 do
        self:addTank(index)
    end
    self:frame{
        code = NodeTree.code,
        hint = 'Move the pointer to aim the turrets. The camera pans along the road, and the status line counts the nodes drawn and culled.',
        controls = {
            ui.button{id = 'add', text = 'Add a tank', onClick = function() self:addTank(#self.tanks + 1) end},
            ui.button{id = 'destroy', text = 'Destroy a tank', onClick = function() self:destroyTank() end},
            ui.toggle{id = 'turrets', text = 'Show the turrets', checked = true, onChange = function(event) self:showTurrets(event.checked) end},
            ui.button{id = 'save', text = 'Save a tank and load a copy', onClick = function() self:copyTank() end},
            ui.label{id = 'saved', text = 'Nothing saved yet.', color = 'textMuted'},
        },
        focus = 'add',
    }
end

-- Places a tank on a lane of the road, at a spread that leaves most of them out of view.
function NodeTree:addTank(index)
    local lane = (index - 1) % NodeTree.lanes
    local tank = self.tree:instantiate(self.prefab, {name = 'tank' .. index, x = (index * 997) % NodeTree.road, y = 120 + lane * NodeTree.laneHeight})
    tank:find('label').text = 'Tank ' .. index
    tank.data.speed = 60 + (index * 37) % 90
    tank.data.direction = index % 2 == 0 and 1 or -1
    self:face(tank)
    tank:on('destroyed', function(node)
        self.destroyedCount = self.destroyedCount + 1
        self:log('Destroyed %s.', node.name)
    end, {owner = self})
    self.tanks[#self.tanks + 1] = tank
end

-- A tank faces the way it drives by mirroring its subtree, and its label mirrors back so it stays readable.
function NodeTree:face(tank)
    tank.scaleX = tank.data.direction
    tank:find('label').scaleX = tank.data.direction
end

function NodeTree:destroyTank()
    local tank = table.remove(self.tanks)
    if tank then
        tank:destroy()
    end
end

function NodeTree:showTurrets(shown)
    for _, tank in ipairs(self.tanks) do
        tank:find('turret').visible = shown
    end
end

-- Saves the first tank to a description and loads it back as a copy on the next lane, which keeps its label and its parts.
function NodeTree:copyTank()
    local source = self.tanks[1]
    if not source then
        return
    end
    local description = nodes2d.save(source)
    description.name = 'copy' .. (#self.tanks + 1)
    description.y = source.y + NodeTree.laneHeight * 0.5
    local copy = self.tree:load(description)
    copy.data.speed = source.data.speed
    copy.data.direction = source.data.direction
    self.tanks[#self.tanks + 1] = copy
    self:set('saved', {text = string.format('Saved %s with %d children into a description, loaded as %s.', source.name, #description.children, copy.name)})
end

function NodeTree:update(dt)
    NodeTree.super.update(self, dt)
    if not self.area then
        return
    end
    self.time = self.time + dt

    -- The camera pans along the road, so tanks leave and enter the view.
    local span = NodeTree.road - self.area.width
    self.camera.position = {(math.sin(self.time * 0.15) * 0.5 + 0.5) * span, 0}
    local pointerX, pointerY = self:toStage(input.mousePosition())

    for _, tank in ipairs(self.tanks) do
        local step = tank.data.speed * tank.data.direction * dt
        tank.x = tank.x + step
        if tank.x < 0 or tank.x > NodeTree.road then
            tank.data.direction = -tank.data.direction
            self:face(tank)
        end
        local spin = step / NodeTree.wheelRadius * tank.data.direction
        tank:find('rear').rotation = tank:find('rear').rotation + spin
        tank:find('front').rotation = tank:find('front').rotation + spin

        -- The pointer in the space of the turret gives the angle its barrel turns to, whichever way the tank faces.
        local turret = tank:find('turret')
        local barrel = turret:find('barrel')
        local localX, localY = turret:toLocal(pointerX, pointerY)
        barrel.rotation = math.max(-1.2, math.min(0.4, math.atan(localY - barrel.y, localX - barrel.x)))
        turret:find('flag').rotation = math.sin(self.time * 6 + tank.x * 0.01) * 0.15
    end
    self.tree:update(dt)

    local stats = graphics2d.stats()
    self:status(string.format('Nodes %d   Drawn %d   Culled %d   Draw calls %d   Destroyed %d', self.tree.size, self.tree.drawnCount, self.tree.culledCount, stats.drawCalls, self.destroyedCount))
end

function NodeTree:draw(area)
    for lane = 0, NodeTree.lanes - 1 do
        graphics2d.drawRect({-200, 150 + lane * NodeTree.laneHeight, NodeTree.road + 400, 14}, '#FF2A303D', {layer = -10})
    end
    self.tree:draw()
end

return NodeTree
