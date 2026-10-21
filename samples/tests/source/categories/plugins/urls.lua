-- Links with the scheme of the plugin open the app, and the native part sends each one as `urlOpened`, retained, so the link that launched the app and the links that arrived while this test was closed wait for its listener.
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Urls = haylen.class('Urls', DemoTest)

-- The links outlive the scene, because a retained event reaches only the listener that connects first.
Urls.received = {}

-- How to open a link of the scheme on each platform, from a terminal of the computer or from the page.
Urls.ways = {
    ios = {'Run this in a terminal of the computer that runs the simulator.', 'xcrun simctl openurl booted %s://hello'},
    tvos = {'Run this in a terminal of the computer that runs the simulator.', 'xcrun simctl openurl booted %s://hello'},
    macos = {'Run this in a terminal.', 'open %s://hello'},
    android = {'Run this in a terminal of the computer that the device is connected to.', 'adb shell am start -a android.intent.action.VIEW -d %s://hello'},
    web = {'Add this to the end of the address of this page and press Enter.', '#hello'},
}

function Urls:enter()
    local scheme = demo.config().urlScheme
    local way = Urls.ways[haylen.platform] or {'This platform receives no links of the plugin.', ''}
    self:frame{
        hint = 'Open a link with the scheme of the plugin while the app runs, or to launch it.',
        checksTitle = 'Opened links',
        controls = {
            ui.label{text = string.format('The plugin declares "%s://" in "CFBundleURLTypes" on Apple platforms and in an intent filter of the link activity on Android. The web stands in with the hash of the page address.', scheme), color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.card{grow = 1, align = 'stretch', gap = 16, justify = 'center',
                ui.icon{image = 'plugins/link.svg', size = 112, color = 'accentText', align = 'center'},
                ui.label{text = 'To try it', font = 'heading', textAlign = 'center', align = 'stretch'},
                ui.label{text = way[1], color = 'textMuted', textAlign = 'center', align = 'stretch'},
                ui.label{text = string.format(way[2], scheme), font = 'monospace', color = 'accentText', textAlign = 'center', align = 'stretch'},
            },
        },
        code = "demo.onUrlOpened(function(link)\n  print(link.url)  -- Such as \"haylendemo://hello\".\nend)",
    }
    if not self.native then
        return
    end

    self.connection = demo.onUrlOpened(function(payload)
        local received = Urls.received
        received[#received + 1] = {url = payload.url, frame = haylen.frameIndex()}
        self:showLinks()
    end)
    self:showLinks()
end

function Urls:exit()
    if self.connection then
        self.connection:disconnect()
    end
    Urls.super.exit(self)
end

function Urls:showLinks()
    local received = Urls.received
    if #received == 0 then
        self.results:set('opened', 'waiting', 'A link opens the app', 'No link has opened the app yet.')
        return
    end
    local opened = #received == 1 and 'A link opened the app' or string.format('%d links opened the app', #received)
    self.results:set('opened', 'pass', 'A link opens the app', opened .. ', which the native part sent as "urlOpened".')
    for index, entry in ipairs(received) do
        self.results:set(index, 'info', string.format('Link %d', index), string.format('The link "%s" arrived at frame %d.', entry.url, entry.frame))
    end
end

return Urls
