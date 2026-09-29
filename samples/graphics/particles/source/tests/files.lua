-- Effect files: every .particles file in content/effects is a JSON emitter that assets.load reads as a ParticleEffect, the template of new emitters.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local art = require('art')
local sample = require('sample')

local Files = haylen.class('Files', sample.Test)

Files.hints = 'Pick an effect file. The emitter follows the cursor: move it with the mouse, a finger, WASD or the left stick.'

function Files:init(entry)
    Files.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.cursor = sample.Cursor()
    self.paths = {}
    for _, path in ipairs(assets.list('effects')) do
        if assets.typeForPath(path) == 'particles' then
            self.paths[#self.paths + 1] = path
        end
    end
    self:choose(self.paths[1])
end

function Files:choose(path)
    self.path = path
    self.effect = assets.load(path)
    self.emitter = particles2d.newEmitter(self.effect)
end

function Files:controls()
    local items = {}
    for index, path in ipairs(self.paths) do
        items[index] = {id = path, text = path:match('([^/]+)%.particles$')}
    end
    return {
        ui.formField{label = 'Effect file', ui.radioGroup{items = items, selected = self.path, onChange = function(event)
            self:choose(event.value)
        end}},
        ui.button{text = 'Restart', onClick = function()
            self.emitter:restart()
        end},
    }
end

function Files:update(dt)
    self.cursor:update(dt)
    self.emitter.position = {self.cursor:world(self.camera)}
    self.emitter:update(dt)
    local config = self.effect.config
    self:setStatus(string.format('%s with %s, rate %g, lifetime %g to %g s, %d particles', self.path, self.effect.texturePath, config.rate, config.lifetime[1], config.lifetime[2], self.emitter.count))
end

function Files:render()
    graphics2d.beginWorld(self.camera)
    art.backdrop({0.04, 0.05, 0.1}, {0.1, 0.1, 0.18})
    self.emitter:draw()
end

function Files:renderUi()
    graphics2d.beginScreen()
    self.cursor:draw()
end

return Files
