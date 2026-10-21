-- The function `platform.plugins()` lists the plugins of `app.json` with their version and whether their native part runs on this platform, which the headless host and any platform without the native part report as false.
local haylen = require('haylen')
local platform = require('haylen.platform')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Info = haylen.class('Info', DemoTest)

function Info:enter()
    local rows = {}
    for index, plugin in ipairs(platform.plugins()) do
        rows[index] = {id = plugin.id, cells = {plugin.id, plugin.version, plugin.native and 'Runs here' or 'Not here'}}
    end
    self:frame{
        hint = 'Every other test of the category needs the native part that this one reports.',
        controls = {
            ui.label{text = 'The platform reports the plugins whose native part it loaded: the Apple runtime the classes that the "plugin.json" files of the bundled package name, the Android library the classes of the manifest, the web page the modules of "config.json", and the desktops the libraries that declared themselves with "registerPlugin".', color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.card{grow = 1, align = 'stretch', gap = 12,
                ui.label{text = 'The plugins of "app.json"', font = 'heading'},
                ui.table{id = 'plugins', columns = {{text = 'Plugin'}, {text = 'Version', width = 180}, {text = 'Native part', width = 220}}, rows = rows},
            },
        },
        code = "for _, plugin in ipairs(platform.plugins()) do\n  print(plugin.id, plugin.version, plugin.native)\nend",
    }
    if self.native then
        self.results:set('native', 'pass', 'The native part of "' .. demo.id .. '" runs', string.format('The field "native" of "platform.plugin(\'%s\')" is "true".', demo.id))
    end
end

return Info
