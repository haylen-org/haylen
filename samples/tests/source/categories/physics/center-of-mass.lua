-- Tall toy blocks that start tipped over in two worlds. On the left their center of mass sits in their middle, where their shapes put it, so they fall over, and on the right `body.centerOfMass` moves it low, like a toy with a weight in its foot, so the same blocks rock back upright. The mass and the inertia stay what the shapes give unless they are set too.
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local CenterOfMass = haylen.class('CenterOfMass', PhysicsTest)

CenterOfMass.code = [[
block:addBox(84, 190, {friction = 0.8})
block.centerOfMass = {0, 76}  -- Low in the body, so the block rocks back upright.
block:applyAngularImpulse(block.inertia * 2.5)  -- Tips it over.
local center = block.worldCenter  -- The dot drawn on each block.]]

CenterOfMass.width, CenterOfMass.height = 84, 190
CenterOfMass.tilts = {0.5, 0.8, 1.1}
CenterOfMass.ground = 360
CenterOfMass.colors = {'blue', 'yellow', 'purple'}

function CenterOfMass:enter()
    self:frame{
        code = CenterOfMass.code,
        hint = 'Tip the blocks with the button or drag them. A dot with an arrow marks the center of mass of each block. R or the X button stands them up again.',
        controls = {
            ui.button{id = 'tip', text = 'Tip them over', onClick = function() self:tip() end},
            ui.label{text = 'How low the weight sits on the right', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'center', value = 76, min = 0, max = 94, step = 2, showValue = true, decimals = 0, onChange = function(event) self:setCenter(event.value) end},
            ui.button{id = 'reset', text = 'Stand them up', onClick = function() self:build() end},
        },
        focus = 'tip',
        view = {1580, 640},
    }
    self.camera.position = {0, 80}
    self.center = 76
    self:build()
end

function CenterOfMass:build()
    self.halves = {}
    for index, weighted in ipairs({false, true}) do
        local origin = index == 1 and -400 or 400
        local world = physics2d.newWorld()
        local ground = world:createBody({type = 'static', x = origin, y = CenterOfMass.ground + 30})
        ground:addBox(800, 60)
        local blocks = {}
        for slot, tilt in ipairs(CenterOfMass.tilts) do
            local block = world:createBody({x = origin + (slot - 2) * 240, y = CenterOfMass.ground - CenterOfMass.height / 2 - 30, rotation = tilt})
            block:addBox(CenterOfMass.width, CenterOfMass.height, {friction = 0.8})
            Look.dress(block, CenterOfMass.colors[slot], {radius = 14})
            if weighted then
                block.centerOfMass = {0, self.center}
            end
            blocks[slot] = block
        end
        self.halves[index] = {world = world, origin = origin, blocks = blocks, grab = Grab(world), weighted = weighted}
    end
end

function CenterOfMass:setCenter(value)
    self.center = value
    for _, block in ipairs(self.halves[2].blocks) do
        block.centerOfMass = {0, value}
        block.awake = true
    end
end

function CenterOfMass:tip()
    for _, half in ipairs(self.halves) do
        for _, block in ipairs(half.blocks) do
            block:applyAngularImpulse(block.inertia * 2.5)
        end
    end
end

function CenterOfMass:update(dt)
    CenterOfMass.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    for _, half in ipairs(self.halves) do
        half.grab:update(self.pointer, self.camera)
    end
    local standing = {0, 0}
    for index, half in ipairs(self.halves) do
        for _, block in ipairs(half.blocks) do
            local tilt = math.abs(PhysicsTest.degrees(block.rotation)) % 180
            standing[index] = standing[index] + ((tilt < 20 or tilt > 160) and 1 or 0)
        end
    end
    self:status(string.format('%d of 3 blocks stand on the left, where the center of mass is in the middle, and %d of 3 on the right, where it sits %d units below it.', standing[1], standing[2], self.center))
end

function CenterOfMass:fixedUpdate(step)
    for _, half in ipairs(self.halves) do
        self:simulate(half.world, step)
    end
end

-- A weighted block shows its weight as an iron foot, and every block shows its center of mass as a dot with gravity pulling down on it.
function CenterOfMass:drawBlock(block, weighted)
    Look.draw(block, {layer = 1})
    local cos, sin = math.cos(block.rotation), math.sin(block.rotation)
    if weighted then
        local foot = CenterOfMass.height / 2 - 26
        Look.block(block.x - sin * foot, block.y + cos * foot, CenterOfMass.width - 16, 36, 'iron', 2, block.rotation)
    end
    local unit = Look.pixel
    local center = block.worldCenter
    Look.line(center.x, center.y, center.x, center.y + 46 * unit, 4 * unit, '#FFFFFFFF', 3)
    Look.line(center.x, center.y + 46 * unit, center.x - 9 * unit, center.y + 34 * unit, 4 * unit, '#FFFFFFFF', 3)
    Look.line(center.x, center.y + 46 * unit, center.x + 9 * unit, center.y + 34 * unit, 4 * unit, '#FFFFFFFF', 3)
    Look.pin(center.x, center.y, 9 * unit, 4, '#FFFFFFFF')
end

function CenterOfMass:draw(area)
    Scenery.sky(self.camera, CenterOfMass.ground)
    Scenery.ground(CenterOfMass.ground)
    self:compare('Center of mass in the middle', 'Center of mass moved low', 0, -200)
    for _, half in ipairs(self.halves) do
        for _, block in ipairs(half.blocks) do
            self:drawBlock(block, half.weighted)
        end
        half.grab:draw()
    end
end

return CenterOfMass
