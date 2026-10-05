-- The plugin opens the file picker of the platform and answers with the name of the picked file, or `nil` when the person cancels. Android launches the document picker through the Activity Result API, Apple platforms present `UIDocumentPickerViewController` or `NSOpenPanel`, and the web opens a file input.
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Result = haylen.class('Result', DemoTest)

function Result:enter()
    self.picks = 0
    self:frame{
        hint = 'Pick a file, then pick again and cancel.',
        focus = 'pick',
        controls = {
            ui.button{id = 'pick', text = 'Pick a file', variant = 'primary', onClick = function() self:pick() end},
            ui.label{text = 'The desktops share no C API for a file picker, so "pickFile" fails there with the code "unsupported". A browser opens its file chooser only right after a click, a tap or a key press.', color = 'textMuted', font = 'caption'},
        },
        code = "local picked, err = demo.pickFile():await()\nprint(picked and picked.name or 'cancelled')",
    }
end

function Result:pick()
    self:act(function()
        self.picks = self.picks + 1
        local key = 'pick' .. self.picks
        local name = 'The call "pickFile", attempt ' .. self.picks
        self.results:set(key, 'waiting', name, 'The file picker shows.')
        local picked, err = demo.pickFile():await()
        if err and err.code == 'noUserGesture' then
            self.results:set(key, 'info', name, err.message)
        elseif err then
            self.results:failure(key, name, err)
        elseif picked then
            self.results:set(key, 'pass', name, 'The person picked "' .. picked.name .. '".')
        else
            self.results:set(key, 'pass', name, 'The person cancelled, and "pickFile" answered "nil".')
        end
    end)
end

return Result
