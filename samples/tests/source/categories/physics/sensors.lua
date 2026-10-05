-- A sensor zone that counts the balls inside it, and the contact, hit and sensor events a world reports while balls rain on pegs.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Sensors = haylen.class('Sensors', PhysicsTest)

local kMaxBalls = 40
local kLogLines = 6
local kSparkLife = 0.4

function Sensors:enter()
    self:frame{
        hint = 'Balls fall through the green sensor zone, which counts them without touching them. Drag balls in and out, R or the X button clears them.',
        controls = {
            ui.toggle{id = 'rain', text = 'Rain balls', checked = true, onChange = function(event) self.raining = event.checked end},
            ui.button{id = 'drop', text = 'Drop a ball', onClick = function() self:dropBall() end},
            ui.button{id = 'clear', text = 'Clear the balls', onClick = function() self:clear() end},
            ui.label{id = 'log', text = '', font = 'caption', color = 'textMuted'},
        },
        focus = 'rain',
    }
    self.raining = true
    self.random = m.random(5)
    self.counts = {begin = 0, finish = 0, hits = 0}
    self.events = {}
    self.sparks = {}
    self:build()
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

    local ground = self.world:createBody({type = 'static', x = 0, y = 410})
    parts.box(ground, 1600, 40)
    self.statics = {ground}
    for row = 0, 3 do
        for column = 0, 6 - row % 2 do
            local peg = self.world:createBody({type = 'static', x = -420 + column * 140 + (row % 2) * 70, y = -250 + row * 90})
            parts.circle(peg, 12, {restitution = 0.2})
            self.statics[#self.statics + 1] = peg
        end
    end

    self.zone = self.world:createBody({type = 'static', x = 0, y = 250})
    self.zone:addBox(520, 160, {sensor = true})

    self.world.onContactBegin = function(a, b, contact)
        self.counts.begin = self.counts.begin + 1
    end
    self.world.onContactEnd = function(a, b, contact)
        self.counts.finish = self.counts.finish + 1
    end
    self.world.onHit = function(a, b, contact)
        self.counts.hits = self.counts.hits + 1
        self.sparks[#self.sparks + 1] = {x = contact.x, y = contact.y, radius = math.min(40, contact.speed / 25), life = kSparkLife}
        self:note(string.format('Hit at %.0f units per second', contact.speed))
    end
    self.world.onSensorBegin = function(sensor, visitor, shapes)
        visitor.data.inside = true
        self:note('A ball entered the zone')
    end
    self.world.onSensorEnd = function(sensor, visitor, shapes)
        if visitor then
            visitor.data.inside = false
            self:note('A ball left the zone')
        end
    end
end

function Sensors:dropBall()
    local ball = self.world:createBody({x = self.random:range(-460, 460), y = -400})
    parts.circle(ball, self.random:range(14, 24), {restitution = 0.4, density = 1})
    self.balls[#self.balls + 1] = ball
    if #self.balls > kMaxBalls then
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
    self.events[kLogLines + 1] = nil
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
    for index = #self.sparks, 1, -1 do
        local spark = self.sparks[index]
        spark.life = spark.life - dt
        if spark.life <= 0 then
            table.remove(self.sparks, index)
        end
    end
    self:status(string.format('In the zone %d, contacts begun %d, contacts ended %d, hits %d', self:visitors(), self.counts.begin, self.counts.finish, self.counts.hits))
    if self.logChanged then
        self.logChanged = false
        self:set('log', {text = table.concat(self.events, '\n')})
    end
end

function Sensors:fixedUpdate(step)
    self:simulate(self.world, step)
end

function Sensors:draw(area)
    local occupied = self:visitors() > 0
    graphics2d.drawRect({-260, 170, 520, 160}, occupied and '#5534C759' or '#2234C759')
    graphics2d.drawRectOutline({-260, 170, 520, 160}, 3, occupied and '#FF34C759' or '#8834C759')
    graphics2d.drawText(nil, 'Sensor zone', 0, 180, {size = 26, color = '#FFB9F6CA', anchor = {0.5, 0}})
    parts.drawAll(self.statics)
    parts.drawAll(self.balls, {layer = 1})
    for _, spark in ipairs(self.sparks) do
        local fade = spark.life / kSparkLife
        graphics2d.drawRing(spark.x, spark.y, spark.radius * (2 - fade), 3, m.color(1, 0.85, 0.3, fade), {layer = 5})
    end
    self.grab:draw()
end

return Sensors
