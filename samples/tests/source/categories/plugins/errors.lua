-- A button raises a Lua error on purpose, which shows the error screen. The native part receives the error through its app error hook and keeps it, and after R or Restart restarts the app from the error screen it sends the error back as `lastError`, retained, which this test shows.
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Errors = haylen.class('Errors', DemoTest)

-- The error the native part sent back outlives the scene, because a retained event reaches only the listener that connects first.
Errors.received = nil

function Errors:enter()
    self:frame{
        hint = 'Raise the error, restart the app from the error screen with R or Restart, and open this test again from the menu.',
        focus = 'raise',
        controls = {
            ui.button{id = 'raise', text = 'Raise a Lua error', variant = 'primary', onClick = function() self:raise() end},
            ui.label{text = 'The method "appDidFail(with:)" receives the error on Apple platforms, "onAppError" on Android, "context.onAppError" on the web and the error handler of "HaylenNativeApi" on the desktops. The native part keeps the message and sends it when the next app starts.', color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.card{grow = 1, align = 'stretch', gap = 16, justify = 'center',
                ui.icon{id = 'errorIcon', image = 'interface/states/info.svg', size = 96, color = 'textMuted', align = 'center'},
                ui.label{text = 'The last error the native part kept', font = 'heading', textAlign = 'center', align = 'stretch'},
                ui.label{id = 'errorMessage', text = 'No earlier app of this process stopped with an error.', textAlign = 'center', align = 'stretch'},
                ui.label{id = 'errorPlace', text = '', font = 'caption', color = 'textMuted', textAlign = 'center', align = 'stretch'},
            },
        },
        code = "demo.onLastError(function(failure)\n  print(failure.message, failure.line)\nend)",
    }
    if not self.native then
        return
    end

    self.connection = demo.onLastError(function(payload)
        Errors.received = payload
        self:showError()
    end)
    self:showError()
end

function Errors:exit()
    if self.connection then
        self.connection:disconnect()
    end
    Errors.super.exit(self)
end

function Errors:raise()
    if self.native then
        error(string.format('The app errors test raised this error on purpose at %s.', os.date('%H:%M:%S')))
    end
end

function Errors:showError()
    local name = 'The native part sends the last error back'
    local received = Errors.received
    if not received then
        self.results:set('last', 'waiting', name, 'No earlier app of this process stopped with an error. Raise one and restart with R.')
        return
    end
    self:set('errorMessage', {text = received.message})
    self:set('errorPlace', {text = string.format('From "%s", line %s, kept by %s.', received.file, tostring(received.line), received.language)})
    self:set('errorIcon', {image = 'interface/states/fail.svg', color = 'dangerText'})
    if not received.message:find('raised this error on purpose', 1, true) then
        self.results:set('last', 'info', name, 'The last error of this process came from another part of the project. Raise the error of this test to check it.')
        return
    end
    self.results:set('last', 'pass', name, string.format('%s kept the error of this test and sent it back as "lastError" after the restart.', received.language))
end

return Errors
