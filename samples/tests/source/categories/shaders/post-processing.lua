-- A post-processing chain over a scene of the island: the built-in vignette runs in the composite, then every material of `postProcess.materials` runs over the image the step before made.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Scenery = require('categories.effects.scenery')
local ShaderTest = require('categories.shaders.shader-test')

local PostProcessing = haylen.class('PostProcessing', ShaderTest)

PostProcessing.code = [[
local grading = graphics2d.newMaterial(assets.shader('shaders/grading.shader'), {strength = 0.8})
grading:set('shadows', '#FF2A6A80')
grading:set('highlights', '#FFFFB070')
local crt = graphics2d.newMaterial(assets.shader('shaders/crt.shader'), {curvature = 0.08, scanlines = 0.25})
local chain = {vignetteStrength = 0.7, materials = {blurX, blurY, grading, crt}}
graphics2d.beginWorld(camera, {clear = '#FF203048', postProcess = chain})]]

PostProcessing.view = {1920, 1080}
PostProcessing.passes = {{'vignette', 'Built-in vignette'}, {'blur', 'Blur, two passes'}, {'grading', 'Color grading'}, {'crt', 'Tube screen'}}

function PostProcessing:init(entry)
    PostProcessing.super.init(self, entry)
    self.scenery = Scenery()
    self.walker = assets.texture('sprites/sheets/walker.png', {filter = 'linear'})
    self.coin = assets.texture('physics/coin.png', {filter = 'linear'})
    self.enabled = {vignette = true, blur = false, grading = true, crt = true}
    self.radius = 1.5
    self.blurX = ShaderTest.material('blur')
    self.blurY = ShaderTest.material('blur')
    self.grading = ShaderTest.material('grading', {shadows = '#FF2A6A80', highlights = '#FFFFB070', strength = 0.8, contrast = 1.15})
    self.crt = ShaderTest.material('crt', {curvature = 0.08, scanlines = 0.25, split = 0.0015})
end

function PostProcessing:enter()
    local controls = {}
    for _, pass in ipairs(PostProcessing.passes) do
        local key = pass[1]
        controls[#controls + 1] = ui.toggle{id = key, text = pass[2], checked = self.enabled[key], onChange = function(event)
            self.enabled[key] = event.checked
        end}
        if key == 'blur' then
            controls[#controls + 1] = ui.formField{label = 'Blur radius in pixels', ui.slider{id = 'radius', min = 0, max = 4, value = self.radius, showValue = true, onChange = function(event)
                self.radius = event.value
            end}}
        end
    end
    self:frame{
        code = PostProcessing.code,
        hint = 'Turn the passes on and off. The chain runs in the order listed: blur, color grading, then the tube screen.',
        view = PostProcessing.view,
        controls = controls,
        focus = 'vignette',
    }
end

-- Returns the materials of the passes that are on, in chain order, and their names. The blur steps one pixel of the play area on the screen.
function PostProcessing:chain()
    local materials, names = {}, {}
    if self.enabled.blur then
        local scale = viewport.pixelRect().width / viewport.visibleRect().width
        self.blurX:set('direction', {1 / (self.stage.width * scale), 0})
        self.blurY:set('direction', {0, 1 / (self.stage.height * scale)})
        self.blurX:set('radius', self.radius)
        self.blurY:set('radius', self.radius)
        table.move({self.blurX, self.blurY}, 1, 2, #materials + 1, materials)
        table.move({'horizontal blur', 'vertical blur'}, 1, 2, #names + 1, names)
    end
    if self.enabled.grading then
        materials[#materials + 1] = self.grading
        names[#names + 1] = 'color grading'
    end
    if self.enabled.crt then
        self.crt:set('time', haylen.elapsed())
        materials[#materials + 1] = self.crt
        names[#names + 1] = 'tube screen'
    end
    return materials, names
end

function PostProcessing:update(dt)
    PostProcessing.super.update(self, dt)
    if not self.stage then
        return
    end
    local _, names = self:chain()
    self.scenery:update(dt)
    local vignette = self.enabled.vignette and 'The vignette' or 'No vignette'
    if #names == 0 then
        self:status(vignette .. ', and no materials.')
        return
    end
    local last = table.remove(names)
    self:status(vignette .. ', then the ' .. (#names > 0 and table.concat(names, ', the ') .. ' and the ' or '') .. last .. '.')
end

function PostProcessing:render()
    if not self.area then
        return
    end
    local materials = self:chain()
    graphics2d.beginWorld(self.camera, {clear = '#FF203048', postProcess = {vignetteStrength = self.enabled.vignette and 0.7 or 0, materials = materials}})
    local time = haylen.elapsed()
    self.scenery:draw(false)
    -- Three walkers run in place on the grass while coins spin over them, so every pass has edges, colors and motion to work on.
    for index = 0, 2 do
        local frame = 8 + math.floor(time * 14 + index * 3) % 8
        local x = -520 + index * 520
        graphics2d.drawShape({x - 80, 390, 160, 26}, {kind = 'ellipse', color = '#40203018', layer = 1})
        graphics2d.draw(self.walker, x, 406, {source = {frame % 8 * 256, frame // 8 * 256, 256, 256}, width = 280, height = 280, pivotY = 248 / 256, layer = 2})
        for coin = 0, 2 do
            local spin = math.abs(math.cos(time * 2.5 + coin + index))
            local y = 30 - coin * 110 + math.sin(time * 3 + coin) * 10
            graphics2d.draw(self.coin, x + 160, y, {width = 76 * math.max(0.15, spin), height = 76, layer = 2})
        end
    end
end

return PostProcessing
