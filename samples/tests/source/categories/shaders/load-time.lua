-- Shader load time: every shader of the category loads at once in the background and then draws, in the frame right after the last one arrives. The results show how long the files took and how long that first frame took on the backend of the platform, which the rendering guide compares: Metal compiles the shader sources on worker threads while they load, and the other backends make each program when it first draws.
local assets = require('haylen.assets')
local datetime = require('datetime')
local debugging = require('haylen.debug')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')

local Results = require('harness.results')
local Test = require('harness.test')

local LoadTime = haylen.class('LoadTime', Test)

LoadTime.names = {'blur', 'crt', 'dissolve', 'flash', 'grading', 'hologram', 'live', 'outline', 'palette', 'pixelate', 'rainbow', 'texture_map', 'wave'}
LoadTime.code = [[
local shader = assets.loadAsync('shaders/wave.shader'):await()
local material = graphics2d.newMaterial(shader, {amplitude = 0.02})
graphics2d.draw(texture, x, y, {material = material})  -- The first draw makes the GPU program.]]

function LoadTime:enter()
    self.results = Results(self.entry.code)
    self.texture = assets.texture('shaders/images/planet.png', {filter = 'linear'})
    self.materials = {}
    self.started = haylen.frameIndex()
    self.loadFrames = 0
    self:frame{code = LoadTime.code, hint = 'Every shader draws with the default values of its uniforms, so some show plain colors. Leave and open the test again to measure shaders the app already holds, whose first frame costs nothing extra.'}
    self.results:set('load', 'waiting', 'Shader files', string.format('Loading %d shaders.', #LoadTime.names))

    scene.spawn(self, function()
        local started = datetime.now():millis()
        local loads = {}
        for index, name in ipairs(LoadTime.names) do
            loads[index] = assets.loadAsync('shaders/' .. name .. '.shader')
        end
        for _, load in ipairs(loads) do
            local shader, err = load:await()
            if not shader then
                self.results:set('load', 'fail', 'Shader files', err)
                return
            end
            self.materials[#self.materials + 1] = graphics2d.newMaterial(shader)
        end
        self.loadFrames = haylen.frameIndex() - self.started
        self.results:set('load', 'info', 'Shader files', string.format('%d shaders loaded in %.0f ms over %d frames.', #self.materials, datetime.now():millis() - started, self.loadFrames))
    end)
end

-- The profiler describes the frame before, so the frame that first drew the shaders is measured one frame later.
function LoadTime:update(dt)
    LoadTime.super.update(self, dt)
    if self.drawnFrame and not self.measured and haylen.frameIndex() > self.drawnFrame then
        self.measured = true
        self.results:set('first', 'info', 'First frame with every shader', string.format('%.1f ms on "%s".', debugging.frame().milliseconds, graphics.backendName()))
    end
    self:status(string.format('Backend "%s"   Shaders %d of %d   Frame %.1f ms', graphics.backendName(), #self.materials, #LoadTime.names, debugging.frame().milliseconds))
end

function LoadTime:draw(area)
    if #self.materials == #LoadTime.names then
        self.drawnFrame = self.drawnFrame or haylen.frameIndex()
        local size = math.min(120, (area.width - 60) / #self.materials)
        for index, material in ipairs(self.materials) do
            local x = 30 + (index - 0.5) * size
            graphics2d.draw(self.texture, x, 80, {width = size * 0.8, height = size * 0.8, material = material})
            Test.caption(LoadTime.names[index], x, 80 + size * 0.5, {size = 16, anchor = {0.5, 0}})
        end
    end
    self.results:draw(area, 180)
end

return LoadTime
