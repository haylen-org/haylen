-- Intensity above 1: where the light map holds floating point, strong lights brighten the scene past its unlit colors instead of stopping at white.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local ui = require('haylen.ui')

local LightingTest = require('categories.lighting.lighting-test')
local Room = require('categories.lighting.room')

local Hdr = haylen.class('Hdr', LightingTest)

function Hdr:init(entry)
    Hdr.super.init(self, entry)
    self.room = Room()
    self.ambientLight = '#FF202430'
    self.fixed = {}
    for index, intensity in ipairs({1, 2, 4}) do
        self.fixed[index] = lighting2d.newLight({x = (index - 2) * 520, y = -40, radius = 260, color = '#FFFFD8A0', intensity = intensity})
    end
    self.held = lighting2d.newLight({radius = 300, color = '#FFA0D0FF', intensity = 3})
end

function Hdr:enter()
    self:frame{
        hint = 'The fixed lights shine with intensity 1, 2 and 4. Move the cursor light and raise its intensity to overexpose the floor.',
        cursor = true,
        controls = {ui.formField{label = 'Intensity of the cursor light', ui.slider{id = 'intensity', min = 0, max = 6, value = self.held.intensity, showValue = true, onChange = function(event)
            self.held.intensity = event.value
        end}}},
    }
end

function Hdr:update(dt)
    Hdr.super.update(self, dt)
    self.held.x, self.held.y = self.cursorX, self.cursorY
    local mode = graphics2d.hdrLighting() and 'Floating point, light goes past 1' or 'Saturates at 1 on this backend'
    self:status(string.format('Intensity %.2f   Light map: %s', self.held.intensity, mode))
end

function Hdr:draw(area)
    self.room:draw()
    for _, light in ipairs(self.fixed) do
        graphics2d.drawLight(light)
    end
    graphics2d.drawLight(self.held)
end

function Hdr:renderUi()
    if not self.stage then
        return
    end
    Hdr.super.renderUi(self)
    for _, light in ipairs(self.fixed) do
        local x, y = self.camera:worldToScreen(light.x, light.y + 300)
        graphics2d.drawText(nil, string.format('Intensity %g', light.intensity), x, y, {size = 36, anchor = {0.5, 0.5}, outlineWidth = 3})
    end
end

return Hdr
