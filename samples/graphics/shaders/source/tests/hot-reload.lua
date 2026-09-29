-- Hot reload: while the desktop player runs a package with --dev, make.py recompiles every shader source that changes and the player reloads the .shader file in place.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local art = require('art')
local sample = require('sample')

local HotReload = haylen.class('HotReload', sample.Test)

HotReload.hints = 'Keep this test open in a desktop dev run and edit content/shaders/live.glsl: the stripes change as soon as the file is saved.'

HotReload.steps = {
    '1. Run the sample on the desktop with python3 make.py run samples/graphics/shaders.',
    '2. Open samples/graphics/shaders/content/shaders/live.glsl and change the colors or the stripe count.',
    '3. Save: make.py compiles live.shader again and the player reloads it without a restart.',
    '4. The material keeps its values, so the time uniform goes on where it was.',
    '5. A source with an error prints the error and leaves the last good shader running.',
    'Builds for iOS, Android and the web ship the compiled .shader files and never reload them.',
}

function HotReload:init(entry)
    HotReload.super.init(self, entry)
    self.hero = art.hero()
    self.planet = art.planet()
    self.material = art.material('live')
end

function HotReload:update(dt)
    self.material:set('time', haylen.elapsed())
    local uniforms = {}
    for _, uniform in ipairs(self.material.shader.uniforms) do
        uniforms[#uniforms + 1] = uniform.name .. ' (' .. uniform.type .. ')'
    end
    self:setStatus(string.format('shader %s with %s, running on %s', self.material.shader.name, table.concat(uniforms, ', '), haylen.platform))
end

function HotReload:render()
    graphics2d.beginScreen()
    graphics2d.draw(graphics.whiteTexture(), 300, 460, {width = 360, height = 360, material = self.material})
    graphics2d.draw(self.planet, 720, 460, {width = 360, height = 360, material = self.material})
    graphics2d.draw(self.hero, 1140, 460, {width = 360, height = 360, material = self.material})
    for index, line in ipairs(HotReload.steps) do
        graphics2d.drawText(nil, line, 120, 700 + index * 44, {size = 30, outlineWidth = 2})
    end
end

return HotReload
