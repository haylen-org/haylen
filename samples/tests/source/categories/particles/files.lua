-- Effect files: every `.particles` file in `content/particles/effects` is a JSON emitter that `assets.load` reads as a `ParticleEffect`, the template of new emitters, shown by its name with the file it comes from.
local assets = require('haylen.assets')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local Files = haylen.class('Files', ParticleTest)

Files.code = [[
for _, path in ipairs(assets.list('particles/effects')) do
    if assets.typeForPath(path) == 'particles' then paths[#paths + 1] = path end
end
local effect = assets.load('particles/effects/sparks.particles')  -- A ParticleEffect.
local emitter = particles2d.newEmitter(effect)
log.info(effect.texturePath, effect.config.rate, effect.config.lifetime[1])]]

Files.ground = 330

-- The name of an effect from its file name, such as "Campfire" for "campfire.particles".
function Files.title(path)
    local name = path:match('([^/]+)%.particles$')
    return name:sub(1, 1):upper() .. name:sub(2):gsub('%-', ' ')
end

function Files:init(entry)
    Files.super.init(self, entry)
    self.paths = {}
    for _, path in ipairs(assets.list('particles/effects')) do
        if assets.typeForPath(path) == 'particles' then
            self.paths[#self.paths + 1] = path
        end
    end
    self:choose(self.paths[1])
    self.pointer.origin = {0.5, 0.72}
end

function Files:choose(path)
    self.path = path
    self.effect = assets.load(path)
    self.emitter = particles2d.newEmitter(self.effect)
end

function Files:enter()
    local items = {}
    for index, path in ipairs(self.paths) do
        items[index] = {id = path, text = Files.title(path)}
    end
    self:frame{
        hint = 'Pick an effect of the folder "particles/effects". The emitter follows the cursor: move it with the mouse, a finger, the arrows, WASD or a stick.',
        code = Files.code,
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
    self:report('%s from "%s" holds %d particles.', Files.title(self.path), self.path:match('[^/]+$'), self.emitter.count)
end

function Files:draw(area)
    ParticleTest.backdrop({0.06, 0.07, 0.14}, {0.16, 0.12, 0.2})
    ParticleTest.paveFloor('stone', Files.ground, 150, '#FF7A7488')
    if self.path:find('campfire') then
        ParticleTest.prop('campfire', self.emitter.x, self.emitter.y + 50, 0.5, {pivotY = 1, layer = 1})
    end
    self.emitter:draw()
end

return Files
