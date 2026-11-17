-- The app of the test port: it checks that the platform, the operating system and the graphics API are the ones of the port, that the port refuses to start programs and that Varn takes its external driver of processes, draws a few frames of shapes, sprites and text through the device of the port, which runs the code that the shader tool of the port wrote, and quits with the status 0 when every check passed.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local log = require('haylen.log')
local scene = require('haylen.scene')
local process = require('process')
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

-- The process module of Varn runs on the external driver of the test port, which names its own folder.
if process.cwd() ~= '/test-port' then
    log.error(string.format('The process module of the test port runs in "%s" instead of the external driver of the port.', tostring(process.cwd())))
    failures = failures + 1
end

-- The test port does not allow `processes`, so the functions that start other programs say so.
local started, reason = pcall(io.popen, 'echo')
if started or not tostring(reason):find('This platform starts no other program, so "io.popen" is not available.', 1, true) then
    log.error(string.format('The test port started a program through "io.popen" or failed with "%s".', tostring(reason)))
    failures = failures + 1
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
