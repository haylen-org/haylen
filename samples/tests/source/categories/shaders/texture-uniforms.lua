-- Textures as uniforms: a material takes textures by name like any value, so the same shader recolors with any gradient, including a render target redrawn every frame.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local ShaderTest = require('categories.shaders.shader-test')

local TextureUniforms = haylen.class('TextureUniforms', ShaderTest)

TextureUniforms.code = [[
local material = graphics2d.newMaterial(assets.shader('shaders/texture-map.shader'), {pattern_scale = 3})
material:set('ramp_texture', assets.texture('shaders/images/ramp-fire.png'))
material:set('pattern_texture', stripes)
local live = graphics.newRenderTarget(256, 4, {filter = 'linear'})
material:set('ramp_texture', live.texture)  -- A render target redrawn every frame.
graphics2d.draw(planet, x, y, {width = 300, height = 300, material = material})]]

function TextureUniforms:init(entry)
    TextureUniforms.super.init(self, entry)
    self.live = graphics.newRenderTarget(256, 4, {filter = 'linear'})
    self.liveCamera = graphics2d.newCamera()
    self.liveCamera.position = {128, 2}
    self.ramps = {fire = ShaderTest.ramp('fire'), ice = ShaderTest.ramp('ice'), toxic = ShaderTest.ramp('toxic'), live = self.live.texture}
    self.patterns = {stripes = ShaderTest.pattern(), noise = ShaderTest.noise()}
    self.ramp = 'fire'
    self.material = ShaderTest.material('texture-map', {pattern_strength = 0.2, pattern_scale = 3, ramp_texture = self.ramps.fire, pattern_texture = self.patterns.stripes})
    self.cells = ShaderTest.cells(3, 3)
    self.textureNames = '"' .. table.concat(self.material.shader.textures, '" and "') .. '"'
end

function TextureUniforms:enter()
    self:frame{
        code = TextureUniforms.code,
        hint = 'Pick the gradient the brightness maps through and the pattern laid over it. The live gradient is a render target drawn every frame.',
        controls = {
            ui.formField{label = 'Gradient texture', ui.radioGroup{id = 'ramp', selected = 'fire', items = {{id = 'fire', text = 'Fire'}, {id = 'ice', text = 'Ice'}, {id = 'toxic', text = 'Toxic'}, {id = 'live', text = 'Live render target'}}, onChange = function(event)
                self.ramp = event.value
                self.material:set('ramp_texture', self.ramps[event.value])
            end}},
            ui.formField{label = 'Pattern texture', ui.radioGroup{id = 'pattern', selected = 'stripes', items = {{id = 'stripes', text = 'Stripes'}, {id = 'noise', text = 'Noise'}, {id = 'none', text = 'None, white'}}, onChange = function(event)
                self.material:set('pattern_texture', self.patterns[event.value])
            end}},
        },
        focus = 'ramp',
    }
end

function TextureUniforms:update(dt)
    TextureUniforms.super.update(self, dt)
    local time = haylen.elapsed()
    self.material:set('scroll', {time * 0.1, time * 0.05})
    self:status(string.format('The shader reads the textures %s, and the gradient is "%s".', self.textureNames, self.ramp))
end

-- Redraws the live gradient: four bands of color that slide along it.
function TextureUniforms:renderLive()
    graphics2d.beginTarget(self.live, self.liveCamera, {clear = '#FF000000'})
    local time = haylen.elapsed()
    for index = 0, 3 do
        local color = m.fromHsv(time * 0.1 + index * 0.25, 0.8, 0.3 + index * 0.23)
        graphics2d.drawRect({index * 64, 0, 64, 4}, color)
    end
end

-- The live gradient draws before the stage that shows it.
function TextureUniforms:render()
    self:renderLive()
    TextureUniforms.super.render(self)
end

function TextureUniforms:draw(area)
    local cells = self.cells
    self:drawCard(cells[1], 'Planet', true)
    self:drawPicture(cells[1], 'planet', cells[1].size, {material = self.material})
    self:drawCard(cells[2], 'Knight')
    self:drawPicture(cells[2], 'knight', cells[2].size * 1.1, {material = self.material})

    -- The gradient the brightness maps through, as a swatch in a frame on a card of its own.
    local rect = cells[3].rect
    graphics2d.drawShape(rect, {radius = 20, color = '#FF232938', borderWidth = 3, borderColor = ShaderTest.ink})
    local width = rect[3] - 80
    graphics2d.drawShape({rect[1] + 40, cells[3].y - 40, width, 80}, {radius = 18, texture = self.ramps[self.ramp], borderWidth = 4, borderColor = ShaderTest.ink, layer = 2})
    graphics2d.drawText(nil, 'Dark', rect[1] + 40, cells[3].y + 60, {size = 22, color = '#FFB9C0E0', layer = 2})
    graphics2d.drawText(nil, 'Bright', rect[1] + 40 + width, cells[3].y + 60, {size = 22, color = '#FFB9C0E0', anchor = {1, 0}, layer = 2})
    local title = 'The gradient texture'
    local plate = graphics2d.measureText(self:font(), title, {size = 26}) + 36
    graphics2d.drawShape({rect[1] + rect[3] / 2 - plate / 2, rect[2] + rect[4] - 52, plate, 40}, {radius = 20, color = '#E62F3352', borderWidth = 2, borderColor = ShaderTest.ink, layer = 9})
    graphics2d.drawText(self:font(), title, rect[1] + rect[3] / 2, rect[2] + rect[4] - 32, {size = 26, anchor = {0.5, 0.5}, outlineWidth = 3, outlineColor = ShaderTest.ink, layer = 10})
end

return TextureUniforms
