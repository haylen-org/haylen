-- Hot reload: while `haylen app run` plays the app in development on any platform, its development server compiles every shader source that changes again and sends the `.shader` file, which the app reloads in place.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local hotReload = require('haylen.hotReload')

local ShaderTest = require('categories.shaders.shader-test')
local Test = require('harness.test')

local HotReload = haylen.class('HotReload', ShaderTest)

HotReload.code = [[
local material = graphics2d.newMaterial(assets.shader('shaders/live.shader'))
material:set('time', haylen.elapsed())  -- The values stay when the shader reloads.
graphics2d.draw(knight, x, y, {width = 300, height = 300, material = material})
-- In development, saving `live.glsl` compiles and reloads `live.shader` in place.]]

HotReload.steps = {
    '1. Run the test project with "haylen app run samples/tests".',
    '2. Open "samples/tests/content/shaders/live.glsl" and change the colors or the stripe count.',
    '3. Save: the development server compiles "live.shader" again and the app reloads it without a restart.',
    '4. The material keeps its values, so the "time" uniform goes on where it was.',
    '5. A source with an error prints the error and leaves the last good shader running.',
    'Release builds ship the compiled ".shader" files and never reload them.',
}

function HotReload:init(entry)
    HotReload.super.init(self, entry)
    self.cellGrid = {3, 3, 420}
    self.material = ShaderTest.material('live')
    local uniforms = {}
    for _, uniform in ipairs(self.material.shader.uniforms) do
        uniforms[#uniforms + 1] = string.format('"%s" (%s)', uniform.name, uniform.type)
    end
    self.uniforms = table.concat(uniforms, ', ')
end

function HotReload:enter()
    self:frame{code = HotReload.code, hint = 'Keep this test open in a development run and edit "content/shaders/live.glsl": the stripes change as soon as the file is saved.'}
end

function HotReload:update(dt)
    HotReload.super.update(self, dt)
    self.material:set('time', haylen.elapsed())
    self:status(string.format('The shader "%s" has the uniforms %s, on the platform "%s".', self.material.shader.name, self.uniforms, haylen.platform))
end

-- Draws the live material on three cards over the steps, which line up with the cards, and while the app does not run in development, a badge over the steps that says that hot reload is off.
function HotReload:draw(area)
    local cells = self.cells
    local idle = math.floor(haylen.elapsed() * 6) % 4
    self:drawCard(cells[1], 'Planet', true)
    self:drawPicture(cells[1], 'planet', cells[1].size, {material = self.material})
    self:drawCard(cells[2], 'Knight')
    self:drawPicture(cells[2], 'knight', cells[2].size * 1.1, {material = self.material})
    self:drawCard(cells[3], 'Hero')
    self:drawPicture(cells[3], 'walker', cells[3].size * 1.1, {material = self.material, source = {idle * 256, 0, 256, 256}})

    local left, top = cells[1].rect[1], cells[1].rect[2] + cells[1].rect[4] + 28
    if not hotReload.active() then
        local text = 'Hot reload is off, because the project does not run in development.'
        local width = graphics2d.measureText(nil, text, {size = 26}) + 48
        graphics2d.drawShape({left, top, width, 52}, {radius = 26, color = '#FFC8372F', borderWidth = 3, borderColor = ShaderTest.ink, layer = 6})
        graphics2d.drawText(nil, text, left + width / 2, top + 26, {size = 26, anchor = {0.5, 0.5}, color = '#FFFFFFFF', layer = 7})
        top = top + 68
    end
    for index, line in ipairs(HotReload.steps) do
        Test.caption(line, left, top + (index - 1) * 42, {size = 26, color = index == #HotReload.steps and Test.muted or Test.ink, maxWidth = self.layoutWidth - 48})
    end
end

return HotReload
