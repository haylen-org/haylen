-- Intensity above 1: where the light map holds floating point, strong lights brighten the scene past its unlit colors instead of stopping at white.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local ui = require('haylen.ui')

local sample = require('sample')
local Stage = require('stage')

local Hdr = haylen.class('Hdr', sample.Test)

Hdr.hints = 'The fixed lights shine with intensity 1, 2 and 4. Move the cursor light and raise its intensity to overexpose the floor.'

function Hdr:init(entry)
    Hdr.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.stage = Stage()
    self.cursor = sample.Cursor()
    self.fixed = {}
    for index, intensity in ipairs({1, 2, 4}) do
        self.fixed[index] = lighting2d.newLight({x = (index - 2) * 520, y = -40, radius = 260, color = '#FFFFD8A0', intensity = intensity})
    end
    self.held = lighting2d.newLight({radius = 300, color = '#FFA0D0FF', intensity = 3})
end

function Hdr:controls()
    return {ui.formField{label = 'Intensity of the cursor light', ui.slider{min = 0, max = 6, value = self.held.intensity, showValue = true, onChange = function(event)
        self.held.intensity = event.value
    end}}}
end

function Hdr:update(dt)
    Hdr.super.update(self, dt)
    self.cursor:update(dt)
    self.held.x, self.held.y = self.cursor:world(self.camera)
    local mode = graphics2d.hdrLighting() and 'floating point, light goes past 1' or 'saturates at 1 on this backend'
    self:setStatus(string.format('intensity %.2f, light map %s', self.held.intensity, mode))
end

function Hdr:render()
    graphics2d.beginWorld(self.camera, {ambientLight = '#FF202430'})
    self.stage:draw(true)
    for _, light in ipairs(self.fixed) do
        graphics2d.drawLight(light)
    end
    graphics2d.drawLight(self.held)
end

function Hdr:renderUi()
    graphics2d.beginScreen()
    for _, light in ipairs(self.fixed) do
        local x, y = self.camera:worldToScreen(light.x, light.y + 300)
        graphics2d.drawText(nil, string.format('intensity %g', light.intensity), x, y, {size = 36, anchor = {0.5, 0.5}, outlineWidth = 3})
    end
    self.cursor:draw()
end

return Hdr
