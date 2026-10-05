-- The method `platform-sample.echo` of the plugin `platform-sample`, which native code answers on every platform, called with a text or without one to see its failure, with the answer, the language that produced it and, on a build without the native code of the plugin, why the method is missing and a Lua stand-in.
local haylen = require('haylen')
local json = require('json')
local platform = require('haylen.platform')
local ui = require('haylen.ui')

local Test = require('harness.test')
local platformSample = require('platform-sample')

local CustomHandler = haylen.class('CustomHandler', Test)

CustomHandler.sources = {
    {'Android', '"plugins/platform-sample/android/src/main/java/dev/haylen/samples/platform/PlatformSamplePlugin.java"', 'Registered in "onLoad", which the meta-data of the manifest of the module names.'},
    {'Apple', '"plugins/platform-sample/apple/PlatformSamplePlugin.m"', 'Registered in "loadWithContext:", which the runtime calls for the class of "plugin.json".'},
    {'Web', '"plugins/platform-sample/web/platform-sample.js"', 'Registered in "load", which the loader calls before the runtime starts.'},
}
CustomHandler.missing = 'The desktop player runs the Lua of the app and nothing else, so no native code of the plugin answers here, and the bridge fails the call. A C++ handler needs a C++ app built with "haylen_add_app" that calls "engine.getPlatform().registerHandler", which the Lua player cannot load. The function "platform.registerHandler" answers with Lua instead, and the stand-in of the panel does that for this session.'

function CustomHandler:enter()
    self.text = 'Hello from Lua'
    self:frame{
        hint = 'Call "platform-sample.echo" and see which language answered.',
        focus = 'call',
        controls = {
            ui.formField{label = 'Text to send', ui.textField{id = 'text', value = self.text, maxLength = 60, onChange = function(event) self.text = event.value end}},
            ui.button{id = 'call', text = 'Call "platform-sample.echo"', variant = 'primary', onClick = function() self:call(self.text) end},
            ui.button{id = 'empty', text = 'Call it without a text', onClick = function() self:call('') end},
            ui.button{id = 'standIn', text = 'Answer with a Lua stand-in', enabled = false, onClick = function() self:registerStandIn() end},
            ui.label{text = 'A Lua handler takes precedence over native ones on every platform and stays for the rest of the session, so the button only turns on where the method is missing.', color = 'textMuted', font = 'caption'},
        },
        code = "local answer, err = platformSample.echo('Hi'):await()",
    }
    self:call(self.text)
end

function CustomHandler:call(text)
    self.result, self.error, self.pending = nil, nil, true
    self:spawn(function()
        local result, err = platformSample.echo(text):await()
        self.result, self.error, self.pending = result, err, false
        self.missing = err ~= nil and err.code == 'noHandler'
        self:set('standIn', {enabled = self.missing and not platform.hasHandler('platform-sample.echo')})
    end)
end

function CustomHandler:registerStandIn()
    platform.registerHandler('platform-sample.echo', function(params)
        if params.text == nil or params.text == '' then
            error('The method "platform-sample.echo" needs a text.')
        end
        return {echo = params.text, characters = utf8.len(params.text), language = 'Lua stand-in', system = haylen.platform}
    end)
    self:call(self.text)
end

function CustomHandler:update(dt)
    CustomHandler.super.update(self, dt)
    self:status(string.format('Platform "%s", an engine handler for "platform-sample.echo" "%s", %d pending calls', haylen.platform, tostring(platform.hasHandler('platform-sample.echo')), platform.pendingCallCount()))
end

function CustomHandler:draw(area)
    local width = area.width - 48
    local y = 24
    if self.pending then
        Test.caption('Waiting for "platform-sample.echo"', 24, y, {size = 34, color = Test.warm})
    elseif self.error then
        Test.caption('The method "platform-sample.echo" failed', 24, y, {size = 34, color = Test.red})
        Test.caption(self.error.message, 24, y + 50, {size = 24, color = Test.ink, maxWidth = width})
        if self.missing then
            Test.caption(CustomHandler.missing, 24, y + 100, {size = 22, maxWidth = width})
        end
    elseif self.result then
        Test.caption('Answered by ' .. tostring(self.result.language), 24, y, {size = 34, color = Test.green})
        Test.caption(json.encode(self.result), 24, y + 50, {size = 22, color = Test.ink, maxWidth = width})
    end

    local top = area.height - 40 - #CustomHandler.sources * 64
    Test.caption('Where each platform answers it', 24, top - 44, {size = 24, color = Test.ink})
    for index, source in ipairs(CustomHandler.sources) do
        local rowY = top + (index - 1) * 64
        Test.caption(source[1], 24, rowY, {size = 22, color = Test.accent})
        Test.caption(source[2], 160, rowY, {size = 20, color = Test.ink})
        Test.caption(source[3], 160, rowY + 28, {size = 18})
    end
end

return CustomHandler
