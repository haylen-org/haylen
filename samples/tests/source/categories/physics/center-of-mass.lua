-- Tall blocks tipped over in two worlds. On the left their center of mass sits in their middle, where their shapes put it, so they fall over, and on the right `body.centerOfMass` moves it low, like a weighted toy, so the same blocks rock back upright. The mass and the inertia stay what the shapes give unless they are set too.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local CenterOfMass = haylen.class('CenterOfMass', PhysicsTest)

local kWidth, kHeight = 70, 150
local kTilts = {0.5, 0.8, 1.1}

function CenterOfMass:enter()
    self:frame{
        hint = 'Tip the blocks with the button or drag them. A dot marks the center of mass of each block. R or the X button stands them up again.',
        controls = {
            ui.button{id = 'tip', text = 'Tip them over', onClick = function() self:tip() end},
            ui.label{text = 'Center of mass on the right, below the middle', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'center', value = 60, min = 0, max = 74, step = 2, showValue = true, decimals = 0, onChange = function(event) self:setCenter(event.value) end},
            ui.button{id = 'reset', text = 'Stand them up', onClick = function() self:build() end},
        },
        focus = 'tip',
    }
    self.center = 60
    self:build()
end

function CenterOfMass:build()
    self.sides = {}
    for index, weighted in ipairs({false, true}) do
        local origin = index == 1 and -400 or 400
        local world = physics2d.newWorld()
        local ground = world:createBody({type = 'static', x = origin, y = 400})
        parts.box(ground, 780, 40)
        local blocks = {}
        for slot, tilt in ipairs(kTilts) do
            local block = world:createBody({x = origin + (slot - 2) * 230, y = 380 - kHeight / 2 - 20, rotation = tilt})
            parts.box(block, kWidth, kHeight, {friction = 0.8})
            parts.paint(block, weighted and '#FF6FDCA0' or '#FFE57373')
            if weighted then
                block.centerOfMass = {0, self.center}
            end
            blocks[slot] = block
        end
        self.sides[index] = {world = world, origin = origin, ground = ground, blocks = blocks, grab = Grab(world), weighted = weighted}
    end
end

function CenterOfMass:setCenter(value)
    self.center = value
    for _, block in ipairs(self.sides[2].blocks) do
        block.centerOfMass = {0, value}
        block.awake = true
    end
end

function CenterOfMass:tip()
    for _, side in ipairs(self.sides) do
        for _, block in ipairs(side.blocks) do
            block:applyAngularImpulse(block.inertia * 2.5)
        end
    end
end

function CenterOfMass:update(dt)
    CenterOfMass.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    for _, side in ipairs(self.sides) do
        side.grab:update(self.pointer, self.camera)
    end
    local plain, weighted = self.sides[1].blocks[2], self.sides[2].blocks[2]
    self:status(string.format('Middle blocks: tilt %.0f and %.0f degrees, mass %.2f and %.2f kg, inertia %.0f and %.0f, center %.0f and %.0f below the middle, step %.2f ms', math.deg(plain.rotation), math.deg(weighted.rotation), plain.mass, weighted.mass, plain.inertia, weighted.inertia, plain.centerOfMass.y, weighted.centerOfMass.y, self:stepTime()))
end

function CenterOfMass:fixedUpdate(step)
    for _, side in ipairs(self.sides) do
        self:simulate(side.world, step)
    end
end

function CenterOfMass:draw(area)
    graphics2d.drawLine(0, -430, 0, 430, 2, '#44FFFFFF')
    local titles = {'Center of mass in the middle', 'Center of mass moved low'}
    for index, side in ipairs(self.sides) do
        graphics2d.drawText(nil, titles[index], side.origin, -390, {size = 28, color = index == 1 and '#FFFF8A84' or '#FF6FDCA0', anchor = {0.5, 0.5}})
        parts.draw(side.ground)
        for _, block in ipairs(side.blocks) do
            parts.draw(block, {layer = 1})
            local center = block.worldCenter
            graphics2d.drawCircle(center.x, center.y, 9, '#FFFFFFFF', {layer = 2})
            graphics2d.drawCircle(center.x, center.y, 5, '#FF263238', {layer = 2})
        end
        side.grab:draw()
    end
end

return CenterOfMass
