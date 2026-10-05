-- Post-processing chain: the `postProcess` options of a world canvas add bloom, a vignette, saturation, contrast and brightness, color grading through a lookup texture, chromatic aberration, pixelation and blur, applied in one chain when the canvas is composited. The lookup textures are made in Lua as 256 by 16 strips, and the presets set every value at once.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')
local Scenery = require('categories.effects.scenery')

local PostProcessing = haylen.class('PostProcessing', ParticleTest)

PostProcessing.library = 'particles/library/'
PostProcessing.lutSteps = 16
PostProcessing.sliders = {
    {'bloomStrength', 'Bloom strength', 0, 2},
    {'bloomThreshold', 'Bloom threshold', 0, 1.5},
    {'bloomRadius', 'Bloom radius', 1, 48, 0},
    {'vignetteStrength', 'Vignette strength', 0, 1},
    {'vignetteRadius', 'Vignette radius', 0, 1},
    {'saturation', 'Saturation', 0, 2},
    {'contrast', 'Contrast', 0.5, 1.5},
    {'brightness', 'Brightness', 0.3, 1.5},
    {'colorLutStrength', 'Grading strength', 0, 1},
    {'chromaticAberration', 'Chromatic aberration', 0, 12, 1},
    {'pixelate', 'Pixel size', 0, 16, 0},
    {'blur', 'Blur radius', 0, 24, 1},
}
PostProcessing.defaults = {bloomStrength = 0, bloomThreshold = 0.8, bloomRadius = 12, vignetteStrength = 0, vignetteRadius = 0.6, saturation = 1, contrast = 1, brightness = 1, colorLutStrength = 1, chromaticAberration = 0, pixelate = 0, blur = 0, grade = 'none'}
PostProcessing.presets = {
    {id = 'clean', text = 'Clean', values = {}},
    {id = 'dream', text = 'Dream', values = {bloomStrength = 1.2, bloomThreshold = 0.55, bloomRadius = 28, blur = 2, saturation = 1.25, vignetteStrength = 0.35}},
    {id = 'retro', text = 'Retro', values = {pixelate = 6, chromaticAberration = 3, grade = 'sepia', colorLutStrength = 0.8, vignetteStrength = 0.5, contrast = 1.15}},
    {id = 'paused', text = 'Paused', values = {blur = 12, saturation = 0.35, brightness = 0.7, vignetteStrength = 0.6}},
}
PostProcessing.grades = {
    {id = 'none', text = 'None'},
    {id = 'warm', text = 'Warm sunset', grade = function(r, g, b) return r * 1.1 + 0.03, g * 0.98 + 0.01, b * 0.78 end},
    {id = 'cold', text = 'Cold morning', grade = function(r, g, b) return r * 0.82, g * 0.95 + 0.02, b * 1.12 + 0.04 end},
    {id = 'sepia', text = 'Old photo', grade = function(r, g, b) return r * 0.393 + g * 0.769 + b * 0.189, r * 0.349 + g * 0.686 + b * 0.168, r * 0.272 + g * 0.534 + b * 0.131 end},
    {id = 'night', text = 'Moonlight', grade = function(r, g, b)
        local light = r * 0.3 + g * 0.59 + b * 0.11
        return light * 0.45, light * 0.62, light * 0.95 + 0.06
    end},
}

-- Makes the lookup texture of a grading: square cells side by side, one for each step of blue, with red across a cell and green down it.
function PostProcessing.lut(grade)
    local steps = PostProcessing.lutSteps
    local pixels = {}
    for green = 0, steps - 1 do
        for blue = 0, steps - 1 do
            for red = 0, steps - 1 do
                local r, g, b = grade(red / (steps - 1), green / (steps - 1), blue / (steps - 1))
                pixels[#pixels + 1] = string.char(math.floor(math.max(0, math.min(1, r)) * 255 + 0.5), math.floor(math.max(0, math.min(1, g)) * 255 + 0.5), math.floor(math.max(0, math.min(1, b)) * 255 + 0.5), 255)
            end
        end
    end
    return graphics.newTexture(steps * steps, steps, {pixels = table.concat(pixels), filter = 'linear'})
end

function PostProcessing.newSystem(name)
    return particles2d.newSystem(assets.load(PostProcessing.library .. name .. '.particles'), {seed = 9})
end

function PostProcessing:init(entry)
    PostProcessing.super.init(self, entry)
    self.scenery = Scenery()
    self.robot = assets.texture('sprites/images/hero.png', {filter = 'linear'})
    self.luts = {}
    for _, grade in ipairs(PostProcessing.grades) do
        if grade.grade then
            self.luts[grade.id] = PostProcessing.lut(grade.grade)
        end
    end
    self.campfire = PostProcessing.newSystem('fire/campfire_realistic')
    self.campfire.position = {-560, Scenery.ground - 30}
    self.campfire.scale = 2
    self.neon = PostProcessing.newSystem('fire/neon_flame')
    self.neon.position = {560, Scenery.ground - 30}
    self.neon.scale = 2
    self.torch = PostProcessing.newSystem('fire/torch_flame')
    self.torch.scale = 2
    self.burst = PostProcessing.newSystem('explosions/explosion_neon')
    self.burst.emitting = false
    self.systems = {self.campfire, self.neon, self.torch, self.burst}
    self.enabled = true
    self.post = {}
    self:apply(PostProcessing.presets[1])
end

-- Sets every value to its default and then to the values of the preset.
function PostProcessing:apply(preset)
    for key, value in pairs(PostProcessing.defaults) do
        self.post[key] = value
    end
    for key, value in pairs(preset.values) do
        self.post[key] = value
    end
    self.grade = self.post.grade
    self.post.grade = nil
    self.post.colorLut = self.luts[self.grade]
end

function PostProcessing:showValues()
    for _, slider in ipairs(PostProcessing.sliders) do
        self:set(slider[1], {value = self.post[slider[1]]})
    end
    self:set('grade', {selected = self.grade})
end

function PostProcessing:enter()
    -- The presets sit two by two, so their names fit the panel.
    local presets = {{}, {}}
    for index, preset in ipairs(PostProcessing.presets) do
        local row = presets[(index + 1) // 2]
        row[#row + 1] = ui.button{id = preset.id, text = preset.text, grow = 1, onClick = function()
            self:apply(preset)
            self:showValues()
        end}
    end
    local grades = {}
    for index, grade in ipairs(PostProcessing.grades) do
        grades[index] = {id = grade.id, text = grade.text}
    end
    local controls = {
        ui.row{gap = 8, children = presets[1]},
        ui.row{gap = 8, children = presets[2]},
        ui.toggle{id = 'enabled', text = 'Post-processing on', checked = self.enabled, onChange = function(event) self.enabled = event.checked end},
        ui.formField{label = 'Color grading', ui.radioGroup{id = 'grade', items = grades, selected = self.grade, onChange = function(event)
            self.grade = event.value
            self.post.colorLut = self.luts[event.value]
        end}},
    }
    for _, slider in ipairs(PostProcessing.sliders) do
        local key = slider[1]
        controls[#controls + 1] = ui.formField{label = slider[2], ui.slider{id = key, min = slider[3], max = slider[4], value = self.post[key], showValue = true, decimals = slider[5], onChange = function(event)
            self.post[key] = event.value
        end}}
    end
    self:frame{
        hint = 'The torch follows the cursor, and a click, a tap, E, Enter, Space or the south button sets off a bright burst there.',
        cursor = true,
        controls = controls,
    }
end

function PostProcessing:update(dt)
    PostProcessing.super.update(self, dt)
    self.scenery:update(dt)
    self.torch.position = {self.cursorX, self.cursorY}
    if self.stage and self:pressed() then
        self.burst.position = {self.cursorX, self.cursorY}
        self.burst:restart()
    end
    ParticleTest.updateAll(self.systems, dt)
    local stats = graphics2d.stats()
    self:report('Passes %d   Draw calls %d   Grading "%s"   Bloom %.2f   Blur %.1f', stats.passes, stats.drawCalls, self.grade, self.post.bloomStrength, self.post.blur)
end

function PostProcessing:render()
    if not self.area then
        return
    end
    graphics2d.beginWorld(self.camera, {postProcess = self.enabled and self.post or nil})
    graphics2d.drawRect(graphics2d.canvasBounds(), ParticleTest.stageColor, {layer = -1000})
    self:draw(self.area)
end

function PostProcessing:draw(area)
    self.scenery:draw()
    graphics2d.draw(self.robot, 0, Scenery.ground, {scaleX = 2.6, scaleY = 2.6, pivotY = 1, layer = 1})
    ParticleTest.drawAll(self.systems)
end

return PostProcessing
