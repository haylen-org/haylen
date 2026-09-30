-- Opened URLs: links with the scheme of the plugin open the app, and the native part sends each one as urlOpened, retained, so the link that launched the app and the links that arrived while this test was closed wait for its listener.
local haylen = require('haylen')
local ui = require('haylen.ui')

local demo = require('native-demo')
local sample = require('sample')

local Urls = haylen.class('Urls', sample.Test)

-- The URLs outlive the scene, because a retained event reaches only the listener that connects first.
local received = {}

function Urls:enter()
    local scheme = demo.config().urlScheme
    self:frame({
        hint = 'Open a link with the scheme of the plugin while the app runs, or to launch it.',
        focus = 'back',
        controls = {
            ui.label{text = string.format('The plugin declares %s:// in CFBundleURLTypes on Apple platforms and in an intent filter of the activity on Android. The web stands in with the hash of the page address.', scheme), color = 'textMuted', font = 'caption'},
            ui.label{font = 'monospace', text = string.format('xcrun simctl openurl booted %s://hello\nopen %s://hello\nadb shell am start -a android.intent.action.VIEW -d %s://hello\nhttp://localhost:8000/#hello', scheme, scheme, scheme)},
        },
    })
    if not self.native then
        return
    end

    self.connection = demo.onUrlOpened(function(payload)
        received[#received + 1] = {url = payload.url, frame = haylen.frameIndex()}
        self:show()
    end)
    self:show()
end

function Urls:exit()
    if self.connection then
        self.connection:disconnect()
    end
end

function Urls:show()
    if #received == 0 then
        self.results:set('opened', 'waiting', 'a link opens the app', 'No link has opened the app yet.')
        return
    end
    local opened = #received == 1 and 'A link opened the app' or string.format('%d links opened the app', #received)
    self.results:set('opened', 'pass', 'a link opens the app', opened .. ', which the native part sent as urlOpened.')
    for index, entry in ipairs(received) do
        self.results:set(index, 'info', 'urlOpened ' .. index, string.format('%s arrived at frame %d.', entry.url, entry.frame))
    end
end

return Urls
