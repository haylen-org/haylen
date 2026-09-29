-- Unshaded and emissive draws: unshaded draws keep their colors whatever the light, and emission adds the colors of a draw on top of the light, so windows and neon glow at night.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local ui = require('haylen.ui')

local sample = require('sample')

local Emission = haylen.class('Emission', sample.Test)

Emission.hints = 'The street lamp on the cursor lights the houses. Windows and the neon sign use emission, and the road signs are unshaded.'

Emission.houses = {
    {x = -820, width = 420, height = 460, color = '#FF7A6A5A'},
    {x = -300, width = 360, height = 620, color = '#FF6A7488'},
    {x = 180, width = 480, height = 400, color = '#FF8A6A6A'},
}

function Emission:init(entry)
    Emission.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.cursor = sample.Cursor()
    self.emission = 1
    self.unshaded = true
    self.lamp = lighting2d.newLight({radius = 520, color = '#FFFFD8A0', intensity = 1.2})
    self.time = 0
end

function Emission:controls()
    return {
        ui.formField{label = 'Emission of windows and neon', ui.slider{min = 0, max = 3, value = self.emission, showValue = true, onChange = function(event)
            self.emission = event.value
        end}},
        ui.toggle{align = 'stretch', text = 'Unshaded road signs', checked = true, onChange = function(event)
            self.unshaded = event.checked
        end},
    }
end

function Emission:update(dt)
    Emission.super.update(self, dt)
    self.cursor:update(dt)
    self.time = self.time + dt
    self.lamp.x, self.lamp.y = self.cursor:world(self.camera)
    self:setStatus(string.format('emission %.2f, unshaded %s', self.emission, tostring(self.unshaded)))
end

function Emission:drawHouse(house)
    local ground = 300
    local top = ground - house.height
    graphics2d.drawRect({house.x, top, house.width, house.height}, house.color, {layer = 1})
    graphics2d.drawPolygon({{house.x - 30, top}, {house.x + house.width / 2, top - 140}, {house.x + house.width + 30, top}}, '#FF4A3030', {layer = 1})
    for row = 0, math.floor((house.height - 120) / 130) do
        for column = 0, math.floor((house.width - 100) / 120) do
            local lit = (row * 3 + column + math.floor(house.x)) % 4 ~= 0
            local color = lit and '#FFFFD27A' or '#FF2A3040'
            graphics2d.drawRect({house.x + 50 + column * 120, top + 50 + row * 130, 60, 80}, color, {layer = 2, emission = lit and self.emission or 0})
        end
    end
end

function Emission:drawNeon()
    local pulse = 0.75 + 0.25 * math.sin(self.time * 6)
    local order = {layer = 2, emission = self.emission * pulse}
    graphics2d.drawRing(760, -120, 90, 12, '#FFFF40C0', order)
    graphics2d.drawLine(700, -120, 820, -120, 12, '#FF40E0FF', order)
    graphics2d.drawLine(760, -180, 760, -60, 12, '#FF40E0FF', order)
    graphics2d.draw(graphics2d.lightTexture(), 760, -120, {width = 420, height = 420, color = '#60FF40C0', blend = 'additive', layer = 3, emission = self.emission * pulse})
end

function Emission:drawSigns()
    local order = {layer = 2, unshaded = self.unshaded}
    for index, color in ipairs({'#FF30A0FF', '#FFFFC020', '#FF30D070'}) do
        local x = -700 + index * 380
        graphics2d.drawRect({x - 6, 330, 12, 150}, '#FF505058', {layer = 2, unshaded = self.unshaded})
        graphics2d.drawPolygon({{x - 70, 300}, {x + 50, 300}, {x + 90, 340}, {x + 50, 380}, {x - 70, 380}}, color, order)
    end
end

function Emission:render()
    graphics2d.beginWorld(self.camera, {ambientLight = '#FF141A30'})
    graphics2d.drawRect({-1600, -900, 3200, 1200}, '#FF2A3450')
    graphics2d.drawRect({-1600, 300, 3200, 700}, '#FF3A3A40')
    for _, house in ipairs(Emission.houses) do
        self:drawHouse(house)
    end
    self:drawNeon()
    self:drawSigns()
    graphics2d.drawLight(self.lamp)
end

function Emission:renderUi()
    graphics2d.beginScreen()
    self.cursor:draw()
end

return Emission
