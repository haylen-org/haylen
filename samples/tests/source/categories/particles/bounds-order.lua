-- Bounds and draw order: `bounds` is a rectangle relative to the emitter that particles stay in, where `boundsMode = 'kill'` removes the ones that leave it and `'wrap'` brings them back on the other side, and `particleOrder` draws new particles above the older ones with `'oldestFirst'` or below them with `'newestFirst'`.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local BoundsOrder = haylen.class('BoundsOrder', ParticleTest)

BoundsOrder.code = [[
local rain = particles2d.newEmitter({texture = streak, rate = 150, speed = {950, 1100}})
rain:configure({bounds = {-400, -20, 800, 800}, boundsMode = 'kill'})  -- Dies at the ground.
snow:configure({bounds = {-260, -300, 520, 600}, boundsMode = 'wrap'})  -- Comes back.
smoke:configure({particleOrder = 'newestFirst'})  -- New puffs draw below the older ones.
rain:configure({bounds = false})  -- Removes the bounds.]]

BoundsOrder.cloud = -480
BoundsOrder.snowBox = {-220, -330, 520, 600}
BoundsOrder.order = {layer = 1}
BoundsOrder.captionY = 400
BoundsOrder.tile = 140

function BoundsOrder:init(entry)
    BoundsOrder.super.init(self, entry)
    self.ground = 300
    self.groundRect = {-1020, self.ground, 840, 700}
    self.bounded = true
    self.wind = 60

    self.rain = particles2d.newEmitter(ParticleTest.look('rain-streak', {rate = 150, lifetime = 3, speed = {950, 1100}, direction = 1.62, spread = 0.03, startSize = 44, endSize = 44, colors = {'#B0B0C8F0'}, shape = 'rectangle', shapeSize = {280, 4}, bounds = self:rainBounds(), boundsMode = 'kill', maxParticles = 512, layer = 2, seed = 241}))
    self.rain.x, self.rain.y = -600, BoundsOrder.cloud

    local box = BoundsOrder.snowBox
    self.snow = particles2d.newEmitter(ParticleTest.look('snowflake', {rate = 14, prewarm = 10, lifetime = {10, 12}, speed = {20, 60}, direction = 1.5708, spread = 1, gravity = {self.wind, 30}, turbulence = {strength = 30, frequency = 0.01, speed = 0.4}, startSize = {14, 28}, endSizeScale = 1, rotation = {0, m.tau}, spin = {-1, 1}, colors = {'#00FFFFFF', '#FFFFFFFF', '#FFFFFFFF', '#00FFFFFF'}, colorTimes = {0, 0.05, 0.9, 1}, shape = 'rectangle', shapeSize = {box[3] / 2, box[4] / 2}, bounds = {-box[3] / 2, -box[4] / 2, box[3], box[4]}, boundsMode = 'wrap', maxParticles = 256, layer = 2, seed = 242}))
    self.snow.x, self.snow.y = box[1] + box[3] / 2, box[2] + box[4] / 2

    local smoke = ParticleTest.look('smoke-puff', {rate = 12, lifetime = {3, 3.5}, speed = {60, 90}, spread = 0.4, startSize = {60, 80}, endSize = {180, 220}, rotation = {0, m.tau}, spin = {-0.5, 0.5}, colors = {'#FFFFC060', '#FFC07040', '#E0505060', '#00404050'}, maxParticles = 64, layer = 2})
    smoke.particleOrder, smoke.seed = 'oldestFirst', 243
    self.oldestFirst = particles2d.newEmitter(smoke)
    smoke.particleOrder, smoke.seed = 'newestFirst', 244
    self.newestFirst = particles2d.newEmitter(smoke)
    self.oldestFirst.x, self.oldestFirst.y = 460, 330
    self.newestFirst.x, self.newestFirst.y = 860, 330

    self.emitters = {self.rain, self.snow, self.oldestFirst, self.newestFirst}
end

-- Returns the bounds of the rain, from just above its cloud down to the ground, or `false`, which removes them.
function BoundsOrder:rainBounds()
    return self.bounded and {-400, -20, 800, self.ground - BoundsOrder.cloud + 20} or false
end

function BoundsOrder:enter()
    self:frame{
        hint = 'The rain dies where it reaches the ground because its bounds end there, and falls through it without them. The snow wraps around its box in the wind. The left smoke draws each new puff on top and the right one below the older puffs.',
        code = BoundsOrder.code,
        controls = {
            ui.toggle{id = 'bounded', text = 'Bounds on the rain', checked = self.bounded, onChange = function(event)
                self.bounded = event.checked
                self.rain:configure({bounds = self:rainBounds()})
            end},
            ui.formField{label = 'Height of the ground', ui.slider{id = 'ground', min = -100, max = 450, value = self.ground, showValue = true, decimals = 0, onChange = function(event)
                self.ground = event.value
                self.groundRect[2] = event.value
                self.rain:configure({bounds = self:rainBounds()})
            end}},
            ui.formField{label = 'Wind over the snow', ui.slider{id = 'wind', min = -200, max = 200, value = self.wind, showValue = true, decimals = 0, onChange = function(event)
                self.wind = event.value
                self.snow:configure({gravity = {event.value, 30}})
            end}},
        },
        focus = 'bounded',
    }
end

function BoundsOrder:update(dt)
    BoundsOrder.super.update(self, dt)
    ParticleTest.updateAll(self.emitters, dt)
    local rain = self.bounded and string.format('dies at the ground at %.0f', self.ground) or 'falls through the ground'
    self:report('%d raindrops: the rain %s, and %d flakes wrap around their box in a wind of %.0f.', self.rain.count, rain, self.snow.count, self.wind)
end

-- Paves the ground under the rain with stone tiles from its height down, with a dark rim along its top.
function BoundsOrder:drawGround()
    local tiles = assets.texture(ParticleTest.tiles, {filter = 'linear'})
    local rect, size = self.groundRect, BoundsOrder.tile
    for y = rect[2], rect[2] + rect[4], size do
        for x = rect[1], rect[1] + rect[3] - size, size do
            graphics2d.draw(tiles, x, y, {source = ParticleTest.floors.stone, width = size, height = size, pivotX = 0, pivotY = 0, color = '#FF7A8090', layer = 1})
        end
    end
    graphics2d.drawRect({rect[1], rect[2] - 4, rect[3], 8}, '#FF1E1B33', {layer = 1})
end

function BoundsOrder:draw(area)
    ParticleTest.backdrop({0.07, 0.08, 0.14}, {0.16, 0.18, 0.24})
    self:drawGround()
    graphics2d.drawShape(BoundsOrder.snowBox, {radius = 10, color = '#201E1B33', borderWidth = 6, borderColor = '#FF9AA6C8', layer = 1})
    ParticleTest.drawAll(self.emitters)
    ParticleTest.label('Rain that dies', -600, BoundsOrder.captionY, "boundsMode = 'kill'")
    ParticleTest.label('Snow that wraps', 40, BoundsOrder.captionY, "boundsMode = 'wrap'")
    ParticleTest.label('New on top', 460, BoundsOrder.captionY, "particleOrder = 'oldestFirst'")
    ParticleTest.label('New below', 860, BoundsOrder.captionY, "particleOrder = 'newestFirst'")
end

return BoundsOrder
