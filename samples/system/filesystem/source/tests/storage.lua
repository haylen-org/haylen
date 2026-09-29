-- The user folder with haylen.storage: every call runs at once and returns its result, writes replace files atomically and create missing folders, and paths stay inside the folder of the app.
local haylen = require('haylen')
local storage = require('haylen.storage')
local ui = require('haylen.ui')

local Activity = require('activity')
local sample = require('sample')

local Storage = haylen.class('Storage', sample.Test)

Storage.hints = 'Press the buttons with the mouse, a finger, or the arrows and Enter or A. Every call and its result goes to the activity on the right.'
Storage.focus = 'write'

local kNote = 'notes/today.txt'
local kProfile = 'profile/player.json'
local kFolders = {'notes', 'profile'}

function Storage:init(entry)
    Storage.super.init(self, entry)
    self.activity = Activity(self, 'activity')
    self.lines = 0
end

function Storage:content()
    local actions = {
        {id = 'write', text = 'Write the note', run = self.write},
        {id = 'append', text = 'Append a line', run = self.append},
        {id = 'read', text = 'Read the note', run = self.read},
        {id = 'writeJson', text = 'Write JSON', run = self.writeJson},
        {id = 'readJson', text = 'Read JSON', run = self.readJson},
        {id = 'exists', text = 'Check exists', run = self.exists},
        {id = 'missing', text = 'Read a missing file', run = self.missing},
        {id = 'outside', text = 'Leave the folder', run = self.outside},
        {id = 'remove', text = 'Remove the note', run = self.remove},
        {id = 'clear', text = 'Remove everything', run = self.clear},
    }
    local buttons = {}
    for index, action in ipairs(actions) do
        buttons[index] = ui.button{id = action.id, text = action.text, align = 'stretch', onClick = function()
            action.run(self)
            self:refresh()
        end}
    end
    return {
        ui.panel{width = 560, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'haylen.storage'},
            ui.grid{columns = 2, gap = 12, children = buttons},
            ui.label{text = 'Folder of the app: ' .. storage.root(), font = 'caption', color = 'textMuted'},
        },
        ui.panel{width = 520, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'storage.list()'},
            ui.scroll{grow = 1, focusable = false, ui.list{id = 'files', items = {}}},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'Activity'},
            self.activity:node(),
        },
    }
end

function Storage:enter()
    Storage.super.enter(self)
    self:refresh()
end

-- Lists every file of the user folder with its size, which storage learns by reading the file.
function Storage:refresh()
    local items = {}
    for index, path in ipairs(storage.list()) do
        items[index] = {id = path, text = path, caption = sample.bytes(#storage.read(path))}
    end
    self:show('files', {items = items})
end

function Storage:write()
    local text = string.format('Notes of %s\n', os.date('%Y-%m-%d'))
    storage.write(kNote, text)
    self.lines = 0
    self.activity:add(string.format("storage.write('%s', text)", kNote), sample.bytes(#text) .. ', the folder notes was created on the way')
end

-- The storage API writes whole files, so appending reads the file and writes it back longer, still atomically.
function Storage:append()
    self.lines = self.lines + 1
    local old = storage.exists(kNote) and storage.read(kNote) or ''
    storage.write(kNote, old .. string.format('%s line %d\n', os.date('%H:%M:%S'), self.lines))
    self.activity:add(string.format("storage.write('%s', old .. line)", kNote), 'now ' .. sample.bytes(#storage.read(kNote)))
end

function Storage:read()
    if not storage.exists(kNote) then
        self.activity:add(string.format("storage.exists('%s') is false", kNote), 'write the note first')
        return
    end
    local text = storage.read(kNote)
    self.activity:add(string.format("storage.read('%s')", kNote), sample.bytes(#text) .. ': ' .. text:gsub('\n', ' | '))
end

function Storage:writeJson()
    local profile = {name = 'Ana', coins = math.random(10, 999), visits = (storage.exists(kProfile) and storage.readJson(kProfile).visits or 0) + 1}
    storage.writeJson(kProfile, profile)
    self.activity:add(string.format("storage.writeJson('%s', profile)", kProfile), string.format('%d coins, visit %d', profile.coins, profile.visits))
end

function Storage:readJson()
    if not storage.exists(kProfile) then
        self.activity:add(string.format("storage.exists('%s') is false", kProfile), 'write the JSON first')
        return
    end
    local profile = storage.readJson(kProfile)
    self.activity:add(string.format("storage.readJson('%s')", kProfile), string.format('name %s, %d coins, %d visits', profile.name, profile.coins, profile.visits))
end

function Storage:exists()
    self.activity:add(string.format("storage.exists('%s') is %s", kNote, tostring(storage.exists(kNote))))
    self.activity:add("storage.exists('notes') is " .. tostring(storage.exists('notes')), 'folders are not files')
end

function Storage:missing()
    local ok, failure = pcall(storage.read, 'notes/missing.txt')
    self.activity:add("storage.read('notes/missing.txt') raised", ok and 'nothing' or failure)
end

-- Paths cannot leave the folder of the app, so another app's files stay out of reach.
function Storage:outside()
    local ok, failure = pcall(storage.write, '../other-app/secret.txt', 'hello')
    self.activity:add("storage.write('../other-app/secret.txt') raised", ok and 'nothing' or failure)
end

function Storage:remove()
    self.activity:add(string.format("storage.remove('%s') returned %s", kNote, tostring(storage.remove(kNote))))
end

function Storage:clear()
    local count = 0
    for _, folder in ipairs(kFolders) do
        for _, path in ipairs(storage.list(folder)) do
            count = count + (storage.remove(path) and 1 or 0)
        end
    end
    storage.flush()
    self.activity:add(string.format('Removed %d files of notes and profile', count), 'then storage.flush()')
end

return Storage
