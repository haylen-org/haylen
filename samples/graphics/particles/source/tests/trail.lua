-- Trails: an emitter that moves leaves its particles where they were born, because particles live in world space, so a high rate and no speed draw a trail.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')

local art = require('art')
local sample = require('sample')

local Trail = haylen.class('Trail', sample.Test)

Trail.hints = 'Draw with the cursor: move it with the mouse, a finger, WASD or the left stick. The comet circles on its own.'

function Trail:init(entry)
    Trail.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.cursor = sample.Cursor()
    local soft = art.texture('soft')
    self.pen = particles2d.newEmitter({texture = soft, rate = 240, lifetime = {0.7, 0.9}, speed = {0, 20}, spread = 6.2832, startSize = {34, 40}, endSize = 0, colors = {'#FFA0F0FF', '#FF6080FF', '#00A040FF'}, blend = 'additive', maxParticles = 512, layer = 2, seed = 10})
    self.comet = particles2d.newEmitter({texture = soft, rate = 200, lifetime = {1, 1.4}, speed = {0, 30}, spread = 6.2832, startSize = {60, 70}, endSize = 4, colors = {'#FFFFF0C0', '#FFFF9030', '#00FF2000'}, blend = 'additive', maxParticles = 512, layer = 1, seed = 11})
    self.dust = particles2d.newEmitter({texture = art.texture('spark'), rate = 60, lifetime = {1, 2}, speed = {20, 80}, spread = 6.2832, gravity = {0, 40}, startSize = {4, 7}, endSize = 0, colors = {'#FFFFE0A0', '#00FF8040'}, blend = 'additive', layer = 3, seed = 12})
    self.time = 0
end

function Trail:update(dt)
    self.cursor:update(dt)
    self.time = self.time + dt
    self.pen.position = {self.cursor:world(self.camera)}
    local x, y = math.cos(self.time * 1.3) * 520, math.sin(self.time * 1.3) * 280 + 60
    self.comet.position = {x, y}
    self.dust.position = {x, y}
    for _, emitter in ipairs({self.pen, self.comet, self.dust}) do
        emitter:update(dt)
    end
    self:setStatus(string.format('%d particles in the trail of the cursor, %d in the comet', self.pen.count, self.comet.count + self.dust.count))
end

function Trail:render()
    graphics2d.beginWorld(self.camera)
    art.backdrop({0.02, 0.02, 0.06}, {0.06, 0.04, 0.12})
    for _, emitter in ipairs({self.comet, self.pen, self.dust}) do
        emitter:draw()
    end
end

function Trail:renderUi()
    graphics2d.beginScreen()
    self.cursor:draw()
end

return Trail
