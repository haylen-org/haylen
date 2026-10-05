-- A post-processing chain: the built-in vignette runs in the composite, then every material of `postProcess.materials` runs over the image the step before made.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local ShaderTest = require('categories.shaders.shader-test')

local PostProcessing = haylen.class('PostProcessing', ShaderTest)

PostProcessing.view = {1920, 1080}
PostProcessing.passes = {{'vignette', 'Built-in vignette'}, {'blur', 'Blur, two passes'}, {'grading', 'Color grading'}, {'crt', 'Tube screen'}}

function PostProcessing:init(entry)
    PostProcessing.super.init(self, entry)
    self.hero = ShaderTest.hero()
    self.planet = ShaderTest.planet()
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
        names[#names + 1] = 'grading'
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
    local vignette = self.enabled.vignette and 'Vignette' or 'No vignette'
    self:status(vignette .. (#names > 0 and ', then ' .. table.concat(names, ', ') or ', no materials'))
end

function PostProcessing:render()
    if not self.area then
        return
    end
    local materials = self:chain()
    graphics2d.beginWorld(self.camera, {clear = '#FF203048', postProcess = {vignetteStrength = self.enabled.vignette and 0.7 or 0, materials = materials}})
    local time = haylen.elapsed()
    for index = 0, 11 do
        graphics2d.drawRect({-1400 + index * 240, -900, 120, 1800}, index % 2 == 0 and '#FF2A3E5C' or '#FF33486A')
    end
    for index = 0, 4 do
        local x = -700 + index * 350 + math.sin(time * 0.5 + index) * 60
        graphics2d.draw(self.planet, x, -80 + math.cos(time * 0.7 + index) * 120, {width = 220, height = 220, layer = 1})
    end
    for index = 0, 5 do
        local x = -760 + index * 300
        local bounce = math.abs(math.sin(time * 3 + index)) * 120
        graphics2d.draw(self.hero, x, 300 - bounce, {width = 128, height = 128, layer = 2})
    end
    graphics2d.drawText(nil, 'POST', 0, -330, {size = 160, anchor = {0.5, 0.5}, color = '#FFFFE070', outlineWidth = 6, outlineColor = '#FF402000', layer = 3})
end

return PostProcessing
