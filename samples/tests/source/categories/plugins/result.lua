-- The plugin opens the file picker of the platform and answers with the name of the picked file, or `nil` when the person cancels. Android launches the document picker through the Activity Result API, Apple platforms present `UIDocumentPickerViewController` or `NSOpenPanel`, and the web opens a file input.
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Result = haylen.class('Result', DemoTest)

Result.pickers = {
    ios = 'The document picker of UIKit answers the call.',
    macos = 'The open panel of AppKit answers the call.',
    android = 'The document picker of Android answers through the Activity Result API.',
    web = 'A file input of the page answers the call, and the browser opens it only right after a click, a tap or a key press.',
}

function Result:enter()
    self.picks = 0
    self:frame{
        hint = 'Pick a file, then pick again and cancel.',
        focus = 'pick',
        checksEmpty = 'Pick a file to check the answer of the picker.',
        controls = {
            ui.button{id = 'pick', text = 'Pick a file', variant = 'primary', onClick = function() self:pick() end},
            ui.label{text = Result.pickers[haylen.platform] or 'This platform has no file picker of the plugin, so "pickFile" fails with the code "unsupported".', color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.card{grow = 1, align = 'stretch', gap = 16, justify = 'center',
                ui.icon{id = 'fileIcon', image = 'plugins/file.svg', size = 128, color = 'textMuted', align = 'center'},
                ui.label{id = 'fileName', text = 'No file picked yet', font = 'heading', textAlign = 'center', align = 'stretch'},
                ui.label{id = 'fileState', text = 'The name of the file that the picker answers with shows here.', color = 'textMuted', textAlign = 'center', align = 'stretch'},
            },
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
            self:set('fileName', {text = picked.name})
            self:set('fileIcon', {color = 'accentText'})
            self:set('fileState', {text = string.format('Picked on attempt %d.', self.picks)})
            self.results:set(key, 'pass', name, 'The person picked "' .. picked.name .. '".')
        else
            self:set('fileState', {text = string.format('The person cancelled attempt %d, and the call answered "nil".', self.picks)})
            self.results:set(key, 'pass', name, 'The person cancelled, and "pickFile" answered "nil".')
        end
    end)
end

return Result
