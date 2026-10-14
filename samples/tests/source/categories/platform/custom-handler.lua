-- The method `platform-sample.echo` of the plugin `platform-sample`, which native code answers on every platform that builds it: the test checks the echo, the count of characters and the language that answered, and that an empty text fails. The desktop player runs no native code, where a Lua stand-in shows how `platform.registerHandler` answers instead.
local haylen = require('haylen')
local json = require('json')
local platform = require('haylen.platform')
local ui = require('haylen.ui')

local SampleTest = require('categories.platform.sample-test')
local Test = require('harness.test')
local platformSample = require('platform-sample')

local CustomHandler = haylen.class('CustomHandler', SampleTest)

CustomHandler.sources = {
    {'Android', '"plugins/platform-sample/android/src/main/java/dev/haylen/samples/platform/PlatformSamplePlugin.java"', 'Registered in "onLoad", which the meta-data of the manifest of the module names.'},
    {'Apple', '"plugins/platform-sample/apple/PlatformSamplePlugin.m"', 'Registered in "loadWithContext:", which the runtime calls for the class of "plugin.json".'},
    {'Web', '"plugins/platform-sample/web/platform-sample.js"', 'Registered in "load", which the loader calls before the runtime starts.'},
}

function CustomHandler:enter()
    self.text = 'Hello from Lua, olá'
    self:frame{
        hint = 'Call "platform-sample.echo" again with any text and see which language answered.',
        focus = 'call',
        controls = {
            ui.formField{label = 'Text to send', ui.textField{id = 'text', value = self.text, maxLength = 60, onChange = function(event) self.text = event.value end}},
            ui.button{id = 'call', text = 'Call "platform-sample.echo"', variant = 'primary', onClick = function() self:run() end},
            ui.button{id = 'standIn', text = 'Answer with a Lua stand-in', enabled = false, onClick = function() self:registerStandIn() end},
            ui.label{text = 'A Lua handler takes precedence over native ones on every platform and stays for the rest of the session, so the button only turns on where no native code answers.', color = 'textMuted', font = 'caption'},
        },
        code = "local answer, err = platformSample.echo('Hi'):await()",
    }
    self:run()
end

function CustomHandler:run()
    self.results:clear()
    local native = self:checkNative()
    self:set('standIn', {enabled = not native and not platform.hasHandler('platform-sample.echo')})
    if not native and not platform.hasHandler('platform-sample.echo') then
        return
    end
    self:spawn(function()
        self:checkEcho()
        self:checkEmpty()
    end)
end

function CustomHandler:checkEcho()
    local name = 'The method "platform-sample.echo" answers'
    self.results:set('echo', 'waiting', name, 'Waiting for the answer.')
    local result, err = platformSample.echo(self.text):await()
    if err then
        self.results:set('echo', 'fail', name, string.format('%s (code "%s")', err.message, tostring(err.code)))
        return
    end
    local expected = platform.hasHandler('platform-sample.echo') and 'Lua stand-in' or SampleTest.languages[haylen.platform]
    local same = result.echo == self.text and result.characters == utf8.len(self.text) and result.language == expected
    self.results:set('echo', same and 'pass' or 'fail', name, string.format('%s answered %s, and %s was expected.', tostring(result.language), json.encode(result), expected))
end

function CustomHandler:checkEmpty()
    local name = 'An empty text fails'
    local result, err = platformSample.echo(''):await()
    if not err then
        self.results:set('empty', 'fail', name, 'The method answered ' .. json.encode(result) .. ' instead of failing.')
        return
    end
    local named = err.message:find('needs a text', 1, true) ~= nil
    self.results:set('empty', named and 'pass' or 'fail', name, string.format('It failed with the code "%s": %s', tostring(err.code), err.message))
end

function CustomHandler:registerStandIn()
    platform.registerHandler('platform-sample.echo', function(params)
        if params.text == nil or params.text == '' then
            error('The method "platform-sample.echo" needs a text.')
        end
        return {echo = params.text, characters = utf8.len(params.text), language = 'Lua stand-in', system = haylen.platform}
    end)
    self:run()
end

function CustomHandler:draw(area)
    local top = self.results:draw(area)
    local y = math.max(top, area.height - 40 - #CustomHandler.sources * 64)
    Test.caption('Where each platform answers it', 24, y - 44, {size = 24, color = Test.ink})
    for index, source in ipairs(CustomHandler.sources) do
        local rowY = y + (index - 1) * 64
        Test.caption(source[1], 24, rowY, {size = 22, color = Test.accent})
        Test.caption(source[2], 160, rowY, {size = 20, color = Test.ink})
        Test.caption(source[3], 160, rowY + 28, {size = 18})
    end
end

return CustomHandler
