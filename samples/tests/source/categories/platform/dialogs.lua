-- The native message box and the pickers of files to open, of the destination of data to save and of a folder, whose answers reach Lua on a later frame, and a message that the app gives up, which the platform closes. Each dialog has a card with its last answer.
local async = require('async')
local dialogs = require('haylen.dialogs')
local fs = require('fs')
local haylen = require('haylen')
local json = require('json')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Dialogs = haylen.class('Dialogs', Test)

Dialogs.cancelSeconds = 2
Dialogs.kinds = {
    {id = 'message', title = 'A message', call = 'dialogs.message', button = 'Show a message', icon = 'platform/message.svg', primary = true},
    {id = 'open', title = 'Files to open', call = 'dialogs.openFiles', button = 'Open files', icon = 'plugins/file.svg'},
    {id = 'save', title = 'Where to save', call = 'dialogs.saveFile', button = 'Save a file', icon = 'platform/save.svg'},
    {id = 'folder', title = 'A folder', call = 'dialogs.openFolder', button = 'Open a folder', icon = 'platform/folder.svg'},
    {id = 'giveUp', title = 'A message given up', call = 'call:cancel()', button = 'Give a message up', icon = 'platform/timer.svg'},
}

function Dialogs:enter()
    local controls, cards = {}, {}
    for index, kind in ipairs(Dialogs.kinds) do
        controls[index] = ui.button{id = kind.id, text = kind.button, variant = kind.primary and 'primary' or 'default', onClick = function() self[kind.id](self) end}
        cards[index] = ui.card{align = 'stretch', padding = {12, 20},
            ui.row{gap = 20,
                ui.icon{image = kind.icon, size = 44, color = 'accentText'},
                ui.column{grow = 1, gap = 2,
                    ui.row{gap = 12, ui.label{text = kind.title, font = 'heading'}, ui.label{text = kind.call, font = 'monospace', color = 'accentText'}},
                    ui.label{id = kind.id .. '-answer', text = 'Its answer shows here.', color = 'textMuted'},
                },
                ui.badge{id = kind.id .. '-state', text = 'Not shown yet', tone = 'neutral'},
            },
        }
    end
    controls[#controls + 1] = ui.label{text = string.format('Every dialog answers on a later frame and never blocks the app. The message given up closes by itself after %d seconds, and platforms without a picker, such as tvOS, answer with the code "unsupported".', Dialogs.cancelSeconds), color = 'textMuted', font = 'caption'}
    self:frame{
        hint = 'Show each dialog and answer it, or cancel it.',
        focus = 'message',
        controls = controls,
        content = {ui.scroll{grow = 1, height = 0, ui.column{gap = 12, children = cards}}},
        code = "local button = dialogs.message({text = 'Save?', buttons = {'Save', 'Discard', 'Cancel'}}):await()\nlocal files = dialogs.openFiles({multiple = true}):await()\nlocal saved = dialogs.saveFile({name = 'report.json', data = data}):await()\nlocal folder = dialogs.openFolder():await()\nlocal call = dialogs.message({text = 'Closing soon.'})\ncall:cancel()  -- The platform closes it, and the call fails with the code \"cancelled\".",
    }
end

-- Runs a dialog in a task of the scene and shows its answer, or its error, with the frames it took, on its card and in the log. A dialog that the platform lacks or that the app gave up is expected to fail.
function Dialogs:run(id, call, describe)
    local frame = haylen.frameIndex()
    self:set(id .. '-state', {text = 'Showing', tone = 'information'})
    self:set(id .. '-answer', {text = 'Waiting for the answer.', color = 'textMuted'})
    self:spawn(function()
        local answer, err = call:await()
        local frames = haylen.frameIndex() - frame
        local text, tone
        if err then
            local expected = err.code == 'unsupported' or err.code == 'cancelled'
            text = string.format('After %d frames, it failed with the code "%s": %s', frames, tostring(err.code), err.message)
            tone = expected and 'neutral' or 'danger'
            self:set(id .. '-state', {text = expected and 'Ended' or 'Failed', tone = tone})
        else
            text = string.format('After %d frames, %s', frames, describe(answer))
            self:set(id .. '-state', {text = 'Answered', tone = 'success'})
        end
        self:set(id .. '-answer', {text = text, color = 'text'})
        self:log('%s', text)
    end)
end

function Dialogs:message()
    local call = dialogs.message({title = 'Unsaved changes', text = 'Save the changes before leaving?', kind = 'warning', buttons = {'Save', 'Discard', 'Cancel'}})
    self:run('message', call, function(button)
        return button and 'the person pressed "' .. button .. '".' or 'the message closed without a button.'
    end)
end

-- Every picked file is readable at its path, so the card shows the size it reads.
function Dialogs:open()
    local call = dialogs.openFiles({title = 'Pick files', filters = {{name = 'Images', extensions = {'png', 'jpg'}}, {name = 'Text', extensions = {'txt', 'json', 'md'}}}, multiple = true})
    self:run('open', call, function(files)
        if not files then
            return 'the person cancelled the picker.'
        end
        local names = {}
        for _, file in ipairs(files) do
            local data = fs.readFile(file.path):await()
            names[#names + 1] = string.format('"%s" with %d bytes', file.name, #data)
        end
        return 'the person picked ' .. table.concat(names, ', ') .. '.'
    end)
end

function Dialogs:save()
    local data = json.encode({saved = os.date('%Y-%m-%d %H:%M:%S'), frame = haylen.frameIndex()})
    local call = dialogs.saveFile({title = 'Save the report', name = 'haylen-report.json', data = data, filters = {{name = 'JSON', extensions = {'json'}}}})
    self:run('save', call, function(saved)
        return saved and string.format('the report went to "%s".', saved.path or saved.name) or 'the person cancelled the picker.'
    end)
end

function Dialogs:folder()
    self:run('folder', dialogs.openFolder({title = 'Pick a folder'}), function(folder)
        return folder and 'the person picked "' .. folder .. '".' or 'the person cancelled the picker.'
    end)
end

-- The platform closes the message that the app gave up, and the call fails with `cancelled`.
function Dialogs:giveUp()
    local call = dialogs.message({text = string.format('This message closes by itself after %d seconds.', Dialogs.cancelSeconds), buttons = {'OK'}})
    self:run('giveUp', call, function(button)
        return 'the person pressed "' .. tostring(button) .. '" before the app gave it up.'
    end)
    self:spawn(function()
        async.sleep(Dialogs.cancelSeconds * 1000):await()
        call:cancel()
    end)
end

return Dialogs
