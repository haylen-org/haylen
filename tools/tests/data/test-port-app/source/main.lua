-- The app that proves an external port: it checks that it plays on the platform, the operating system and the graphics API of the test port, that it reads a file of its content and that the part of its plugin, which the external backend linked into it, answers through the plugin. It draws a few frames through the device of the port and quits with the status 0 when every check passed, or 1 otherwise.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local greeter = require('test-port-greeter')
local haylen = require('haylen')
local log = require('haylen.log')
local scene = require('haylen.scene')
local system = require('haylen.system')

local failures = 0
local checks = {
    {'platform', haylen.platform, 'test-port'},
    {'operating system', system.info().os, 'port'},
    {'graphics API', haylen.graphicsApi, 'port'},
    {'greeting of the content', assets.json('greeting.json').greeting, 'Hello from the content of the app.'},
    {'greeting of the plugin', greeter.greeting(), 'Hello from the part of the plugin.'},
}
for _, check in ipairs(checks) do
    if check[2] == check[3] then
        log.info(string.format('The %s is "%s".', check[1], check[3]))
    else
        log.error(string.format('The %s is "%s" instead of "%s".', check[1], tostring(check[2]), check[3]))
        failures = failures + 1
    end
end

local camera = graphics2d.newCamera()
local frames = 0

scene.push({
    update = function(self, dt)
        frames = frames + 1
        if frames < 10 then
            return
        end
        if failures == 0 then
            log.info(string.format('The app of the test port passed every check and drew %d frames through "%s".', frames, graphics.apiName()))
        end
        haylen.quit({status = failures == 0 and 0 or 1})
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
