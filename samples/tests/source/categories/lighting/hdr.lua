-- Intensity above 1: where the light map holds floating point, strong lights brighten the scene past its unlit colors instead of stopping at white.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local ui = require('haylen.ui')

local LightingTest = require('categories.lighting.lighting-test')
local Room = require('categories.lighting.room')

local Hdr = haylen.class('Hdr', LightingTest)

Hdr.code = [[
local lamp = lighting2d.newLight({x = 520, y = -40, radius = 260, color = '#FFFFD8A0', intensity = 4})
print(graphics2d.hdrLighting())  -- Whether the light map holds light past 1 on this backend.
graphics2d.beginWorld(camera, {ambientLight = '#FF202430'})
graphics2d.drawLight(lamp)]]

function Hdr:init(entry)
    Hdr.super.init(self, entry)
    self.room = Room()
    self.ambientLight = '#FF202430'
    self.fixed = {}
    for index, intensity in ipairs({1, 2, 4}) do
        self.fixed[index] = lighting2d.newLight({x = (index - 2) * 520, y = -40, radius = 260, color = '#FFFFD8A0', intensity = intensity})
    end
    self.held = lighting2d.newLight({radius = 300, color = '#FFA0D0FF', intensity = 3})
    self.pointer.origin = {0.18, 0.8}
end

function Hdr:enter()
    self:frame{
        hint = 'The fixed lights shine with intensity 1, 2 and 4. Move the cursor light and raise its intensity to overexpose the floor.',
        code = Hdr.code,
        cursor = true,
        controls = {ui.formField{label = 'Intensity of the cursor light', ui.slider{id = 'intensity', min = 0, max = 6, value = self.held.intensity, showValue = true, onChange = function(event)
            self.held.intensity = event.value
        end}}},
    }
end

function Hdr:update(dt)
    Hdr.super.update(self, dt)
    self.held.x, self.held.y = self.cursorX, self.cursorY
    local mode = graphics2d.hdrLighting() and 'holds floating point, so light goes past 1' or 'stops at 1 on this backend'
    self:status(string.format('The cursor light shines at %.2f, and the light map %s.', self.held.intensity, mode))
end

function Hdr:draw(area)
    self.room:draw()
    for _, light in ipairs(self.fixed) do
        Room.drawLamp(light.x, light.y, light.color, 26)
        graphics2d.drawLight(light)
    end
    Room.drawLamp(self.held.x, self.held.y, self.held.color, 26)
    graphics2d.drawLight(self.held)
end

function Hdr:renderUi()
    if not self.stage then
        return
    end
    Hdr.super.renderUi(self)
    for _, light in ipairs(self.fixed) do
        local x, y = self.camera:worldToScreen(light.x, light.y - 290)
        graphics2d.drawText(nil, string.format('Intensity %g', light.intensity), x, y, {size = 34, anchor = {0.5, 1}, outlineWidth = 3})
    end
end

return Hdr
