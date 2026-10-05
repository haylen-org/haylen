-- Blend modes: the same emitter with each blend of the renderer, over a light band and a dark band. Additive and screen brighten, multiply darkens and alpha covers. Only the premultiplied blend takes colors already multiplied by their alpha, so that column uses a premultiplied image and colors.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')

local ParticleTest = require('categories.particles.particle-test')

local BlendModes = haylen.class('BlendModes', ParticleTest)

BlendModes.modes = {'alpha', 'additive', 'multiply', 'screen', 'premultiplied'}
BlendModes.straight = {texture = 'soft', colors = {'#FFFF9040', '#80FF5020', '#00FF2010'}}
BlendModes.premultiplied = {texture = 'soft_premultiplied', colors = {'#FFFF9040', '#80802810', '#00000000'}}

function BlendModes:init(entry)
    BlendModes.super.init(self, entry)
    self.emitters = {}
    for index, mode in ipairs(BlendModes.modes) do
        local look = mode == 'premultiplied' and BlendModes.premultiplied or BlendModes.straight
        local emitter = particles2d.newEmitter({texture = ParticleTest.texture(look.texture), rate = 40, lifetime = {1.6, 2}, speed = {200, 260}, direction = -1.5708, spread = 0.4, startSize = {50, 70}, endSize = {20, 30}, colors = look.colors, blend = mode, layer = 2, seed = 80 + index})
        emitter.position = {(index - 3) * 330, 250}
        self.emitters[index] = emitter
    end
    self.names = '"' .. table.concat(BlendModes.modes, '", "') .. '"'
end

function BlendModes:enter()
    self:frame{hint = 'Each column blends the same orange particles its own way over the light and the dark band.'}
end

function BlendModes:update(dt)
    BlendModes.super.update(self, dt)
    for _, emitter in ipairs(self.emitters) do
        emitter:update(dt)
    end
    self:status('Blend modes ' .. self.names)
end

function BlendModes:draw(area)
    local view = graphics2d.canvasBounds()
    graphics2d.drawRect({view.x, view.y, view.width, view.height / 2}, '#FFB8C0D0')
    graphics2d.drawRect({view.x, view.y + view.height / 2, view.width, view.height / 2}, '#FF141820')
    graphics2d.drawRect({view.x, 20, view.width, 60}, '#FF4060C0')
    for index, emitter in ipairs(self.emitters) do
        emitter:draw()
        ParticleTest.label('Blend "' .. BlendModes.modes[index] .. '"', emitter.x, 300)
    end
end

return BlendModes
