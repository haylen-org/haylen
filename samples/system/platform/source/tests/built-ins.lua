-- Built-in methods: every method that works without app code, called through the bridge with its parameters, who answers it, the frames its answer took and the result or the error it came back with.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local platform = require('haylen.platform')
local ui = require('haylen.ui')

local sample = require('sample')

local BuiltIns = haylen.class('BuiltIns', sample.Test)

-- Methods that change nothing on the device are called when the test opens, while opening a page and vibrating wait for their buttons.
local kCalls = {
    {method = 'engine.info', open = true},
    {method = 'app.version', open = true},
    {method = 'device.info', open = true},
    {method = 'system.locale', open = true},
    {method = 'system.open_url', params = {url = 'https://example.com'}, text = 'Open example.com'},
    {method = 'system.open_url', params = {}, text = 'Open without a url'},
    {method = 'haptics.vibrate', params = {duration = 80}, text = 'Vibrate for 80 ms'},
}

function BuiltIns:enter()
    self.calls = {}
    local controls = {
        ui.button{id = 'again', text = 'Call the first four again', variant = 'primary', onClick = function() self:callOpening() end},
    }
    for index, entry in ipairs(kCalls) do
        self.calls[index] = {entry = entry}
        controls[#controls + 1] = ui.button{id = 'call' .. index, text = entry.text or 'Call ' .. entry.method, onClick = function() self:call(index) end}
    end
    controls[#controls + 1] = ui.label{text = 'engine.info and app.version are engine handlers, which platform.hasHandler sees. The native side of each platform answers the others: HaylenBridge on Android and Apple platforms, the page on the web and the desktop handlers on Windows and Linux.', color = 'textMuted', font = 'caption'}
    controls[#controls + 1] = ui.label{font = 'monospace', text = "local info, err = platform.call('device.info'):await()\nplatform.hasHandler('engine.info')\nplatform.pendingCalls()"}
    self:frame({hint = 'Every answer reaches Lua at the start of a later frame, never during the call.', focus = 'again', controls = controls})
    self:callOpening()
end

function BuiltIns:callOpening()
    for index, entry in ipairs(kCalls) do
        if entry.open then
            self:call(index)
        end
    end
end

-- Calls one method in a task of the scene, which waits for its promise and ends with the scene.
function BuiltIns:call(index)
    local call = self.calls[index]
    local promise, id = platform.call(call.entry.method, call.entry.params)
    call.id, call.frame, call.done, call.result, call.error = id, haylen.frame(), nil, nil, nil
    self:spawn(function()
        local result, err = promise:await()
        if call.id == id then
            call.done = haylen.frame() - call.frame
            call.result, call.error = result, err
        end
    end)
end

function BuiltIns:update(dt)
    BuiltIns.super.update(self, dt)
    self:status(string.format('platform %s   pending calls %d   engine.info handler %s   device.info handler %s', haylen.platform, platform.pendingCalls(), platform.hasHandler('engine.info'), platform.hasHandler('device.info')))
end

function BuiltIns:draw(area)
    local y = 20
    for _, call in ipairs(self.calls) do
        local entry = call.entry
        local who = platform.hasHandler(entry.method) and 'engine handler' or 'native side'
        sample.caption(entry.method .. ' ' .. sample.json(entry.params or {}), 24, y, {size = 24, color = sample.ink})
        sample.caption(who, area.width - 24, y, {anchor = {1, 0}, color = sample.accent})
        local text, color
        if not call.id then
            text, color = 'not called yet', sample.muted
        elseif not call.done then
            text, color = 'waiting for the answer', sample.warm
        elseif call.error then
            text, color = string.format('failed after %d frames: %s', call.done, call.error), sample.red
        else
            text, color = string.format('answered after %d frames: %s', call.done, sample.json(call.result)), sample.green
        end
        sample.caption(text, 24, y + 34, {size = 20, color = color, maxWidth = area.width - 48})
        local _, height = graphics2d.measureText(nil, text, {size = 20, maxWidth = area.width - 48})
        y = y + 34 + height + 26
    end
end

return BuiltIns
