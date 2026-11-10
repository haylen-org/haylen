-- The app asks the person for the permissions that the demo plugin owns, the camera, the microphone, the location and notifications, through `haylen.system`, which the native part of the plugin answers with the prompts of the system, and the plugin uses them: its camera starts or fails with the code "denied" and the sentence that says how to ask again, and it schedules a local notification, whose tap reaches the app as `notificationOpened`. The native part sends `notificationOpened` retained, so a tap that launched the closed app waits for the listener of this test.
local haylen = require('haylen')
local system = require('haylen.system')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Permissions = haylen.class('Permissions', DemoTest)

Permissions.code = [[
local state = system.requestPermission('camera'):await()
local started, failure = demo.startCamera():await()
demo.notify(5):await()
demo.onNotificationOpened(function(tap)
  print(tap.identifier)
end)]]

Permissions.notifySeconds = 5

-- The permissions that the demo plugin declares in the "permissions" of its "plugin.json".
Permissions.kinds = {'camera', 'microphone', 'location', 'notifications'}

-- The taps outlive the scene, because a retained event reaches only the listener that connects first.
Permissions.opened = {}

function Permissions:enter()
    self:frame{
        hint = 'Ask for the permissions, refuse one and start the camera, then schedule a notification, close the app and tap the notification. The menu offers this test again when the app opens.',
        focus = 'camera',
        controls = {
            ui.button{id = 'camera', text = 'Ask for the camera', variant = 'primary', onClick = function() self:ask('camera') end},
            ui.button{id = 'microphone', text = 'Ask for the microphone', onClick = function() self:ask('microphone') end},
            ui.button{id = 'location', text = 'Ask for the location', onClick = function() self:ask('location') end},
            ui.button{id = 'notifications', text = 'Ask for notifications', onClick = function() self:ask('notifications') end},
            ui.button{id = 'start', text = 'Start the camera', onClick = function() self:startCamera() end},
            ui.button{id = 'notify', text = string.format('Notify in %d seconds', Permissions.notifySeconds), onClick = function() self:notify() end},
            ui.label{text = 'The plugin declares these permissions in its "plugin.json" and registers them on each platform. Apple platforms show the prompt with the usage description of "plugin.json", and Android asks for the permissions of the manifest of the plugin. The plugin asks for a permission the app does not hold yet before it uses it. The tap on the notification reaches the app whether it runs, waits in the background or was closed.', color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.row{gap = 16, align = 'stretch',
                Permissions.card('camera', 'plugins/camera.svg', 'Camera'),
                Permissions.card('microphone', 'plugins/microphone.svg', 'Microphone'),
            },
            ui.row{gap = 16, align = 'stretch',
                Permissions.card('location', 'plugins/map.svg', 'Location'),
                Permissions.card('notifications', 'plugins/bell.svg', 'Notifications'),
            },
            ui.card{grow = 1, align = 'stretch', gap = 12, justify = 'center',
                ui.icon{image = 'plugins/bell.svg', size = 64, color = 'textMuted', align = 'center'},
                ui.label{text = 'Local notification', font = 'heading', textAlign = 'center', align = 'stretch'},
                ui.label{id = 'notification', text = string.format('Notify in %d seconds, then tap the notification, even after closing the app.', Permissions.notifySeconds), color = 'textMuted', textAlign = 'center', align = 'stretch'},
            },
        },
    }
    if not self.native then
        return
    end
    self:spawn(function()
        for _, kind in ipairs(Permissions.kinds) do
            self:show(kind, system.permission(kind):await())
        end
    end)
    self.connection = demo.onNotificationOpened(function(payload)
        local opened = Permissions.opened
        opened[#opened + 1] = {identifier = payload.identifier, action = payload.action, frame = haylen.frameIndex()}
        self:showOpened()
    end)
    self:showOpened()
end

-- A card with the state of one permission, which the answer of its prompt changes.
function Permissions.card(kind, icon, title)
    return ui.card{grow = 1, align = 'stretch', gap = 8,
        ui.row{gap = 16, ui.icon{image = icon, size = 44, color = 'accentText'}, ui.label{text = title, font = 'heading', grow = 1}},
        ui.statusIndicator{id = kind .. 'State', text = 'Checking', tone = 'neutral'},
    }
end

-- The words and the tone of each state of a permission.
Permissions.states = {granted = {'Granted', 'success'}, prompt = {'Not asked yet', 'information'}, denied = {'Denied', 'danger'}, unknown = {'Asked at the first use', 'warning'}}

function Permissions:exit()
    if self.connection then
        self.connection:disconnect()
    end
    Permissions.super.exit(self)
end

function Permissions:show(kind, state, message)
    local shown = Permissions.states[state]
    self:set(kind .. 'State', shown and {text = shown[1], tone = shown[2]} or {text = 'Unsupported', tone = 'neutral', tooltip = message})
end

function Permissions:ask(kind)
    self:act(function()
        local name = 'The ' .. kind .. ' permission reaches the app'
        self.results:set(kind, 'waiting', name, 'The prompt of the system shows, unless the person answered it before.')
        local state, message, failure = system.requestPermission(kind):await()
        self:show(kind, state, message)
        if failure then
            self.results:set(kind, 'skip', name, message)
            return
        end
        self.results:set(kind, 'pass', name, string.format('The request answered "%s".', state))
    end)
end

-- The plugin asks for the camera where the app does not hold it yet, so a refused camera fails at once with the code "denied".
function Permissions:startCamera()
    self:act(function()
        local name = 'The camera of the plugin follows the permission'
        local started, err = demo.startCamera():await()
        if err and err.code == 'noHandler' then
            self.results:set('start', 'skip', name, 'The C library of the desktops has no camera.')
            return
        elseif err and err.code == 'denied' then
            self:show('camera', 'denied')
            self.results:set('start', 'pass', name, err.message)
            return
        elseif err then
            self.results:failure('start', name, err)
            return
        end
        demo.stopCamera()
        self:show('camera', 'granted')
        self.results:set('start', 'pass', name, string.format('The camera started at %d by %d pixels in %s.', started.width, started.height, started.language))
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
