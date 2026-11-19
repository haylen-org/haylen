-- The plugin schedules a local notification once the person allowed notifications, and its tap reaches the app as `notificationOpened`. Swift asks the notification center and JavaScript asks the browser before they schedule it, and on Android the Lua API first opens the screen of the plugin whose Activity Result contract shows the permission dialog. The native part sends `notificationOpened` retained, so a tap that launched the closed app waits for the listener of this test.
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Notifications = haylen.class('Notifications', DemoTest)

Notifications.code = [[
local scheduled, err = demo.notify(5)  -- The system asks the person first.
demo.onNotificationOpened(function(tap)
  print(tap.identifier, tap.action)
end)]]

Notifications.notifySeconds = 5
Notifications.scheduleName = 'A local notification is scheduled'
Notifications.openedName = 'The tap on the notification reaches the app'

-- The taps outlive the scene, because a retained event reaches only the listener that connects first.
Notifications.opened = {}

function Notifications:enter()
    self:frame{
        hint = 'Schedule a notification, then tap it while the app shows, from the home screen or after closing the app. The menu offers this test again when the app opens.',
        focus = 'notify',
        controls = {
            ui.button{id = 'notify', text = string.format('Notify in %d seconds', Notifications.notifySeconds), variant = 'primary', onClick = function() self:notify() end},
            ui.label{text = 'The notification center on Apple platforms, an alarm that posts the notification on Android and the Notification API on the web. The system asks the person for notifications the first time, on Android through a screen of the plugin. The tap reaches the app whether it runs, waits in the background or was closed.', color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.card{grow = 1, align = 'stretch', gap = 12, justify = 'center',
                ui.icon{image = 'plugins/bell.svg', size = 64, color = 'textMuted', align = 'center'},
                ui.label{text = 'Local notification', font = 'heading', textAlign = 'center', align = 'stretch'},
                ui.label{id = 'notification', text = string.format('Notify in %d seconds, then tap the notification, even after closing the app.', Notifications.notifySeconds), color = 'textMuted', textAlign = 'center', align = 'stretch'},
            },
        },
    }
    if not self.native then
        return
    end
    self.connection = demo.onNotificationOpened(function(payload)
        local opened = Notifications.opened
        opened[#opened + 1] = {identifier = payload.identifier, action = payload.action, frame = haylen.frameIndex()}
        self:showOpened()
    end)
    self:showOpened()
end

function Notifications:exit()
    if self.connection then
        self.connection:disconnect()
    end
    Notifications.super.exit(self)
end

function Notifications:notify()
    self:act(function()
        local name = Notifications.scheduleName
        self.results:set('notify', 'waiting', name, 'The plugin asks for notifications and schedules the notification.')
        local scheduled, err = demo.notify(Notifications.notifySeconds)
        if err and err.code == 'noHandler' then
            self.results:set('notify', 'skip', name, 'The C++ library of the desktops shows no notifications.')
            return
        elseif err and err.code == 'denied' then
            self.results:set('notify', 'info', name, 'The plugin schedules nothing, since the person refused notifications: ' .. err.message)
            return
        elseif err then
            self.results:failure('notify', name, err)
            return
        end
        self:set('notification', {text = string.format('The notification shows in %d seconds. Tap it now, or close the app first to see the tap launch it, except on the web, where a closed page shows nothing.', Notifications.notifySeconds)})
        self.results:set('notify', 'pass', name, string.format('The notification "%s" is on its way.', scheduled.identifier))
        self.results:set('opened', 'waiting', Notifications.openedName, string.format('The app received %d taps so far and waits for the tap on "%s".', #Notifications.opened, scheduled.identifier))
    end)
end

function Notifications:showOpened()
    local name = Notifications.openedName
    local opened = Notifications.opened
    if #opened == 0 then
        self.results:set('opened', 'waiting', name, 'No notification of the plugin was tapped yet.')
        return
    end
    local last = opened[#opened]
    self.results:set('opened', 'pass', name, string.format('The app received %d taps as "notificationOpened", the last one with the action "%s" at frame %d for "%s".', #opened, last.action, last.frame, last.identifier))
end

return Notifications
