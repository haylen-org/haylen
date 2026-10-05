-- Trails and ribbons: the `trail` option draws a ribbon behind every particle of an emitter, all of them in one mesh, and `particles2d.newTrail` makes a single ribbon that follows a point, such as the tip of a blade, adding a point whenever it moved `minDistance` and dropping the points older than its lifetime.
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local TrailsRibbons = haylen.class('TrailsRibbons', ParticleTest)

TrailsRibbons.neon = {'#FFFF40C0', '#FF40E0FF', '#FFB0FF40', '#FFFFB030'}

function TrailsRibbons:init(entry)
    TrailsRibbons.super.init(self, entry)
    self.width = 36
    self.lifetime = 0.3
    self.length = 16
    self.time = 0

    self.sparks = particles2d.newEmitter({texture = ParticleTest.library('hard_dot'), rate = 14, lifetime = {1.4, 1.8}, speed = {500, 700}, direction = -1.5708, spread = 1.4, gravity = {0, 520}, startSize = 14, endSize = 8, colors = {'#FFFFFFFF', '#FFFFFFFF', '#00FFFFFF'}, colorTimes = {0, 0.8, 1}, tints = TrailsRibbons.neon, tintMode = 'cycle', trail = {length = self.length, lifetime = 0.35, widthStart = 10, widthEnd = 0, colors = {'#FFFFFFFF', '#00FFFFFF'}}, blend = 'additive', maxParticles = 64, layer = 3, seed = 321})
    self.sparks.x, self.sparks.y = -420, 380

    self.blade = particles2d.newTrail({lifetime = self.lifetime, minDistance = 6, maxPoints = 96, widthStart = self.width, widthEnd = 0, colors = {'#FFFFFFFF', '#C080E0FF', '#002060FF'}, texture = ParticleTest.library('glow_line'), blend = 'additive', layer = 4})
    self.wisp = particles2d.newTrail({lifetime = 0.8, minDistance = 8, widthStart = 22, widthEnd = 2, colors = {'#FFFFE080', '#80FF6040', '#00FF2000'}, blend = 'additive', layer = 2})
    self.ribbons = {self.blade, self.wisp}
end

function TrailsRibbons:enter()
    self:frame{
        hint = 'The blade follows the cursor: swing it with the mouse, a finger, the arrows, WASD or a stick. The neon sparks on the left draw a ribbon each, and a wisp circles on the right.',
        cursor = true,
        controls = {
            ui.formField{label = 'Width of the blade', ui.slider{id = 'width', min = 4, max = 90, value = self.width, showValue = true, decimals = 0, onChange = function(event)
                self.width = event.value
                self.blade:configure({widthStart = event.value})
            end}},
            ui.formField{label = 'Lifetime of the blade', ui.slider{id = 'lifetime', min = 0.05, max = 1.2, value = self.lifetime, showValue = true, onChange = function(event)
                self.lifetime = event.value
                self.blade:configure({lifetime = event.value})
            end}},
            ui.formField{label = 'Points of each spark ribbon', ui.slider{id = 'length', min = 0, max = 48, step = 1, value = self.length, showValue = true, decimals = 0, onChange = function(event)
                self.length = math.floor(event.value)
                self.sparks:configure({trail = {length = self.length, lifetime = 0.35, widthStart = 10, widthEnd = 0, colors = {'#FFFFFFFF', '#00FFFFFF'}}})
            end}},
        },
    }
end

function TrailsRibbons:update(dt)
    TrailsRibbons.super.update(self, dt)
    self.time = self.time + dt
    self.blade.x, self.blade.y = self.cursorX, self.cursorY
    self.wisp.x, self.wisp.y = 520 + math.cos(self.time * 2.2) * 260, -40 + math.sin(self.time * 3.1) * 200
    ParticleTest.updateAll(self.ribbons, dt)
    self.sparks:update(dt)
    self:report('Spark ribbons of %d points on %d sparks   Blade %d points, width %.0f, lifetime %.2f s   Wisp %d points', self.length, self.sparks.count, self.blade.count, self.width, self.lifetime, self.wisp.count)
end

function TrailsRibbons:draw(area)
    ParticleTest.backdrop({0.02, 0.01, 0.06}, {0.06, 0.03, 0.12})
    self.sparks:draw()
    ParticleTest.drawAll(self.ribbons)
    ParticleTest.label('Sparks with "trail"', self.sparks.x, 420)
    ParticleTest.label('A ribbon of "newTrail"', 520, 280)
end

return TrailsRibbons
