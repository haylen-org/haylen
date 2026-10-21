-- The plugin asks the person for the camera and for notifications with the prompts of the system, whose answers reach the calls, and schedules a local notification, whose tap reaches the app as `notificationOpened`. The native part sends `notificationOpened` retained, so a tap that launched the closed app waits for the listener of this test.
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Permissions = haylen.class('Permissions', DemoTest)

Permissions.notifySeconds = 5

-- The taps outlive the scene, because a retained event reaches only the listener that connects first.
Permissions.opened = {}

function Permissions:enter()
    self:frame{
        hint = 'Ask for the permissions, then schedule a notification, close the app and tap the notification. The menu offers this test again when the app opens.',
        focus = 'camera',
        controls = {
            ui.button{id = 'camera', text = 'Ask for the camera', variant = 'primary', onClick = function() self:ask('camera') end},
            ui.button{id = 'notifications', text = 'Ask for notifications', onClick = function() self:ask('notifications') end},
            ui.button{id = 'notify', text = string.format('Notify in %d seconds', Permissions.notifySeconds), onClick = function() self:notify() end},
            ui.label{text = 'Apple platforms show the prompt with the usage description of "plugin.json", and Android asks for the permissions of the manifest of the plugin. The tap on the notification reaches the app whether it runs, waits in the background or was closed.', color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.row{gap = 16, align = 'stretch',
                Permissions.card('camera', 'plugins/camera.svg', 'Camera'),
                Permissions.card('notifications', 'plugins/bell.svg', 'Notifications'),
            },
            ui.card{grow = 1, align = 'stretch', gap = 16, justify = 'center',
                ui.icon{image = 'plugins/bell.svg', size = 96, color = 'textMuted', align = 'center'},
                ui.label{text = 'Local notification', font = 'heading', textAlign = 'center', align = 'stretch'},
                ui.label{id = 'notification', text = string.format('Notify in %d seconds, then tap the notification, even after closing the app.', Permissions.notifySeconds), color = 'textMuted', textAlign = 'center', align = 'stretch'},
            },
        },
        code ="local answer = demo.requestPermission('camera'):await()\ndemo.notify(5):await()\ndemo.onNotificationOpened(function(tap)\n  print(tap.identifier)\nend)",
    }
    if not self.native then
        return
    end
    self.connection = demo.onNotificationOpened(function(payload)
        local opened = Permissions.opened
        opened[#opened + 1] = {identifier = payload.identifier, action = payload.action, frame = haylen.frameIndex()}
        self:showOpened()
    end)
    self:showOpened()
end

-- A card with the state of one permission, which the answer of its prompt changes.
function Permissions.card(kind, icon, title)
    return ui.card{grow = 1, align = 'stretch', gap = 12,
        ui.row{gap = 16, ui.icon{image = icon, size = 56, color = 'accentText'}, ui.label{text = title, font = 'heading', grow = 1}},
        ui.statusIndicator{id = kind .. 'State', text = 'Not asked yet', tone = 'information'},
    }
end

function Permissions:exit()
    if self.connection then
        self.connection:disconnect()
    end
    Permissions.super.exit(self)
end

function Permissions:ask(kind)
    self:act(function()
        local name = 'The ' .. kind .. ' permission reaches the app'
        self.results:set(kind, 'waiting', name, 'The prompt of the system shows, unless the person answered it before.')
        local answer, err = demo.requestPermission(kind):await()
        if err and err.code == 'noHandler' then
            self.results:set(kind, 'skip', name, 'The C library of the desktops asks for no permissions.')
            return
        elseif err then
            self.results:failure(kind, name, err)
            return
        end
        self:set(kind .. 'State', {text = answer.granted and 'Granted' or 'Not granted, with the status "' .. answer.status .. '"', tone = answer.granted and 'success' or 'danger'})
        self.results:set(kind, 'pass', name, string.format('The native part in %s answered %s with the status "%s".', answer.language, answer.granted and 'granted' or 'not granted', answer.status))
    end)
end

function Permissions:notify()
    self:act(function()
        local name = 'A local notification is scheduled'
        local scheduled, err = demo.notify(Permissions.notifySeconds):await()
        if err and err.code == 'noHandler' then
            self.results:set('notify', 'skip', name, 'The C library of the desktops shows no notifications.')
            return
        elseif err then
            self.results:failure('notify', name, err)
            return
        end
        self:set('notification', {text = string.format('The notification shows in %d seconds. Tap it now, or close the app first to see the tap launch it, except on the web, where a closed page shows nothing.', Permissions.notifySeconds)})
        self.results:set('notify', 'pass', name, string.format('The notification "%s" is on its way.', scheduled.identifier))
    end)
end

function Permissions:showOpened()
    local name = 'The tap on the notification reaches the app'
    local opened = Permissions.opened
    if #opened == 0 then
        self.results:set('opened', 'waiting', name, 'No notification of the plugin was tapped yet.')
        return
    end
    local last = opened[#opened]
    self.results:set('opened', 'pass', name, string.format('The app received %d taps as "notificationOpened", the last one with the action "%s" at frame %d for "%s".', #opened, last.action, last.frame, last.identifier))
end

return Permissions
