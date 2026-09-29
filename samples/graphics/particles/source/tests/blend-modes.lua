-- Blend modes: the same emitter with each blend of the renderer, over a light band and a dark band. Additive and screen brighten, multiply darkens and alpha covers. Multiply, screen and premultiplied blend colors already multiplied by their alpha, so those columns use a premultiplied image and colors.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')

local art = require('art')
local sample = require('sample')

local BlendModes = haylen.class('BlendModes', sample.Test)

BlendModes.hints = 'Each column blends the same orange particles its own way over the light and the dark band.'

BlendModes.modes = {'alpha', 'additive', 'multiply', 'screen', 'premultiplied'}
BlendModes.straight = {texture = 'soft', colors = {'#FFFF9040', '#80FF5020', '#00FF2010'}}
BlendModes.premultiplied = {texture = 'soft_premultiplied', colors = {'#FFFF9040', '#80802810', '#00000000'}}

function BlendModes:init(entry)
    BlendModes.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.emitters = {}
    for index, mode in ipairs(BlendModes.modes) do
        local look = (mode == 'alpha' or mode == 'additive') and BlendModes.straight or BlendModes.premultiplied
        local emitter = particles2d.newEmitter({texture = art.texture(look.texture), rate = 40, lifetime = {1.6, 2}, speed = {200, 260}, direction = -1.5708, spread = 0.4, startSize = {50, 70}, endSize = {20, 30}, colors = look.colors, blend = mode, layer = 2, seed = 80 + index})
        emitter.position = {(index - 3) * 330, 250}
        self.emitters[index] = emitter
    end
end

function BlendModes:update(dt)
    for _, emitter in ipairs(self.emitters) do
        emitter:update(dt)
    end
    self:setStatus('blend modes: ' .. table.concat(BlendModes.modes, ', '))
end

function BlendModes:render()
    graphics2d.beginWorld(self.camera)
    local area = graphics2d.canvasBounds()
    graphics2d.drawRect({area.x, area.y, area.width, area.height / 2}, '#FFB8C0D0')
    graphics2d.drawRect({area.x, area.y + area.height / 2, area.width, area.height / 2}, '#FF141820')
    graphics2d.drawRect({area.x, 20, area.width, 60}, '#FF4060C0')
    for _, emitter in ipairs(self.emitters) do
        emitter:draw()
    end
end

function BlendModes:renderUi()
    graphics2d.beginScreen()
    for index, emitter in ipairs(self.emitters) do
        art.label(self.camera, BlendModes.modes[index], emitter.x, 300)
    end
end

return BlendModes
