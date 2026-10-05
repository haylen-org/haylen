-- Textures as uniforms: a material takes textures by name like any value, so the same shader recolors with any gradient, including a render target redrawn every frame.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local ShaderTest = require('categories.shaders.shader-test')

local TextureUniforms = haylen.class('TextureUniforms', ShaderTest)

function TextureUniforms:init(entry)
    TextureUniforms.super.init(self, entry)
    self.hero = ShaderTest.hero()
    self.planet = ShaderTest.planet()
    self.live = graphics.newRenderTarget(256, 4, {filter = 'linear'})
    self.liveCamera = graphics2d.newCamera()
    self.liveCamera.position = {128, 2}
    self.ramps = {fire = ShaderTest.ramp('fire'), ice = ShaderTest.ramp('ice'), toxic = ShaderTest.ramp('toxic'), live = self.live.texture}
    self.patterns = {stripes = ShaderTest.pattern(), noise = ShaderTest.noise()}
    self.ramp = 'fire'
    self.material = ShaderTest.material('texture_map', {pattern_strength = 0.35, pattern_scale = 3, ramp_texture = self.ramps.fire, pattern_texture = self.patterns.stripes})
    self.cells = ShaderTest.cells(3, 3)
    self.textureNames = '"' .. table.concat(self.material.shader.textures, '", "') .. '"'
end

function TextureUniforms:enter()
    self:frame{
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
    self:status(string.format('Textures of the shader %s   Gradient "%s"', self.textureNames, self.ramp))
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
    graphics2d.draw(self.planet, cells[1].x, cells[1].y, {width = cells[1].size, height = cells[1].size, material = self.material})
    graphics2d.draw(self.hero, cells[2].x, cells[2].y, {width = cells[2].size, height = cells[2].size, material = self.material})
    graphics2d.draw(self.ramps[self.ramp], cells[3].x, cells[3].y, {width = cells[3].size, height = 60})
    ShaderTest.caption('Planet', cells[1].x, cells[1].y + cells[1].size / 2 + 8)
    ShaderTest.caption('Hero', cells[2].x, cells[2].y + cells[2].size / 2 + 8)
    ShaderTest.caption('The gradient texture', cells[3].x, cells[3].y + 50)
end

return TextureUniforms
