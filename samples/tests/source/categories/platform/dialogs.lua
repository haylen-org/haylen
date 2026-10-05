-- The native message box and the pickers of files to open, of the destination of data to save and of a folder, whose answers reach Lua on a later frame, and a message that the app gives up, which the platform closes.
local async = require('async')
local dialogs = require('haylen.dialogs')
local fs = require('fs')
local haylen = require('haylen')
local json = require('json')
local ui = require('haylen.ui')

local Journal = require('harness.journal')
local Test = require('harness.test')

local Dialogs = haylen.class('Dialogs', Test)

Dialogs.cancelSeconds = 2

function Dialogs:enter()
    self.journal = Journal(18)
    self:frame{
        hint = 'Show each dialog and answer it, or cancel it.',
        focus = 'message',
        controls = {
            ui.button{id = 'message', text = 'Show a message', variant = 'primary', onClick = function() self:message() end},
            ui.button{id = 'open', text = 'Open files', onClick = function() self:openFiles() end},
            ui.button{id = 'save', text = 'Save a file', onClick = function() self:saveFile() end},
            ui.button{id = 'folder', text = 'Open a folder', onClick = function() self:openFolder() end},
            ui.button{id = 'cancel', text = string.format('Show a message and give it up after %d seconds', Dialogs.cancelSeconds), onClick = function() self:cancelLater() end},
            ui.label{text = 'Every dialog answers on a later frame and never blocks the app. Platforms without a picker, such as tvOS, answer with the code "unsupported".', color = 'textMuted', font = 'caption'},
        },
        code = "local button = dialogs.message({\n  text = 'Save?', buttons = {'Save', 'Discard', 'Cancel'},\n}):await()\nlocal files = dialogs.openFiles({multiple = true}):await()",
    }
end

-- Runs a dialog in a task of the scene and writes its answer, or its error, with the frames it took, to the journal and to the log. A dialog that the platform lacks or that the app gave up is expected to fail.
function Dialogs:run(name, call, describe)
    local frame = haylen.frameIndex()
    self.journal:add(string.format('The call "%s" shows.', name), Test.accent)
    self:spawn(function()
        local answer, err = call:await()
        local frames = haylen.frameIndex() - frame
        local text, color
        if err then
            text = string.format('The call "%s" failed with "%s" after %d frames: %s', name, tostring(err.code), frames, err.message)
            color = (err.code == 'unsupported' or err.code == 'cancelled') and Test.muted or Test.red
        else
            text = string.format('The call "%s" answered after %d frames: %s', name, frames, describe(answer))
            color = Test.green
        end
        self.journal:add(text, color)
        self:log('%s', text)
    end)
end

function Dialogs:message()
    local call = dialogs.message({title = 'Unsaved changes', text = 'Save the changes before leaving?', kind = 'warning', buttons = {'Save', 'Discard', 'Cancel'}})
    self:run('dialogs.message', call, function(button)
        return button and 'the button "' .. button .. '".' or 'no button.'
    end)
end

-- Every picked file is readable at its path, so the journal shows the size it reads.
function Dialogs:openFiles()
    local call = dialogs.openFiles({title = 'Pick files', filters = {{name = 'Images', extensions = {'png', 'jpg'}}, {name = 'Text', extensions = {'txt', 'json', 'md'}}}, multiple = true})
    self:run('dialogs.openFiles', call, function(files)
        if not files then
            return 'nothing, since the picker was cancelled.'
        end
        local names = {}
        for _, file in ipairs(files) do
            local data = fs.readFile(file.path):await()
            names[#names + 1] = string.format('"%s" with %d bytes at "%s"', file.name, #data, file.path)
        end
        return table.concat(names, ', ') .. '.'
    end)
end

function Dialogs:saveFile()
    local data = json.encode({saved = os.date('%Y-%m-%d %H:%M:%S'), frame = haylen.frameIndex()})
    local call = dialogs.saveFile({title = 'Save the report', name = 'haylen-report.json', data = data, filters = {{name = 'JSON', extensions = {'json'}}}})
    self:run('dialogs.saveFile', call, function(saved)
        return saved and string.format('"%s" at "%s".', saved.name, saved.path or 'no path') or 'nothing, since the picker was cancelled.'
    end)
end

function Dialogs:openFolder()
    self:run('dialogs.openFolder', dialogs.openFolder({title = 'Pick a folder'}), function(folder)
        return folder and '"' .. folder .. '".' or 'nothing, since the picker was cancelled.'
    end)
end

-- The platform closes the message that the app gave up, and the call fails with `cancelled`.
function Dialogs:cancelLater()
    local call = dialogs.message({text = string.format('This message closes by itself after %d seconds.', Dialogs.cancelSeconds), buttons = {'OK'}})
    self:run('dialogs.message', call, function(button)
        return 'the button "' .. tostring(button) .. '".'
    end)
    self:spawn(function()
        async.sleep(Dialogs.cancelSeconds * 1000):await()
        self.journal:add(call:cancel() and 'The method "call:cancel()" gave the message up.' or 'The method "call:cancel()" found the message answered.', Test.accent)
    end)
end

function Dialogs:draw(area)
    Test.caption('Answers of the dialogs', 24, 20, {size = 22, color = Test.ink})
    self.journal:draw(24, 56, area.height - 76, 19)
end

return Dialogs
