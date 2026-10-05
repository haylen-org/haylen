-- The user folder with `haylen.storage`: every call runs at once and returns its result, writes replace files atomically and create missing folders, and paths stay inside the folder of the app.
local haylen = require('haylen')
local storage = require('haylen.storage')
local ui = require('haylen.ui')

local Test = require('harness.test')
local Activity = require('categories.files.activity')
local readout = require('categories.files.readout')

local Storage = haylen.class('Storage', Test)

Storage.note = 'notes/today.txt'
Storage.profile = 'profile/player.json'
Storage.folders = {'notes', 'profile'}

function Storage:init(entry)
    Storage.super.init(self, entry)
    self.activity = Activity(self, 'activity')
    self.lines = 0
end

function Storage:enter()
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
        {id = 'clear', text = 'Remove its files', run = self.clear},
    }
    local buttons = {}
    for index, action in ipairs(actions) do
        buttons[index] = ui.button{id = action.id, text = action.text, align = 'stretch', onClick = function()
            action.run(self)
            self:refresh()
        end}
    end
    self:frame{
        hint = 'Press the buttons with the mouse, a finger, or the arrows and Enter or the south button. Every call and its result goes to the activity on the right.',
        focus = 'write',
        content = {ui.row{grow = 1, gap = 24,
            ui.panel{width = 560, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'The module "haylen.storage"'},
                ui.grid{columns = 2, gap = 12, children = buttons},
                ui.label{text = 'The folder of the app is "' .. storage.root() .. '".', font = 'caption', color = 'textMuted'},
            },
            ui.panel{width = 520, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'Files of "notes" and "profile"'},
                ui.scroll{grow = 1, focusable = false, ui.list{id = 'files', items = {}}},
            },
            ui.panel{grow = 1, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'Activity'},
                self.activity:node(),
            },
        }},
    }
    self:refresh()
end

-- Lists the files of the folders of this test with their sizes, which `storage` learns by reading each file.
function Storage:refresh()
    local items = {}
    for _, folder in ipairs(Storage.folders) do
        for _, path in ipairs(storage.list(folder)) do
            items[#items + 1] = {id = path, text = path, caption = readout.bytes(#storage.readText(path))}
        end
    end
    self:set('files', {items = items})
end

function Storage:write()
    local text = string.format('Notes of %s\n', os.date('%Y-%m-%d'))
    storage.writeText(Storage.note, text)
    self.lines = 0
    self.activity:add(string.format('Called "storage.writeText(\'%s\', text)"', Storage.note), readout.bytes(#text) .. ', the folder "notes" was created on the way')
end

-- The storage API writes whole files, so appending reads the file and writes it back longer, still atomically.
function Storage:append()
    self.lines = self.lines + 1
    local old = storage.exists(Storage.note) and storage.readText(Storage.note) or ''
    storage.writeText(Storage.note, old .. string.format('%s line %d\n', os.date('%H:%M:%S'), self.lines))
    self.activity:add(string.format('Called "storage.writeText(\'%s\', old .. line)"', Storage.note), 'Now ' .. readout.bytes(#storage.readText(Storage.note)))
end

function Storage:read()
    if not storage.exists(Storage.note) then
        self.activity:add(string.format('The value of "storage.exists(\'%s\')" is "false"', Storage.note), 'Write the note first')
        return
    end
    local text = storage.readText(Storage.note)
    self.activity:add(string.format('Called "storage.readText(\'%s\')"', Storage.note), readout.bytes(#text) .. ': ' .. text:gsub('\n', ' | '))
end

function Storage:writeJson()
    local profile = {name = 'Ana', coins = math.random(10, 999), visits = (storage.exists(Storage.profile) and storage.readJson(Storage.profile).visits or 0) + 1}
    storage.writeJson(Storage.profile, profile)
    self.activity:add(string.format('Called "storage.writeJson(\'%s\', profile)"', Storage.profile), string.format('%d coins, visit %d', profile.coins, profile.visits))
end

function Storage:readJson()
    if not storage.exists(Storage.profile) then
        self.activity:add(string.format('The value of "storage.exists(\'%s\')" is "false"', Storage.profile), 'Write the JSON first')
        return
    end
    local profile = storage.readJson(Storage.profile)
    self.activity:add(string.format('Called "storage.readJson(\'%s\')"', Storage.profile), string.format('Name %s, %d coins, %d visits', profile.name, profile.coins, profile.visits))
end

function Storage:exists()
    self.activity:add(string.format('The value of "storage.exists(\'%s\')" is "%s"', Storage.note, tostring(storage.exists(Storage.note))))
    self.activity:add(string.format('The value of "storage.exists(\'notes\')" is "%s"', tostring(storage.exists('notes'))), 'Folders are not files')
end

function Storage:missing()
    local ok, failure = pcall(storage.readText, 'notes/missing.txt')
    self.activity:add('The call "storage.readText(\'notes/missing.txt\')" raised', ok and 'Nothing' or failure)
end

-- Paths cannot leave the folder of the app, so the files of other apps stay out of reach.
function Storage:outside()
    local ok, failure = pcall(storage.writeText, '../other-app/secret.txt', 'hello')
    self.activity:add('The call "storage.writeText(\'../other-app/secret.txt\', text)" raised', ok and 'Nothing' or failure)
end

function Storage:remove()
    self.activity:add(string.format('The call "storage.remove(\'%s\')" returned "%s"', Storage.note, tostring(storage.remove(Storage.note))))
end

function Storage:clear()
    local count = 0
    for _, folder in ipairs(Storage.folders) do
        for _, path in ipairs(storage.list(folder)) do
            count = count + (storage.remove(path) and 1 or 0)
        end
    end
    storage.flush()
    self.activity:add(string.format('Removed %d files of "notes" and "profile"', count), 'Then called "storage.flush()"')
end

return Storage
