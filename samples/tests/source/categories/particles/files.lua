-- Effect files: every `.particles` file in `content/particles/effects` is a JSON emitter that `assets.load` reads as a `ParticleEffect`, the template of new emitters.
local assets = require('haylen.assets')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local Files = haylen.class('Files', ParticleTest)

function Files:init(entry)
    Files.super.init(self, entry)
    self.paths = {}
    for _, path in ipairs(assets.list('particles/effects')) do
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

function Files:enter()
    local items = {}
    for index, path in ipairs(self.paths) do
        items[index] = {id = path, text = 'File "' .. path:match('[^/]+$') .. '"'}
    end
    self:frame{
        hint = 'Pick an effect file. The emitter follows the cursor: move it with the mouse, a finger, the arrows, WASD or a stick.',
        cursor = true,
        controls = {
            ui.formField{label = 'Effect file', ui.radioGroup{id = 'file', items = items, selected = self.path, onChange = function(event)
                self:choose(event.value)
            end}},
            ui.button{id = 'restart', text = 'Restart', onClick = function()
                self.emitter:restart()
            end},
        },
    }
end

function Files:update(dt)
    Files.super.update(self, dt)
    self.emitter.position = {self.cursorX, self.cursorY}
    self.emitter:update(dt)
    local config = self.effect.config
    self:status(string.format('Effect "%s" with "%s"   Rate %g   Lifetime %g to %g s   Particles %d', self.path, self.effect.texturePath, config.rate, config.lifetime[1], config.lifetime[2], self.emitter.count))
end

function Files:draw(area)
    ParticleTest.backdrop({0.04, 0.05, 0.1}, {0.1, 0.1, 0.18})
    self.emitter:draw()
end

return Files
