-- Many lights: every light is one quad in the light pass, so hundreds of them stay cheap, while each light with shadows adds a shadow map cast on the CPU.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local m = require('haylen.math')
local ui = require('haylen.ui')

local LightingTest = require('categories.lighting.lighting-test')
local Room = require('categories.lighting.room')

local Stress = haylen.class('Stress', LightingTest)

Stress.maxLights = 2048

function Stress:init(entry)
    Stress.super.init(self, entry)
    self.room = Room()
    self.ambientLight = '#FF101218'
    self.count = 256
    self.shadows = false
    self.frameTime = 1 / 60
    self.lights = {}
    local random = m.random(3)
    for index = 1, Stress.maxLights do
        self.lights[index] = {
            speedX = random:range(0.2, 0.9),
            speedY = random:range(0.2, 0.9),
            phase = random:range(0, m.tau),
            light = lighting2d.newLight({radius = random:range(50, 140), color = m.fromHsv(random:nextFloat(), 0.7, 1), intensity = 0.6}),
        }
    end
end

function Stress:enter()
    self:frame{
        hint = 'Raise the number of lights and give shadows to the first sixteen to see what they cost.',
        controls = {
            ui.formField{label = 'Lights', ui.slider{id = 'lights', min = 16, max = Stress.maxLights, step = 16, value = self.count, showValue = true, decimals = 0, onChange = function(event)
                self.count = math.floor(event.value)
            end}},
            ui.toggle{id = 'shadows', text = 'Shadows on 16 lights', onChange = function(event)
                self.shadows = event.checked
            end},
        },
        focus = 'lights',
    }
end

function Stress:update(dt)
    Stress.super.update(self, dt)
    local time = haylen.elapsed()
    for index = 1, self.count do
        local entry = self.lights[index]
        entry.light.x = math.sin(time * entry.speedX + entry.phase) * 900
        entry.light.y = math.cos(time * entry.speedY + entry.phase * 1.7) * 480
        entry.light.shadows = self.shadows and index <= 16
    end

    self.frameTime = m.lerp(self.frameTime, haylen.unscaledDelta(), 0.05)
    local stats = graphics2d.stats()
    self:status(string.format('Lights %d   Shadow maps %d   Draw calls %d   %.0f frames per second', stats.lights, stats.shadows, stats.drawCalls, 1 / math.max(self.frameTime, 0.0001)))
end

function Stress:draw(area)
    self.room:draw()
    for index = 1, self.count do
        graphics2d.drawLight(self.lights[index].light)
    end
end

return Stress
