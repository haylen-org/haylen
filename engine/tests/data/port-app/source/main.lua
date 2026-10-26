-- The app of the test port: it checks that the platform, the operating system and the graphics API are the ones of the port, draws a few frames of shapes, sprites and text through the device of the port, which runs the code that the shader tool of the port wrote, and quits with the status 0 when every check passed.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local log = require('haylen.log')
local scene = require('haylen.scene')
local system = require('haylen.system')

local failures = 0
local checks = {
    {'platform', haylen.platform, 'test-port'},
    {'operating system', system.info().os, 'port'},
    {'graphics API', haylen.graphicsApi, 'port'},
}
for _, check in ipairs(checks) do
    if check[2] ~= check[3] then
        log.error(string.format('The %s of the test port is "%s" instead of "%s".', check[1], tostring(check[2]), check[3]))
        failures = failures + 1
    end
end

local camera = graphics2d.newCamera()
local frames = 0

scene.push({
    update = function(self, dt)
        frames = frames + 1
        if frames == 10 then
            log.info(string.format('The test port drew %d frames through "%s".', frames, graphics.apiName()))
            haylen.quit({status = failures == 0 and 0 or 1})
        end
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawCircle(0, 0, 40, '#FFD040FF')
        graphics2d.draw(graphics.whiteTexture(), 60, 0, {width = 40, height = 40, color = '#4080FFFF'})
    end,
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(nil, 'Port', 40, 40, {size = 32})
    end,
})
