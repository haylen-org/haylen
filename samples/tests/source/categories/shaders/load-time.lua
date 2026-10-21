-- Shader load time: every shader of the category loads at once in the background and then draws on the planet with the values it shows in the other tests, in the frame right after the last one arrives. The results show how long the files took and how long that first frame took on the backend of the platform, which the rendering guide compares: Metal compiles the shader sources on worker threads while they load, and the other backends make each program when it first draws.
local assets = require('haylen.assets')
local datetime = require('datetime')
local debugging = require('haylen.debug')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local scene = require('haylen.scene')

local Results = require('harness.results')
local Test = require('harness.test')

local LoadTime = haylen.class('LoadTime', Test)

-- Every shader with the name it shows under its picture.
LoadTime.shaders = {
    {'blur', 'Blur'}, {'crt', 'Tube screen'}, {'dissolve', 'Dissolve'}, {'flash', 'Hit flash'}, {'grading', 'Color grading'}, {'hologram', 'Hologram'}, {'live', 'Live stripes'},
    {'outline', 'Outline'}, {'palette', 'Palette swap'}, {'pixelate', 'Pixelation'}, {'rainbow', 'Rainbow'}, {'texture-map', 'Gradient map'}, {'wave', 'Wave'},
}
LoadTime.columns = 7
-- The shaders made for the additive blend draw with it.
LoadTime.blends = {hologram = 'additive'}
LoadTime.code = [[
local shader = assets.loadAsync('shaders/wave.shader'):await()
local material = graphics2d.newMaterial(shader, {amplitude = 0.03, frequency = 12, speed = 4})
graphics2d.draw(texture, x, y, {material = material})  -- The first draw makes the GPU program.]]

-- Flattens a list of colors into the numbers of a `vec4` array uniform.
function LoadTime.colors(list)
    local numbers = {}
    for _, text in ipairs(list) do
        local color = m.color(text)
        table.move({color.r, color.g, color.b, color.a}, 1, 4, #numbers + 1, numbers)
    end
    return numbers
end

-- The values each shader draws the planet with, the ones it shows in the other tests of the category, so every picture shows its effect.
function LoadTime:uniforms(name)
    local texel = {1 / self.texture.width, 1 / self.texture.height}
    local values = {
        blur = {direction = {texel[1], 0}, radius = 16},
        crt = {curvature = 0.12, scanlines = 0.3, split = 0.004},
        dissolve = {amount = 0.45, edge_color = '#FFFFB347', edge_width = 0.1, noise_scale = 1, noise_texture = assets.texture('shaders/images/noise.png', {filter = 'linear', wrap = 'repeat'})},
        flash = {flash_color = '#FFFFFFFF', amount = 0.6},
        grading = {shadows = '#FF2A6A80', highlights = '#FFFFB070', strength = 0.9, contrast = 1.2},
        hologram = {tint = '#FF40E0FF', lines = 1.2, texel = texel},
        outline = {outline_color = '#FFFFC93C', texel = texel, thickness = 6},
        palette = {source_colors = LoadTime.colors({'#FF2F7FE0', '#FF5DBB3A', '#FFFFFFFF', '#FF000000'}), target_colors = LoadTime.colors({'#FFC04CE0', '#FFF0A030', '#FFFFFFFF', '#FF000000'})},
        pixelate = {blocks = {16, 16}},
        rainbow = {spread = 0.004},
        ['texture-map'] = {ramp_texture = assets.texture('shaders/images/ramp-fire.png', {filter = 'linear'}), pattern_texture = assets.texture('shaders/images/pattern.png', {filter = 'linear', wrap = 'repeat'}), pattern_strength = 0.2, pattern_scale = 3},
        wave = {amplitude = 0.03, frequency = 12, speed = 4},
    }
    return values[name] or {}
end

function LoadTime:enter()
    self.results = Results(self.entry.code)
    self.texture = assets.texture('shaders/images/planet.png', {filter = 'linear'})
    self.font = assets.font('fonts/lilita-one-regular.ttf')
    self.materials = {}
    self.started = haylen.frameIndex()
    self.loadFrames = 0
    self:frame{code = LoadTime.code, hint = 'Leave and open the test again to measure shaders the app already holds, whose first frame costs nothing extra.'}
    self.results:set('load', 'waiting', 'Shader files', string.format('Loading %d shaders.', #LoadTime.shaders))

    scene.spawn(self, function()
        local started = datetime.now():millis()
        local loads = {}
        for index, entry in ipairs(LoadTime.shaders) do
            loads[index] = assets.loadAsync('shaders/' .. entry[1] .. '.shader')
        end
        for index, load in ipairs(loads) do
            local shader, err = load:await()
            if not shader then
                self.results:set('load', 'fail', 'Shader files', err)
                return
            end
            self.materials[#self.materials + 1] = graphics2d.newMaterial(shader, self:uniforms(LoadTime.shaders[index][1]))
        end
        self.loadFrames = haylen.frameIndex() - self.started
        self.results:set('load', 'info', 'Shader files', string.format('%d shaders loaded in %.0f ms over %d %s.', #self.materials, datetime.now():millis() - started, self.loadFrames, self.loadFrames == 1 and 'frame' or 'frames'))
    end)
end

-- The profiler describes the frame before, so the frame that first drew the shaders is measured one frame later.
function LoadTime:update(dt)
    LoadTime.super.update(self, dt)
    if self.drawnFrame and not self.measured and haylen.frameIndex() > self.drawnFrame then
        self.measured = true
        self.results:set('first', 'info', 'First frame with every shader', string.format('%.1f ms on "%s".', debugging.frame().milliseconds, graphics.apiName()))
    end
    local time = haylen.elapsed()
    for _, material in ipairs(self.materials) do
        for _, uniform in ipairs(material.shader.uniforms) do
            if uniform.name == 'time' then
                material:set('time', time)
            end
        end
    end
    self:status(string.format('%d of %d shaders on the graphics API "%s", with frames of %.1f ms.', #self.materials, #LoadTime.shaders, graphics.apiName(), debugging.frame().milliseconds))
end

-- Draws each picture on a dark plate with its name under it, in rows whose last one sits in the middle.
function LoadTime:draw(area)
    local margin, gap = 24, 16
    local columns = LoadTime.columns
    local width = (area.width - margin * 2 - gap * (columns - 1)) / columns
    local size = math.min(width, (area.height - 260) / 2 - 44)
    local rows = math.ceil(#LoadTime.shaders / columns)
    if #self.materials == #LoadTime.shaders then
        self.drawnFrame = self.drawnFrame or haylen.frameIndex()
        for index, material in ipairs(self.materials) do
            local column, row = (index - 1) % columns, (index - 1) // columns
            local inRow = math.min(columns, #LoadTime.shaders - row * columns)
            local left = margin + (area.width - margin * 2 - inRow * width - (inRow - 1) * gap) / 2 + column * (width + gap)
            local top = margin + row * (size + 56)
            local x = left + width / 2
            graphics2d.drawShape({left, top, width, size}, {radius = 18, color = '#FF1E2138', borderWidth = 2, borderColor = Test.line})
            graphics2d.draw(self.texture, x, top + size / 2, {width = size * 0.82, height = size * 0.82, material = material, blend = LoadTime.blends[LoadTime.shaders[index][1]], layer = 1})
            graphics2d.drawText(self.font, LoadTime.shaders[index][2], x, top + size + 10, {size = 24, anchor = {0.5, 0}, outlineWidth = 3, outlineColor = '#FF15172A', maxWidth = width, wrap = false, minScale = 0.7, layer = 2})
        end
    end
    self.results:draw(area, margin + rows * (size + 56) + 8)
end

return LoadTime
