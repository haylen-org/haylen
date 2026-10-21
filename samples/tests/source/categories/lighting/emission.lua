-- Unshaded and emissive draws on a village street at night: unshaded draws keep their colors whatever the light, such as the painted signposts, and emission adds the colors of a draw on top of the light, so the windows and the neon sign of the coffee shop glow in the dark.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local m = require('haylen.math')
local ui = require('haylen.ui')

local LightingTest = require('categories.lighting.lighting-test')
local Room = require('categories.lighting.room')
local Town = require('categories.lighting.town')

local Emission = haylen.class('Emission', LightingTest)

Emission.code = [[
graphics2d.beginWorld(camera, {ambientLight = '#FF141A30'})
graphics2d.drawRect(window, '#FFFFC870', {emission = 1})  -- Glows in the dark.
graphics2d.draw(scenery.texture, x, y, {source = tubes, emission = 1.5})  -- The neon.
graphics2d.draw(tiles, x, y, {source = signpost, unshaded = true})  -- Keeps its colors.
graphics2d.drawLight(lamp)]]

Emission.ground = 300
Emission.neonScale = 0.82
Emission.houses = {{name = 'cottage', x = -620, scale = 0.72}, {name = 'stone-house', x = 40, scale = 0.72}, {name = 'cottage', x = 700, scale = 0.72}}
Emission.signpost = {794, 398, 128, 128}
Emission.signposts = {-1080, -300, 1180}

function Emission:init(entry)
    Emission.super.init(self, entry)
    self.town = Town()
    self.tiles = assets.texture(Room.tileset, {filter = 'linear'})
    self.ambientLight = '#FF141A30'
    self.emission = 1
    self.unshaded = true
    self.lamp = lighting2d.newLight({radius = 560, color = '#FFFFD8A0', intensity = 1.6})
    self.pointer.origin = {0.36, 0.72}
    local random = m.random(29)
    self.stars = {}
    for index = 1, 70 do
        self.stars[index] = {x = random:range(-1400, 1400), y = random:range(-540, -200), size = random:range(1.5, 3.5), phase = random:range(0, m.tau)}
    end
end

function Emission:enter()
    self:frame{
        hint = 'The lantern on the cursor lights the street: move it with the mouse, a finger, the arrows, WASD or a stick. The windows and the neon glow with emission, and the signposts are unshaded.',
        code = Emission.code,
        cursor = true,
        controls = {
            ui.formField{label = 'Emission of the windows and the neon', ui.slider{id = 'emission', min = 0, max = 3, value = self.emission, showValue = true, onChange = function(event)
                self.emission = event.value
            end}},
            ui.toggle{id = 'unshaded', text = 'Unshaded signposts', checked = true, onChange = function(event)
                self.unshaded = event.checked
            end},
        },
    }
end

function Emission:update(dt)
    Emission.super.update(self, dt)
    self.lamp.x, self.lamp.y = self.cursorX, self.cursorY
    local signs = self.unshaded and 'keep their painted colors in the dark' or 'take the light like everything else'
    self:status(string.format('The windows and the neon glow with an emission of %.2f, and the signposts %s.', self.emission, signs))
end

-- The coffee shop hangs its neon sign from the wall of the stone house, and the neon pulses like a tube that warms up.
function Emission:drawNeon(x, y)
    local pulse = 0.85 + 0.15 * math.sin(haylen.elapsed() * 5) * math.sin(haylen.elapsed() * 1.7)
    local scale = Emission.neonScale
    self.town:piece('neon-sign', x, y, scale, {pivotX = 0, pivotY = 0, layer = 4})
    self.town:piece('neon-tubes', x, y, scale, {pivotX = 0, pivotY = 0, layer = 5, emission = self.emission * pulse * 1.5})
    local width, height = self.town:size('neon-sign')
    graphics2d.draw(graphics2d.lightTexture(), x + width * scale * 0.58, y + height * scale * 0.62, {width = 440, height = 360, color = '#40FF3CA0', layer = 6, emission = self.emission * pulse * 0.8})
end

function Emission:draw(area)
    local time = haylen.elapsed()
    self.town:sky('#FF070B22', '#FF1C2650', -40)
    for _, star in ipairs(self.stars) do
        graphics2d.drawCircle(star.x, star.y, star.size, m.color(1, 1, 0.92, 0.5 + 0.4 * math.sin(time * 2 + star.phase)), {layer = -18, unshaded = true})
    end
    self.town:piece('moon', 860, -400, 0.4, {layer = -16, unshaded = true})
    self.town:land(-160, Emission.ground)

    for _, house in ipairs(Emission.houses) do
        self.town:house(house.name, house.x, Emission.ground + 14, house.scale, 2, self.emission)
    end
    local stone = Emission.houses[2]
    local width, height = self.town:size(stone.name)
    self:drawNeon(stone.x + width * stone.scale / 2 - 46, Emission.ground - height * stone.scale + 60)

    for _, x in ipairs(Emission.signposts) do
        graphics2d.draw(self.tiles, x, Emission.ground + 40, {source = Emission.signpost, width = 200, height = 200, pivotY = 1, layer = 3, unshaded = self.unshaded})
    end
    Room.drawLamp(self.lamp.x, self.lamp.y, self.lamp.color, 30)
    graphics2d.drawLight(self.lamp)
end

return Emission
