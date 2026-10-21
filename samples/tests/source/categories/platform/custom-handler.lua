-- The method `platform-sample.echo` of the plugin `platform-sample`, which native code answers on every platform that builds it: the test checks the echo, the count of characters and the language that answered, and that an empty text fails. The desktop player runs no native code, where a Lua stand-in shows how `platform.registerHandler` answers instead.
local haylen = require('haylen')
local json = require('json')
local platform = require('haylen.platform')
local ui = require('haylen.ui')

local SampleTest = require('categories.platform.sample-test')
local platformSample = require('platform-sample')

local CustomHandler = haylen.class('CustomHandler', SampleTest)

CustomHandler.sources = {
    {'Android', 'android/src/main/java/dev/haylen/samples/platform/PlatformSamplePlugin.java', 'Registered in "onLoad", which the meta-data of the manifest of the module names.'},
    {'Apple', 'apple/PlatformSamplePlugin.m', 'Registered in "loadWithContext:", which the runtime calls for the class of "plugin.json".'},
    {'Web', 'web/platform-sample.js', 'Registered in "load", which the loader calls before the runtime starts.'},
}

function CustomHandler:enter()
    self.text = 'Hello from Lua, olá'
    local sources = {ui.label{text = 'Where each platform answers it', font = 'heading'}}
    for _, source in ipairs(CustomHandler.sources) do
        sources[#sources + 1] = ui.column{gap = 4,
            ui.row{gap = 12, ui.badge{text = source[1], tone = 'accent', width = 130}, ui.label{text = source[2], font = 'monospace', color = 'accentText', grow = 1}},
            ui.label{text = source[3], font = 'caption', color = 'textMuted'},
        }
    end
    sources[#sources + 1] = ui.label{text = 'The paths are inside the folder "plugins/platform-sample" of the test project.', font = 'caption', color = 'textMuted'}
    self:frame{
        hint = 'Send any text to "platform-sample.echo" again and see which language answered.',
        focus = 'call',
        checksTitle = 'The answers of the handler',
        controls = {
            ui.formField{label = 'Text to send', ui.textField{id = 'text', value = self.text, maxLength = 60, onChange = function(event) self.text = event.value end}},
            ui.button{id = 'call', text = 'Send the text', variant = 'primary', onClick = function() self:run() end},
            ui.button{id = 'standIn', text = 'Answer with a Lua stand-in', enabled = false, onClick = function() self:registerStandIn() end},
            ui.label{text = 'A Lua handler takes precedence over native ones on every platform and stays for the rest of the session, so the button only turns on where no native code answers.', color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.panel{grow = 1, align = 'stretch', gap = 20, children = sources},
        },
        code = "local answer, err = platformSample.echo('Hi'):await()\nprint(answer.echo, answer.characters, answer.language)\nlocal _, failure = platformSample.echo(''):await()\nprint(failure.code, failure.message)  -- A text is required.\nplatform.registerHandler('platform-sample.echo', function(params) return {echo = params.text} end)",
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
    self.results:set('echo', same and 'pass' or 'fail', name, string.format('%s sent back "%s" and counted %s characters, where %s was expected to answer.', tostring(result.language), tostring(result.echo), tostring(result.characters), expected))
end

function CustomHandler:checkEmpty()
    local name = 'An empty text fails'
    local result, err = platformSample.echo(''):await()
    if not err then
        self.results:set('empty', 'fail', name, 'The method answered ' .. json.encode(result) .. ' instead of failing.')
        return
    end
    local named = err.message:find('needs a text', 1, true) ~= nil
    self.results:set('empty', named and 'pass' or 'fail', name, string.format('It failed with the code "%s", saying: %s', tostring(err.code), err.message))
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

return CustomHandler
