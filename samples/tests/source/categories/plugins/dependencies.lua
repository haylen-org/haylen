-- The plugin `native-demo` names the plugin `native-base` in the `dependencies` of its `plugin.json`, so the tooling loads `native-base` first and the native part of `native-demo` calls the helper of `native-base` directly: Swift calls `NativeBase` of the one app target on Apple platforms, Kotlin calls `NativeBase` of the module that the Android backend adds to the module of `native-demo`, and JavaScript imports `describe` from `haylen-plugin:native-base` on the web. The answer carries the text that the helper wrote, next to the answer of the method of `native-base` itself.
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local base = require('native-base')
local demo = require('native-demo')

local Dependencies = haylen.class('Dependencies', DemoTest)

Dependencies.code = [[
-- The "plugin.json" of native-demo names native-base in its "dependencies".
local answer, err = demo.dependency('Hello from native-demo'):await()
print(answer.text, answer.language)
local own = base.describe('Hello from Lua'):await()
print(own.text)]]

Dependencies.demoText = 'Hello from native-demo'
Dependencies.baseText = 'Hello from Lua'
Dependencies.dependencyName = 'The native part of "native-demo" reaches "native-base"'
Dependencies.baseName = 'The native part of "native-base" answers the app'

-- The desktop player runs the C++ library of `native-demo`, and no native part of `native-base`.
Dependencies.desktop = 'The desktop player runs the C++ library of "native-demo", a program of its own that reaches another plugin only through Lua, and no native part of "native-base".'

function Dependencies:enter()
    self:frame{
        hint = 'Call the native part of "native-demo" again to get the text that "native-base" wrote.',
        focus = 'call',
        controls = {
            ui.button{id = 'call', text = 'Call through the dependency', variant = 'primary', onClick = function() self:run() end},
            ui.label{text = 'Swift reaches the helper in the one app target that compiles the Apple sources of every plugin, Kotlin through the module of "native-base" that the Android backend adds to the module of "native-demo", and JavaScript through the module that the page maps to "haylen-plugin:native-base".', color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.card{grow = 1, align = 'stretch', gap = 12, justify = 'center',
                ui.icon{image = 'plugins/link.svg', size = 64, color = 'textMuted', align = 'center'},
                ui.label{text = 'What "native-base" wrote for "native-demo"', font = 'heading', textAlign = 'center', align = 'stretch'},
                ui.label{id = 'demoText', text = 'Waiting for the native part.', textAlign = 'center', align = 'stretch'},
            },
            ui.card{align = 'stretch', gap = 12,
                ui.label{text = 'What "native-base" answered the app', font = 'heading'},
                ui.label{id = 'baseText', text = 'Waiting for the native part.', color = 'textMuted'},
            },
        },
    }
    self:run()
end

function Dependencies:run()
    self:act(function()
        self.results:clear()
        self:callDemo()
        self:callBase()
    end)
end

function Dependencies:callDemo()
    local name = Dependencies.dependencyName
    self.results:set('dependency', 'waiting', name, 'Waiting for the answer of "native-demo".')
    local answer, err = demo.dependency(Dependencies.demoText):await()
    if err and err.code == 'noHandler' then
        self:set('demoText', {text = Dependencies.desktop})
        self.results:set('dependency', 'skip', name, Dependencies.desktop)
        return
    elseif err then
        self.results:failure('dependency', name, err)
        return
    end
    self:set('demoText', {text = answer.text})
    local wrote = answer.text:find(Dependencies.demoText .. ', as the native part of native-base wrote it on ', 1, true) == 1
    self.results:set('dependency', wrote and 'pass' or 'fail', name, string.format('%s called the helper of "native-base", which wrote "%s".', answer.language, answer.text))
end

function Dependencies:callBase()
    local name = Dependencies.baseName
    if not base.available() then
        self:set('baseText', {text = Dependencies.desktop})
        self.results:set('base', 'skip', name, Dependencies.desktop)
        return
    end
    self.results:set('base', 'waiting', name, 'Waiting for the answer of "native-base".')
    local answer, err = base.describe(Dependencies.baseText):await()
    if err then
        self.results:failure('base', name, err)
        return
    end
    self:set('baseText', {text = answer.text})
    self.results:set('base', 'pass', name, string.format('%s answered "%s".', answer.language, answer.text))
end

return Dependencies
