-- App errors: a button raises a Lua error on purpose, which stops the app on the error screen. The native part receives the error through its app error hook and keeps it, and after R restarts the app from the error screen it sends the error back as lastError, retained, which this test shows.
local haylen = require('haylen')
local ui = require('haylen.ui')

local demo = require('native-demo')
local sample = require('sample')

local Errors = haylen.class('Errors', sample.Test)

-- The error the native part sent back outlives the scene, because a retained event reaches only the listener that connects first.
local received = nil

function Errors:enter()
    self:frame({
        hint = 'Raise the error, restart the app from the error screen and open this test again.',
        focus = 'raise',
        controls = {
            ui.button{id = 'raise', text = 'Raise a Lua error', variant = 'primary', onClick = function() self:raise() end},
            ui.label{text = 'appDidFail(with:) receives the error on Apple platforms, onAppError on Android, context.onAppError on the web and the error handler of HaylenNativeApi on the desktops. The native part keeps the message and sends it when the next app starts.', color = 'textMuted', font = 'caption'},
            ui.label{font = 'monospace', text = "demo.onLastError(function(failure)\n  print(failure.message, failure.line)\nend)"},
        },
    })
    if not self.native then
        return
    end

    self.connection = demo.onLastError(function(payload)
        received = payload
        self:show()
    end)
    self:show()
end

function Errors:exit()
    if self.connection then
        self.connection:disconnect()
    end
end

function Errors:raise()
    if self.native then
        error(string.format('The app errors test raised this error on purpose at %s.', os.date('%H:%M:%S')))
    end
end

function Errors:show()
    local name = 'the native part sends the last error back'
    if not received then
        self.results:set('last', 'waiting', name, 'No earlier app of this process stopped with an error. Raise one and restart with R.')
        return
    end
    local expected = received.message:find('raised this error on purpose', 1, true) ~= nil
    self.results:set('last', expected and 'pass' or 'fail', name, string.format('%s kept "%s" from %s, line %s, and sent it back as lastError after the restart.', received.language, received.message, received.file, tostring(received.line)))
end

return Errors
