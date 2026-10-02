-- Permissions and notifications: the plugin asks the person for the camera and for notifications with the prompts of the system, whose answers reach the calls, and schedules a local notification, whose tap reaches the app as `notificationOpened`. The native part sends `notificationOpened` retained, so a tap that launched the closed app waits for the listener of this test.
local haylen = require('haylen')
local ui = require('haylen.ui')

local demo = require('native-demo')
local sample = require('sample')

local Permissions = haylen.class('Permissions', sample.Test)

-- The taps outlive the scene, because a retained event reaches only the listener that connects first.
local opened = {}

local kNotifySeconds = 5

function Permissions:enter()
    self:frame({
        hint = 'Ask for the permissions, then schedule a notification, close the app and tap the notification.',
        focus = 'camera',
        controls = {
            ui.button{id = 'camera', text = 'Ask for the camera', variant = 'primary', onClick = function() self:ask('camera') end},
            ui.button{id = 'notifications', text = 'Ask for notifications', onClick = function() self:ask('notifications') end},
            ui.button{id = 'notify', text = string.format('Notify in %d seconds', kNotifySeconds), onClick = function() self:notify() end},
            ui.label{text = 'Apple platforms show the camera prompt with the usage description that "plugin.json" puts into the "Info.plist", and Android asks for the permissions that the manifest of the plugin declares. The notification shows while the app is in front too, and its tap reaches the app whether it runs, waits in the background or was closed.', color = 'textMuted', font = 'caption'},
            ui.label{font = 'monospace', text = "local answer = demo.requestPermission('camera'):await()\ndemo.notify(5):await()\ndemo.onNotificationOpened(function(tap)\n  print(tap.identifier)\nend)"},
        },
    })
    if not self.native then
        return
    end
    self.connection = demo.onNotificationOpened(function(payload)
        opened[#opened + 1] = {identifier = payload.identifier, action = payload.action, frame = haylen.frameIndex()}
        self:showOpened()
    end)
    self:showOpened()
end

function Permissions:exit()
    if self.connection then
        self.connection:disconnect()
    end
end

function Permissions:ask(kind)
    self:act(function()
        local name = 'The ' .. kind .. ' permission reaches the app'
        self.results:set(kind, 'waiting', name, 'The prompt of the system shows, unless the person answered it before.')
        local answer, err = demo.requestPermission(kind):await()
        if err and err.code == 'noHandler' then
            self.results:set(kind, 'skip', name, 'The native part asks for permissions on Apple platforms and Android alone so far.')
            return
        elseif err then
            self.results:failure(kind, name, err)
            return
        end
        self.results:set(kind, 'pass', name, string.format('The native part in %s answered %s with the status "%s".', answer.language, answer.granted and 'granted' or 'not granted', answer.status))
    end)
end

function Permissions:notify()
    self:act(function()
        local name = 'A local notification is scheduled'
        local scheduled, err = demo.notify(kNotifySeconds):await()
        if err and err.code == 'noHandler' then
            self.results:set('notify', 'skip', name, 'The native part schedules notifications on Apple platforms and Android alone so far.')
            return
        elseif err then
            self.results:failure('notify', name, err)
            return
        end
        self.results:set('notify', 'pass', name, string.format('The notification "%s" shows in %d seconds. Tap it now, or close the app first to see the tap launch it.', scheduled.identifier, kNotifySeconds))
    end)
end

function Permissions:showOpened()
    local name = 'The tap on the notification reaches the app'
    if #opened == 0 then
        self.results:set('opened', 'waiting', name, 'No notification of the plugin was tapped yet.')
        return
    end
    local last = opened[#opened]
    self.results:set('opened', 'pass', name, string.format('The app received %d taps as "notificationOpened", the last one with the action "%s" at frame %d for "%s".', #opened, last.action, last.frame, last.identifier))
end

return Permissions
