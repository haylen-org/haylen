-- The parameters of the plugin, which `app.json` gives in its `plugins` section over the defaults of `plugin.json`. The file `app.json` leaves `tickInterval` out on purpose, so its default shows, and the native part receives the same values from its platform.
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Configuration = haylen.class('Configuration', DemoTest)

Configuration.parameters = {'greeting', 'bannerColor', 'tickInterval', 'urlScheme'}
Configuration.leftOut = 'tickInterval'
Configuration.default = 1

function Configuration:enter()
    local config = demo.config()
    local given = haylen.config.plugins[demo.id] or {}
    local cards = {}
    for index, name in ipairs(Configuration.parameters) do
        cards[index] = ui.card{align = 'stretch', padding = {14, 20},
            ui.row{gap = 16,
                ui.column{grow = 1, gap = 4,
                    ui.label{text = name, font = 'monospace', color = 'textMuted'},
                    ui.label{text = tostring(config[name]), font = 'heading'},
                },
                ui.column{gap = 8, alignItems = 'end',
                    ui.badge{text = given[name] ~= nil and 'From "app.json"' or 'Default of "plugin.json"', tone = given[name] ~= nil and 'accent' or 'information'},
                    ui.badge{id = 'native-' .. name, text = demo.available() and 'Asking the native part' or 'No native part here', tone = 'neutral'},
                },
            },
        }
    end
    self:frame{
        hint = 'Compare the values with "app.json" and "plugins/native-demo/plugin.json", and ask the native part for them again.',
        focus = 'ask',
        controls = {
            ui.button{id = 'ask', text = 'Ask the native part again', variant = 'primary', enabled = demo.available(), onClick = function()
                self:act(function() self:compareNative(config) end)
            end},
            ui.label{text = 'The file "app.json" gives "greeting", "bannerColor" and "urlScheme" and leaves "tickInterval" out, which then takes the default of "plugin.json". The tool "haylen.py" checks the values against the types of "plugin.json" before it builds, and the platforms hand the same values to the native part.', color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.panel{grow = 1, align = 'stretch', gap = 16,
                ui.sectionTitle{text = 'The parameters of "native-demo"'},
                ui.scroll{grow = 1, height = 0, ui.column{gap = 12, children = cards}},
            },
        },
        code = 'local config = demo.config()\nprint(config.greeting, config.tickInterval)\nlocal received = demo.nativeConfig():await()\nprint(received.bannerColor)',
    }

    local defaulted = given[Configuration.leftOut] == nil and config[Configuration.leftOut] == Configuration.default
    self.results:set('default', defaulted and 'pass' or 'fail', 'A parameter that "app.json" leaves out takes its default', string.format('The file "app.json" gives no "%s", and the plugin reads %s, the default of "plugin.json".', Configuration.leftOut, DemoTest.json(config[Configuration.leftOut])))
    self:act(function() self:compareNative(config) end)
end

function Configuration:compareNative(config)
    local name = 'The native part receives the same values'
    self.results:set('native', 'waiting', name, 'Asking the native part.')
    local received, err = demo.nativeConfig():await()
    if err then
        self.results:failure('native', name, err)
        return
    end
    local differs = nil
    for _, parameter in ipairs(Configuration.parameters) do
        local same = received[parameter] == config[parameter]
        self:set('native-' .. parameter, {text = same and 'The same in the native part' or 'Another value in the native part', tone = same and 'success' or 'danger'})
        if not same and not differs then
            differs = string.format('The native part received %s for "%s" instead of %s.', DemoTest.json(received[parameter]), parameter, DemoTest.json(config[parameter]))
        end
    end
    self.results:set('native', differs and 'fail' or 'pass', name, differs or string.format('The native part read the %d parameters with the same values from its platform.', #Configuration.parameters))
end

return Configuration
