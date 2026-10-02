-- Plugin info: platform.plugins() lists the plugins of app.json with their version and whether their native part runs on this platform, which the headless harness of the engine and any platform without the native part report as false.
local haylen = require('haylen')
local platform = require('haylen.platform')
local ui = require('haylen.ui')

local demo = require('native-demo')
local sample = require('sample')

local Info = haylen.class('Info', sample.Test)

function Info:enter()
    self:frame({
        hint = 'Every other test needs the native part that this one reports.',
        focus = 'back',
        controls = {
            ui.label{text = 'The platform reports the plugins whose native part it loaded: the Apple runtime the classes that the "plugin.json" files of the bundled package name, the Android library the classes of the manifest, the web page the modules of "config.json", and the desktops the libraries that declared themselves with "registerPlugin".', color = 'textMuted', font = 'caption'},
            ui.label{font = 'monospace', text = "for _, plugin in ipairs(platform.plugins()) do\n  print(plugin.id, plugin.version, plugin.native)\nend"},
        },
    })

    for index, plugin in ipairs(platform.plugins()) do
        self.results:set(index, 'info', 'Plugin "' .. plugin.id .. '"', string.format('Version %s, native %s on %s', plugin.version, tostring(plugin.native), haylen.platform))
    end
    if self.native then
        self.results:set('native', 'pass', 'The native part of "' .. demo.id .. '" runs', string.format('The field "platform.plugin(\'%s\').native" is "true" on %s.', demo.id, haylen.platform))
    end
end

return Info
