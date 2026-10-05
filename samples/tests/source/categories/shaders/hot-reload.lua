-- Hot reload: while the desktop player runs a package with `--dev`, `haylen.py` compiles every shader source that changes again and the player reloads the `.shader` file in place.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local ShaderTest = require('categories.shaders.shader-test')
local Test = require('harness.test')

local HotReload = haylen.class('HotReload', ShaderTest)

HotReload.steps = {
    '1. Run the test project on the desktop with "python3 haylen.py run tests".',
    '2. Open "samples/tests/content/shaders/live.glsl" and change the colors or the stripe count.',
    '3. Save: "haylen.py" compiles "live.shader" again and the player reloads it without a restart.',
    '4. The material keeps its values, so the "time" uniform goes on where it was.',
    '5. A source with an error prints the error and leaves the last good shader running.',
    'Builds for iOS, Android and the web ship the compiled ".shader" files and never reload them.',
}

function HotReload:init(entry)
    HotReload.super.init(self, entry)
    self.hero = ShaderTest.hero()
    self.planet = ShaderTest.planet()
    self.material = ShaderTest.material('live')
    local uniforms = {}
    for _, uniform in ipairs(self.material.shader.uniforms) do
        uniforms[#uniforms + 1] = string.format('"%s" (%s)', uniform.name, uniform.type)
    end
    self.uniforms = table.concat(uniforms, ', ')
end

function HotReload:enter()
    self:frame{hint = 'Keep this test open in a desktop run and edit "content/shaders/live.glsl": the stripes change as soon as the file is saved.'}
end

function HotReload:update(dt)
    HotReload.super.update(self, dt)
    self.material:set('time', haylen.elapsed())
    self:status(string.format('Shader "%s" with %s   Platform "%s"', self.material.shader.name, self.uniforms, haylen.platform))
end

function HotReload:draw(area)
    local size, y = 300, 190
    graphics2d.draw(graphics.whiteTexture(), 260, y, {width = size, height = size, material = self.material})
    graphics2d.draw(self.planet, 670, y, {width = size, height = size, material = self.material})
    graphics2d.draw(self.hero, 1080, y, {width = size, height = size, material = self.material})
    for index, line in ipairs(HotReload.steps) do
        Test.caption(line, 40, 380 + index * 50, {size = 30, color = Test.ink})
    end
end

return HotReload
