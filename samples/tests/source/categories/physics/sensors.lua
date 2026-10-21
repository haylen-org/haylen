-- A sensor zone that counts the balls inside it, and the contact, hit and sensor events a world reports while balls rain on pegs.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Burst = require('categories.physics.burst')
local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Sensors = haylen.class('Sensors', PhysicsTest)

Sensors.code = [[
zone:addBox(520, 140, {sensor = true})  -- Reports overlaps and never collides.
world.onSensorBegin = function(sensor, visitor) inside = inside + 1 end
world.onSensorEnd = function(sensor, visitor) inside = inside - 1 end
world.onContactBegin = function(a, b, contact) begun = begun + 1 end
world.onHit = function(a, b, contact) spark(contact.x, contact.y, contact.speed) end]]

Sensors.ground = 330
Sensors.zone = {0, 185, 520, 140}
Sensors.maxBalls = 40
Sensors.logLines = 6
Sensors.colors = {'red', 'blue', 'yellow', 'green', 'purple', 'orange'}

function Sensors:enter()
    self:frame{
        code = Sensors.code,
        hint = 'Balls fall through the glowing sensor zone, which counts them without touching them. Drag balls in and out, R or the X button clears them.',
        controls = {
            ui.toggle{id = 'rain', text = 'Rain balls', checked = true, onChange = function(event) self.raining = event.checked end},
            ui.button{id = 'drop', text = 'Drop a ball', onClick = function() self:dropBall() end},
            ui.button{id = 'clear', text = 'Clear the balls', onClick = function() self:clear() end},
            ui.label{text = 'Latest events', font = 'heading'},
            ui.label{id = 'log', text = 'Waiting for the first ball.', font = 'caption', color = 'textMuted'},
        },
        focus = 'rain',
        view = {1300, 720},
    }
    self.raining = true
    self.random = m.random(5)
    self.counts = {begin = 0, finish = 0, hits = 0}
    self.events = {}
    self.pulse = 0
    self.sparks = Burst('combat/stomp-stars', {count = 8, scale = 0.4, layer = 25})
    self:build()
    for index = 1, 8 do
        self:dropBall(-420 + index * 95, -300 - index * 40)
    end
    timer.every(0.35, function()
        if self.raining then
            self:dropBall()
        end
    end, {owner = self})
end

function Sensors:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.balls = {}

    self.ground = self.world:createBody({type = 'static', x = 0, y = Sensors.ground + 30})
    self.ground:addBox(8000, 60)
    self.statics = {}
    for row = 0, 3 do
        for column = 0, 6 - row % 2 do
            local peg = self.world:createBody({type = 'static', x = -420 + column * 140 + (row % 2) * 70, y = -250 + row * 80})
            peg:addCircle(13, {restitution = 0.2})
            Look.dress(peg, 'steel')
            self.statics[#self.statics + 1] = peg
        end
    end

    local zone = Sensors.zone
    self.zone = self.world:createBody({type = 'static', x = zone[1], y = zone[2]})
    self.zone:addBox(zone[3], zone[4], {sensor = true})

    self.world.onContactBegin = function(a, b, contact)
        self.counts.begin = self.counts.begin + 1
    end
    self.world.onContactEnd = function(a, b, contact)
        self.counts.finish = self.counts.finish + 1
    end
    self.world.onHit = function(a, b, contact)
        self.counts.hits = self.counts.hits + 1
        if contact.speed > 260 then
            self.sparks:fire(contact.x, contact.y, math.min(1.4, 0.5 + contact.speed / 900))
        end
        self:note(string.format('A ball hit %s at %.0f per second.', self:struck(a, b), contact.speed))
    end
    self.world.onSensorBegin = function(sensor, visitor, shapes)
        visitor.data.inside = true
        self.pulse = 1
        self:note('A ball entered the zone.')
    end
    self.world.onSensorEnd = function(sensor, visitor, shapes)
        if visitor then
            visitor.data.inside = false
            self:note('A ball left the zone.')
        end
    end
end

-- Names what a ball struck, which is the other body of the hit.
function Sensors:struck(a, b)
    local other = a.type == 'dynamic' and b or a
    if other.type == 'dynamic' then
        return 'another ball'
    end
    return other == self.ground and 'the ground' or 'a peg'
end

function Sensors:dropBall(x, y)
    local ball = self.world:createBody({x = x or self.random:range(-460, 460), y = y or -420})
    ball:addCircle(self.random:range(16, 26), {restitution = 0.4, density = 1})
    Look.dress(ball, Sensors.colors[self.random:integer(1, #Sensors.colors)])
    self.balls[#self.balls + 1] = ball
    if #self.balls > Sensors.maxBalls then
        table.remove(self.balls, 1):destroy()
    end
end

function Sensors:clear()
    for _, ball in ipairs(self.balls) do
        ball:destroy()
    end
    self.balls = {}
end

function Sensors:note(text)
    table.insert(self.events, 1, text)
    self.events[Sensors.logLines + 1] = nil
    self.logChanged = true
end

-- Counts the balls the sensor events marked as inside, so balls destroyed inside the zone drop out of the count.
function Sensors:visitors()
    local count = 0
    for _, ball in ipairs(self.balls) do
        count = count + (ball.data.inside and 1 or 0)
    end
    return count
end

function Sensors:update(dt)
    Sensors.super.update(self, dt)
    if input.pressed('reset') then
        self:clear()
    end
    self.grab:update(self.pointer, self.camera)
    self.sparks:update(dt)
    self.pulse = math.max(0, self.pulse - dt * 3)
    local inside = self:visitors()
    self:status(string.format('%d %s in the zone. Contacts began %d times and ended %d times, with %d hits.', inside, inside == 1 and 'ball' or 'balls', self.counts.begin, self.counts.finish, self.counts.hits))
    if self.logChanged then
        self.logChanged = false
        self:set('log', {text = table.concat(self.events, '\n')})
    end
end

function Sensors:fixedUpdate(step)
    self:simulate(self.world, step)
end

-- The zone is a field of light that brightens while it holds balls and flashes when one enters, with its count in the middle.
function Sensors:drawZone()
    local zone = Sensors.zone
    local inside = self:visitors()
    local unit = Look.pixel
    local glow = (inside > 0 and 0.32 or 0.18) + self.pulse * 0.25
    local area = {zone[1] - zone[3] / 2, zone[2] - zone[4] / 2, zone[3], zone[4]}
    graphics2d.drawShape(area, {radius = 18, color = {0.2, 0.95, 0.75, glow}, borderWidth = 4 * unit, borderColor = {0.55, 1, 0.85, 0.75 + self.pulse * 0.25}, layer = -1})
    local shift = (haylen.elapsed() * 40) % 40
    graphics2d.pushClip(area)
    for x = area[1] - zone[4] + shift, area[1] + area[3], 40 do
        graphics2d.drawStroke({x, area[2] + area[4], x + zone[4], area[2]}, {width = 10, color = {1, 1, 1, 0.12 + self.pulse * 0.1}, layer = -1})
    end
    graphics2d.popClip()
    self:tag(tostring(inside), zone[1], zone[2] - 8, {size = 52 + self.pulse * 14, layer = 3})
    self:tag(inside == 1 and 'ball inside' or 'balls inside', zone[1], zone[2] + 40, {size = 18, layer = 3})
end

function Sensors:draw(area)
    Scenery.sky(self.camera, Sensors.ground)
    Scenery.ground(Sensors.ground)
    Look.drawAll(self.statics)
    Look.drawAll(self.balls, {layer = 1})
    self:drawZone()
    self.sparks:draw()
    self.grab:draw()
end

return Sensors
