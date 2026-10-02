-- Configuration: the parameters of the plugin, which app.json gives in its plugins section over the defaults of plugin.json. app.json leaves tickInterval out on purpose, so its default shows, and the native part receives the same values from its platform.
local haylen = require('haylen')
local ui = require('haylen.ui')

local demo = require('native-demo')
local sample = require('sample')

local Configuration = haylen.class('Configuration', sample.Test)

local kParameters = {'greeting', 'bannerColor', 'tickInterval', 'urlScheme'}
local kLeftOut = 'tickInterval'
local kDefault = 1

function Configuration:enter()
    self:frame({
        hint = 'Compare the values with "app.json" and "plugins/native-demo/plugin.json".',
        focus = 'back',
        controls = {
            ui.label{text = 'The file "app.json" gives "greeting", "bannerColor" and "urlScheme" in its "plugins" section and leaves "tickInterval" out, which then takes the default of "plugin.json". The tool "make.py" checks the values against the types of "plugin.json" before it builds, and the platforms hand the same values to the native part of the plugin.', color = 'textMuted', font = 'caption'},
            ui.label{font = 'monospace', text = '"plugins": {\n  "native-demo": {\n    "greeting": "Hello from app.json",\n    "bannerColor": "#1D3557",\n    "urlScheme": "haylendemo"\n  }\n}'},
        },
    })

    local config = demo.config()
    local given = haylen.config.plugins[demo.id] or {}
    for _, name in ipairs(kParameters) do
        local source = given[name] ~= nil and '"app.json"' or 'the default of "plugin.json"'
        self.results:set(name, 'info', 'Parameter "' .. name .. '"', string.format('%s, from %s', sample.json(config[name]), source))
    end
    local defaulted = given[kLeftOut] == nil and config[kLeftOut] == kDefault
    self.results:set('default', defaulted and 'pass' or 'fail', 'A parameter that "app.json" leaves out takes its default', string.format('The file "app.json" gives no "%s", and the plugin reads %s, the default of "plugin.json".', kLeftOut, sample.json(config[kLeftOut])))
    self:act(function() self:compareNative(config) end)
end

function Configuration:compareNative(config)
    local name = 'The native part receives the same values'
    self.results:set('native', 'waiting', name, 'Asking the native part')
    local received, err = demo.nativeConfig():await()
    if err then
        self.results:failure('native', name, err)
        return
    end
    for _, parameter in ipairs(kParameters) do
        if received[parameter] ~= config[parameter] then
            self.results:set('native', 'fail', name, string.format('The native part received %s for "%s" instead of %s.', sample.json(received[parameter]), parameter, sample.json(config[parameter])))
            return
        end
    end
    self.results:set('native', 'pass', name, 'The native part received ' .. sample.json(received) .. ' from its platform.')
end

return Configuration
