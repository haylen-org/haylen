-- Flicker: lighting2d.flicker turns time into a flame-like multiplier, and a seed per torch keeps them from wavering in step.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local ui = require('haylen.ui')

local sample = require('sample')
local Stage = require('stage')

local Flicker = haylen.class('Flicker', sample.Test)

Flicker.hints = 'Tune how fast and how deep the flames waver. With one seed for every torch they all flicker together.'

function Flicker:init(entry)
    Flicker.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.stage = Stage()
    self.speed = 8
    self.amount = 0.25
    self.shared = false
    self.torches = {}
    for index = 1, 5 do
        local x = (index - 3) * 380
        self.torches[index] = {seed = index, wave = 1, light = lighting2d.newLight({x = x, y = -330, radius = 380, color = '#FFFFA050', intensity = 1.2})}
    end
    self.fire = {seed = 9, wave = 1, light = lighting2d.newLight({x = -100, y = 60, radius = 520, color = '#FFFF8A30', intensity = 1.5})}
end

function Flicker:controls()
    return {
        ui.formField{label = 'Speed', ui.slider{min = 1, max = 24, value = self.speed, showValue = true, decimals = 1, onChange = function(event)
            self.speed = event.value
        end}},
        ui.formField{label = 'Amount', ui.slider{min = 0, max = 0.8, value = self.amount, showValue = true, onChange = function(event)
            self.amount = event.value
        end}},
        ui.toggle{align = 'stretch', text = 'One seed for every torch', onChange = function(event)
            self.shared = event.checked
        end},
    }
end

-- Returns the flicker of a flame now, with its own seed unless every torch shares one.
function Flicker:wave(flame)
    return lighting2d.flicker(haylen.time(), {speed = self.speed, amount = self.amount, seed = self.shared and 0 or flame.seed})
end

function Flicker:update(dt)
    Flicker.super.update(self, dt)
    for _, torch in ipairs(self.torches) do
        torch.wave = self:wave(torch)
        torch.light.intensity = 1.2 * torch.wave
    end
    self.fire.wave = self:wave(self.fire)
    self.fire.light.intensity = 1.5 * self.fire.wave
    self.fire.light.radius = 520 * (0.9 + 0.1 * self.fire.wave)
    self:setStatus(string.format('flicker of the campfire %.3f', self.fire.wave))
end

function Flicker:drawFlame(flame, size)
    local light = flame.light
    graphics2d.draw(graphics2d.lightTexture(), light.x, light.y, {width = size * flame.wave, height = size * 1.4 * flame.wave, color = '#FFFFB040', blend = 'additive', layer = 3, emission = 1.5})
end

function Flicker:render()
    graphics2d.beginWorld(self.camera, {ambientLight = '#FF0E1016'})
    self.stage:draw(true)
    graphics2d.drawRect({-1600, -420, 3200, 60}, '#FF4A4450', {layer = 1})
    for _, torch in ipairs(self.torches) do
        graphics2d.drawRect({torch.light.x - 8, torch.light.y, 16, 50}, '#FF5A3A20', {layer = 2})
        self:drawFlame(torch, 70)
        graphics2d.drawLight(torch.light)
    end
    graphics2d.drawCircle(self.fire.light.x, self.fire.light.y + 30, 60, '#FF3A2A1A', {layer = 2})
    self:drawFlame(self.fire, 160)
    graphics2d.drawLight(self.fire.light)
end

return Flicker
