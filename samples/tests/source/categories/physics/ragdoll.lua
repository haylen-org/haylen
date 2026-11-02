-- Ragdolls from `physics2d.newRagdoll`: eleven capsules with the joint limits of a body, tumbling down stone stairs, dragged by any limb over the steps with a grabber and thrown around.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Figure = require('categories.physics.figure')
local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Ragdoll = haylen.class('Ragdoll', PhysicsTest)

Ragdoll.code = [[
local doll = physics2d.newRagdoll(world, {x = -740, y = -280, height = 280, stiffness = 0.2, group = -1, vx = 260})
local other = physics2d.newRagdoll(world, {x = -100, y = -250, height = 280, group = -2})  -- Collides with the first.
local chest = doll:body('chest')
chest:applyImpulse(chest.mass * 500, -chest.mass * 600)  -- Throws the figure.
for part, body in pairs(doll:bodies()) do drawLimb(part, body) end]]

Ragdoll.maxDolls = 6
Ragdoll.height = 280
-- The courtyard ends in a stone wall on the right, this many rows of blocks tall, which stops the ragdolls that tumble away.
Ragdoll.wall = 660
Ragdoll.wallRows = 4
Ragdoll.steps, Ragdoll.stepWidth, Ragdoll.stepHeight = 6, 100, 72
Ragdoll.stairLeft = -800
Ragdoll.ground = 330

function Ragdoll:enter()
    self:frame{
        code = Ragdoll.code,
        hint = 'Drag a limb to pull a ragdoll up or down the stairs or to throw it. R or the X button starts over.',
        controls = {
            ui.button{id = 'drop', text = 'Drop a ragdoll', onClick = function() self:drop() end},
            ui.button{id = 'push', text = 'Push them all', onClick = function() self:push() end},
            ui.label{text = 'Stiffness of new ragdolls', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'stiffness', value = 0.2, min = 0, max = 1, showValue = true, decimals = 2, onChange = function(event) self.stiffness = event.value end},
            ui.checkbox{id = 'outlines', text = 'Show the capsules', onChange = function(event) self.outlines = event.checked end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        focus = 'drop',
    }
    self.random = m.random(21)
    self.stiffness = 0.2
    self:build()
end

function Ragdoll:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.dolls, self.groups = {}, 0

    local ground = self.world:createBody({type = 'static', x = 0, y = Ragdoll.ground + 30})
    ground:addBox(8000, 60)
    -- Solid steps reach down to the ground, so no limb wedges under a step above it.
    local stairs = self.world:createBody({type = 'static'})
    for step = 0, Ragdoll.steps - 1 do
        local top = Ragdoll.ground - (Ragdoll.steps - step) * Ragdoll.stepHeight
        local left = step == 0 and Ragdoll.stairLeft - 900 or Ragdoll.stairLeft + step * Ragdoll.stepWidth
        local right = Ragdoll.stairLeft + (step + 1) * Ragdoll.stepWidth
        stairs:addBox(right - left, Ragdoll.ground - top, {offsetX = (left + right) / 2, offsetY = (top + Ragdoll.ground) / 2, friction = 0.2})
    end

    local wallHeight = Ragdoll.wallRows * Ragdoll.stepHeight
    local wall = self.world:createBody({type = 'static', x = Ragdoll.wall + 450, y = Ragdoll.ground - wallHeight / 2})
    wall:addBox(900, wallHeight)

    self:drop(Ragdoll.stairLeft + 60, Ragdoll.ground - Ragdoll.steps * Ragdoll.stepHeight - 180)
    self:drop(-60, -250)
    self:drop(300, -120)
end

-- Every ragdoll gets its own negative group, so its parts ignore each other but collide with the other ragdolls.
function Ragdoll:drop(x, y)
    self.groups = self.groups + 1
    local doll = physics2d.newRagdoll(self.world, {
        x = x or self.random:range(Ragdoll.stairLeft + 40, Ragdoll.stairLeft + Ragdoll.stepWidth * 3),
        y = y or -330,
        height = Ragdoll.height,
        stiffness = self.stiffness,
        group = -self.groups,
        vx = self.random:range(200, 320),
    })
    self.dolls[#self.dolls + 1] = {doll = doll, outfit = Figure.dress(doll, self.groups)}
    if #self.dolls > Ragdoll.maxDolls then
        table.remove(self.dolls, 1).doll:destroy()
    end
end

function Ragdoll:push()
    for _, entry in ipairs(self.dolls) do
        local chest = entry.doll:body('chest')
        chest:applyImpulse(chest.mass * self.random:range(300, 700), -chest.mass * 600)
    end
end

function Ragdoll:update(dt)
    Ragdoll.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    self:status(string.format('%d ragdolls of eleven capsules each, %d bodies in all.', #self.dolls, self.world.bodyCount - 3))
end

function Ragdoll:fixedUpdate(step)
    self:simulate(self.world, step)
end

-- Lays a row of stone blocks from `from` toward `to`, in either direction, whose joints alternate with the rows next to it: odd rows start with a block half as long again.
function Ragdoll:drawRow(row, from, to)
    local width, height = Ragdoll.stepWidth, Ragdoll.stepHeight
    local direction = to > from and 1 or -1
    local y = Ragdoll.ground - (row + 0.5) * height
    local x = from
    if row % 2 == 1 then
        Look.block(x + direction * width * 0.75, y, width * 1.5, height, 'stone', -8)
        x = x + direction * width * 1.5
    end
    while (to - x) * direction > 0 do
        Look.block(x + direction * width / 2, y, width, height, 'stone', -8)
        x = x + direction * width
    end
end

-- The stairs and the wall are laid of stone blocks, each row of the stairs reaching out to the edge of its step.
function Ragdoll:drawStones()
    local bounds = graphics2d.canvasBounds()
    for row = 0, Ragdoll.steps - 1 do
        self:drawRow(row, Ragdoll.stairLeft + (Ragdoll.steps - row) * Ragdoll.stepWidth, bounds.x)
    end
    for row = 0, Ragdoll.wallRows - 1 do
        self:drawRow(row, Ragdoll.wall, bounds.x + bounds.width)
    end
end

function Ragdoll:draw(area)
    Scenery.sky(self.camera, Ragdoll.ground)
    Scenery.ground(Ragdoll.ground)
    self:drawStones()
    for index, entry in ipairs(self.dolls) do
        Figure.draw(entry.doll, entry.outfit, index)
    end
    if self.outlines then
        self.world:debugDraw({layer = 30})
    end
    self.grab:draw()
end

return Ragdoll
