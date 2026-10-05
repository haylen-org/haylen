-- The share sheet and other apps, which SDKs open for their content: the native part opens the share sheet of the platform with a text and a link, and opens the settings of the app, a map app and a new message of the mail app through their links. Each platform opens what it has, and a link that no app takes answers that nothing opened.
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Sharing = haylen.class('Sharing', DemoTest)

Sharing.apps = {
    {kind = 'settings', name = 'The settings of the app open'},
    {kind = 'maps', name = 'A map app opens at a place'},
    {kind = 'mail', name = 'A new message of the mail app opens'},
}

function Sharing:enter()
    local controls = {
        ui.button{id = 'share', text = 'Share a text and a link', variant = 'primary', onClick = function() self:share() end},
    }
    for _, app in ipairs(Sharing.apps) do
        controls[#controls + 1] = ui.button{id = app.kind, text = 'Open ' .. app.kind, onClick = function() self:open(app) end}
    end
    controls[#controls + 1] = ui.label{text = 'The share sheet of UIKit, AppKit, Android and the Web Share API, and the links of the settings, the maps and the mail of each system. The desktops of the C library and Apple TV have no share sheet, and a web page cannot open the settings.', color = 'textMuted', font = 'caption'}
    self:frame{
        hint = 'Share, then open each app and come back.',
        focus = 'share',
        controls = controls,
        code = "local result = demo.share('A text', 'https://example.com'):await()\nlocal opened = demo.openApp('maps'):await()",
    }
end

function Sharing:share()
    self:act(function()
        local name = 'The share sheet opens'
        self.results:set('share', 'waiting', name, 'The share sheet shows.')
        local result, err = demo.share('A text that Haylen Tests shares.', 'https://example.com/haylen'):await()
        if err then
            self.results:failure('share', name, err)
            return
        end
        local shared = result.shared == nil and 'does not tell whether the person shared' or (result.shared and 'tells that the person shared' or 'tells that the person closed it')
        self.results:set('share', 'pass', name, string.format('The share sheet of %s opened, and the platform %s.', result.language, shared))
    end)
end

function Sharing:open(app)
    self:act(function()
        local result, err = demo.openApp(app.kind):await()
        if err then
            self.results:failure(app.kind, app.name, err)
            return
        end
        self.results:set(app.kind, result.opened and 'pass' or 'info', app.name, result.opened and string.format('%s opened it.', result.language) or 'No app of this device took the link.')
    end)
end

return Sharing
