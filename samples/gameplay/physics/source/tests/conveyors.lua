-- Conveyor belts made with the tangent speed of their surface, carrying crates down a zigzag of belts that can speed up, slow down and turn around.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Grab = require('grab')
local parts = require('parts')
local sample = require('sample')

local Conveyors = haylen.class('Conveyors', sample.Test)

local kBelts = {{-250, -250, 800, 140}, {250, -90, 800, -140}, {-250, 70, 800, 140}, {250, 230, 800, -160}}
local kBeltHeight = 22
local kStripeGap = 40
local kMaxCrates = 30

function Conveyors:enter()
    Conveyors.super.enter(self, {
        hint = 'Crates ride the belts from the top down. Drag them against the flow, and change the speed or turn the belts around. R or X clears the crates.',
        controls = {
            ui.label{text = 'Belt speed', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'speed', value = 1, min = 0, max = 3, step = 0.1, showValue = true, onChange = function(event) self:setSpeed(event.value, self.direction) end},
            ui.button{id = 'reverse', text = 'Turn the belts around', onClick = function() self:setSpeed(self.speed, -self.direction) end},
            ui.toggle{id = 'spawn', text = 'Spawn crates', checked = true, onChange = function(event) self.spawning = event.checked end},
            ui.button{id = 'reset', text = 'Clear the crates', onClick = function() self:clear() end},
        },
        stats = true,
        focus = 'speed',
    })
    self.random = m.random(29)
    self.speed, self.direction, self.spawning, self.travel = 1, 1, true, 0
    self.world = physics2d.newWorld()
    self.grab = Grab.new(self.world)
    self.crates = {}
    self.belts = {}
    for _, spec in ipairs(kBelts) do
        local body = self.world:createBody({type = 'static', x = spec[1], y = spec[2]})
        local shape = parts.box(body, spec[3], kBeltHeight, {friction = 0.9, tangentSpeed = spec[4]})
        parts.paint(body, '#FF455A64')
        self.belts[#self.belts + 1] = {body = body, shape = shape, width = spec[3], base = spec[4]}
    end
    local floor = self.world:createBody({type = 'static', x = 0, y = 410})
    parts.box(floor, 1600, 40)
    self.statics = {floor}
    timer.every(0.8, function()
        if self.spawning then
            self:spawn()
        end
    end, {owner = self})
end

function Conveyors:setSpeed(speed, direction)
    self.speed, self.direction = speed, direction
    for _, belt in ipairs(self.belts) do
        belt.shape.tangentSpeed = belt.base * speed * direction
    end
end

function Conveyors:spawn()
    local x = self.direction > 0 and -600 or 600
    local crate = self.world:createBody({x = x + self.random:range(-40, 40), y = -400, rotation = self.random:range(-0.2, 0.2)})
    parts.box(crate, 44, 44, {friction = 0.8})
    self.crates[#self.crates + 1] = crate
    if #self.crates > kMaxCrates then
        table.remove(self.crates, 1):destroy()
    end
end

function Conveyors:clear()
    for _, crate in ipairs(self.crates) do
        crate:destroy()
    end
    self.crates = {}
end

function Conveyors:exit()
    Conveyors.super.exit(self)
    self.world, self.grab, self.crates, self.belts, self.statics = nil, nil, nil, nil, nil
end

function Conveyors:update(dt)
    Conveyors.super.update(self, dt)
    if input.pressed('reset') then
        self:clear()
    end
    self.grab:update(self.pointer, self.camera)
    self.travel = self.travel + dt
    self:showStats(string.format('crates %d\nspeed x%.1f\nstep %.2f ms', #self.crates, self.speed * self.direction, sample.milliseconds('physics step')))
end

function Conveyors:fixedUpdate(step)
    sample.step(self.world, step)
end

-- Stripes slide along the top of each belt at the tangent speed of its surface.
function Conveyors:drawStripes(belt)
    local speed = belt.shape.tangentSpeed
    local left = belt.body.x - belt.width / 2
    local top = belt.body.y - kBeltHeight / 2
    local shift = (self.travel * speed) % kStripeGap
    for x = shift, belt.width - 8, kStripeGap do
        graphics2d.drawRect({left + x, top + 3, 8, kBeltHeight - 6}, '#FFFFD54F', {layer = 1})
    end
end

function Conveyors:render()
    self:beginWorld()
    parts.drawAll(self.statics)
    for _, belt in ipairs(self.belts) do
        parts.draw(belt.body)
        self:drawStripes(belt)
    end
    parts.drawAll(self.crates, {layer = 2})
    self.grab:draw()
end

return Conveyors
