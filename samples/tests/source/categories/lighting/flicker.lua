-- Flicker: `lighting2d.flicker` turns time into a flame-like multiplier, and a seed per torch keeps the torches on the wall and the campfire in the courtyard from wavering in step. Their flames are painted flame frames that particles play.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local LightingTest = require('categories.lighting.lighting-test')
local Room = require('categories.lighting.room')
local Town = require('categories.lighting.town')

local Flicker = haylen.class('Flicker', LightingTest)

Flicker.code = [[
local torch = lighting2d.newLight({x = -380, y = -330, radius = 380, color = '#FFFFA050'})
local wave = lighting2d.flicker(haylen.elapsed(), {speed = 8, amount = 0.25, seed = 3})
torch.intensity = 1.2 * wave  -- Each torch has a seed of its own.
graphics2d.drawLight(torch)]]

Flicker.wall = -300
Flicker.brick = 112
Flicker.torchScale = 0.62
Flicker.campfire = {x = -60, y = 90}
Flicker.props = 'particles/props.json'
Flicker.effects = 'particles/library/textures/effects.json'

-- Makes the flames of a fire from the painted flame frames, licking upward and fading from white through orange into red, glowing in the dark with emission.
function Flicker.flames(atlas, size, rate, seed)
    local frames = {}
    for index = 1, 4 do
        frames[index] = atlas:source('flame-' .. index)
    end
    return particles2d.newEmitter({texture = atlas.texture, frames = frames, frameMode = 'loopRandomStart', frameRate = 14, rate = rate, prewarm = 1, maxParticles = 64, lifetime = {0.45, 0.7}, speed = {size * 0.9, size * 1.5}, direction = -1.5708, spread = 0.18, shape = 'ellipse', shapeSize = {size * 0.3, size * 0.08}, startSize = {size, size * 1.3}, endSize = {size * 0.25, size * 0.35}, sizeCurve = 'quadIn', colors = {'#00FFE7A8', '#C0FFC24A', '#A0FF7A1F', '#50D8381F', '#00801010'}, colorTimes = {0, 0.12, 0.45, 0.8, 1}, emission = 1.2, layer = 4, seed = seed})
end

function Flicker:init(entry)
    Flicker.super.init(self, entry)
    self.room = Room()
    self.town = Town()
    self.bricks = Room.bricks()
    self.props = assets.load(Flicker.props, 'atlas', {filter = 'linear'})
    local effects = assets.load(Flicker.effects, 'atlas', {filter = 'linear'})
    self.ambientLight = '#FF0E1016'
    self.speed = 8
    self.amount = 0.25
    self.shared = false
    self.torches = {}
    for index = 1, 5 do
        local x = (index - 3) * 380
        local flame = Flicker.flames(effects, 58, 22, index)
        flame.position = {x, Flicker.wall - 10 - 312 * Flicker.torchScale + 24}
        self.torches[index] = {seed = index, wave = 1, flame = flame, light = lighting2d.newLight({x = x, y = Flicker.wall - 60, radius = 420, color = '#FFFFA050', intensity = 1.2})}
    end
    local fire = Flicker.flames(effects, 120, 30, 9)
    fire.position = {Flicker.campfire.x, Flicker.campfire.y - 10}
    self.fire = {seed = 9, wave = 1, flame = fire, light = lighting2d.newLight({x = Flicker.campfire.x, y = Flicker.campfire.y - 40, radius = 560, color = '#FFFF8A30', intensity = 1.5})}
end

function Flicker:enter()
    self:frame{
        hint = 'Tune how fast and how deep the flames waver. With one seed for every torch they all flicker together.',
        code = Flicker.code,
        controls = {
            ui.formField{label = 'Speed', ui.slider{id = 'speed', min = 1, max = 24, value = self.speed, showValue = true, decimals = 1, onChange = function(event)
                self.speed = event.value
            end}},
            ui.formField{label = 'Amount', ui.slider{id = 'amount', min = 0, max = 0.8, value = self.amount, showValue = true, onChange = function(event)
                self.amount = event.value
            end}},
            ui.toggle{id = 'shared', text = 'One seed for every torch', onChange = function(event)
                self.shared = event.checked
            end},
        },
        focus = 'speed',
    }
end

-- Returns the flicker of a flame now, with its own seed unless every torch shares one.
function Flicker:wave(flame)
    return lighting2d.flicker(haylen.elapsed(), {speed = self.speed, amount = self.amount, seed = self.shared and 0 or flame.seed})
end

function Flicker:update(dt)
    Flicker.super.update(self, dt)
    for _, torch in ipairs(self.torches) do
        torch.wave = self:wave(torch)
        torch.light.intensity = 1.2 * torch.wave
        torch.flame:update(dt)
    end
    self.fire.wave = self:wave(self.fire)
    self.fire.light.intensity = 1.5 * self.fire.wave
    self.fire.light.radius = 560 * (0.9 + 0.1 * self.fire.wave)
    self.fire.flame:update(dt)
    local together = self.shared and 'all together' or 'each on its own'
    self:status(string.format('The flames waver %s, and the campfire burns at %.0f percent now.', together, self.fire.wave * 100))
end

-- The wall of brick tiles at the top of the courtyard, laid from its foot upward as one sprite whose source repeats the bricks, with the torches fixed to it.
function Flicker:drawWall()
    local canvas = graphics2d.canvasBounds()
    local size = Flicker.brick
    local left = math.floor(canvas.x / size) * size
    local top = Flicker.wall - math.ceil((Flicker.wall - canvas.y) / size) * size
    local width, height = canvas:right() - left, Flicker.wall - top
    graphics2d.draw(self.bricks, left, top, {source = {0, 0, self.bricks.width * width / size, self.bricks.height * height / size}, width = width, height = height, pivotX = 0, pivotY = 0, layer = 1})
    graphics2d.drawRect({canvas.x, Flicker.wall - 6, canvas.width, 16}, '#FF1E1B33', {layer = 2})
    for _, torch in ipairs(self.torches) do
        self.town:piece('wall-torch', torch.light.x, Flicker.wall - 10, Flicker.torchScale, {pivotY = 1, layer = 3})
        torch.flame:draw()
        graphics2d.drawLight(torch.light)
    end
end

function Flicker:draw(area)
    self.room:draw()
    self:drawWall()
    local source = self.props:source('campfire')
    graphics2d.draw(self.props.texture, Flicker.campfire.x, Flicker.campfire.y + 40, {source = source, width = source.width * 0.55, height = source.height * 0.55, pivotY = 1, layer = 3})
    self.fire.flame:draw()
    graphics2d.drawLight(self.fire.light)
end

return Flicker
