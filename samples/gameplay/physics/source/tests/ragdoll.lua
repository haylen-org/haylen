-- Ragdolls from `physics2d.newRagdoll`: eleven capsules with the joint limits of a body, tumbling down stairs and thrown around.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('grab')
local parts = require('parts')
local sample = require('sample')

local Ragdoll = haylen.class('Ragdoll', sample.Test)

local kMaxDolls = 8
local kHeight = 240
local kColors = {'#FFFFCC80', '#FF80DEEA', '#FFCE93D8', '#FFA5D6A7'}

-- The bones of the stick figure, each from one part to the next.
local kBones = {
    {'head', 'chest'}, {'chest', 'hips'},
    {'chest', 'upperArmLeft'}, {'upperArmLeft', 'lowerArmLeft'},
    {'chest', 'upperArmRight'}, {'upperArmRight', 'lowerArmRight'},
    {'hips', 'upperLegLeft'}, {'upperLegLeft', 'lowerLegLeft'},
    {'hips', 'upperLegRight'}, {'upperLegRight', 'lowerLegRight'},
}

function Ragdoll:enter()
    Ragdoll.super.enter(self, {
        hint = 'Drag a limb to throw a ragdoll. The figures draw as bones between the centers of their parts, and the outlines show the capsules themselves. R or X starts over.',
        controls = {
            ui.button{id = 'drop', text = 'Drop a ragdoll', onClick = function() self:drop() end},
            ui.button{id = 'push', text = 'Push them all', onClick = function() self:push() end},
            ui.label{text = 'Joint friction of new ragdolls', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'stiffness', value = 20, min = 0, max = 200, showValue = true, decimals = 0, onChange = function(event) self.stiffness = event.value end},
            ui.checkbox{id = 'outlines', text = 'Show the capsules', onChange = function(event) self.outlines = event.checked end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        stats = true,
        focus = 'drop',
    })
    self.random = m.random(21)
    self.stiffness = 20
    self:build()
end

function Ragdoll:build()
    self.world = physics2d.newWorld()
    self.grab = Grab.new(self.world)
    self.dolls, self.groups = {}, 0

    local ground = self.world:createBody({type = 'static', x = 0, y = 410})
    parts.box(ground, 1600, 40)
    self.statics = {ground}
    for step = 0, 5 do
        local stair = self.world:createBody({type = 'static', x = -680 + step * 90, y = -120 + step * 90})
        parts.box(stair, 240 - step * 10, 30)
        self.statics[#self.statics + 1] = stair
    end
    local block = self.world:createBody({type = 'static', x = 450, y = 300})
    parts.box(block, 120, 180)
    self.statics[#self.statics + 1] = block

    self:drop(-680, -200)
    self:drop(-100, -250)
end

-- Every ragdoll gets its own negative group, so its parts ignore each other but collide with the other ragdolls.
function Ragdoll:drop(x, y)
    self.groups = self.groups + 1
    local doll = physics2d.newRagdoll(self.world, {
        x = x or self.random:range(-500, 300),
        y = y or -330,
        height = kHeight,
        jointFriction = self.stiffness,
        group = -self.groups,
        vx = self.random:range(-60, 60),
    })
    self.dolls[#self.dolls + 1] = doll
    if #self.dolls > kMaxDolls then
        table.remove(self.dolls, 1):destroy()
    end
end

function Ragdoll:push()
    for _, doll in ipairs(self.dolls) do
        local chest = doll:body('chest')
        chest:applyImpulse(chest.mass * self.random:range(300, 700), -chest.mass * 600)
    end
end

function Ragdoll:exit()
    Ragdoll.super.exit(self)
    self.world, self.grab, self.dolls, self.statics = nil, nil, nil, nil
end

function Ragdoll:update(dt)
    Ragdoll.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    self:showStats(string.format('Ragdolls %d\nBodies %d\nStep %.2f ms', #self.dolls, self.world.bodyCount, sample.milliseconds('physics step')))
end

function Ragdoll:fixedUpdate(step)
    sample.step(self.world, step)
end

function Ragdoll:drawDoll(doll, color)
    local bodies = doll:bodies()
    local order = {layer = 1}
    local thickness = kHeight / 14
    for _, bone in ipairs(kBones) do
        local a, b = bodies[bone[1]], bodies[bone[2]]
        graphics2d.drawLine(a.x, a.y, b.x, b.y, thickness, color, order)
        graphics2d.drawCircle(b.x, b.y, thickness / 2, color, order)
        -- Forearms and shins reach past their centers to the hands and feet.
        if bone[2]:find('^lower') then
            local tipX, tipY = b.x + (b.x - a.x) * 0.5, b.y + (b.y - a.y) * 0.5
            graphics2d.drawLine(b.x, b.y, tipX, tipY, thickness, color, order)
        end
    end
    local head = bodies.head
    graphics2d.drawCircle(head.x, head.y, kHeight / 11, color, {layer = 2})
end

function Ragdoll:render()
    self:beginWorld()
    parts.drawAll(self.statics)
    for index, doll in ipairs(self.dolls) do
        self:drawDoll(doll, kColors[index % #kColors + 1])
    end
    if self.outlines then
        self.world:debugDraw({layer = 3})
    end
    self.grab:draw()
end

return Ragdoll
