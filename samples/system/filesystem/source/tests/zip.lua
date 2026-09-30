-- Zip archives with Varn `zip`: `zip.create` packs files of the user folder, `zip.list` reads the names of the entries and `zip.extract` unpacks them into a folder. A package file cannot be opened in place, so the archive shipped in the package is first written to the user folder.
local assets = require('haylen.assets')
local fs = require('fs')
local haylen = require('haylen')
local storage = require('haylen.storage')
local ui = require('haylen.ui')
local zip = require('zip')

local Activity = require('activity')
local sample = require('sample')

local Zip = haylen.class('Zip', sample.Test)

Zip.hints = 'Write the files, create the archive, list it and extract it, or unpack the postcards shipped in the package. Every step is awaited in a task of the scene.'
Zip.focus = 'files'

local kFiles = {
    ['readme.txt'] = 'An archive made by the Haylen Filesystem sample.\n',
    ['data/scores.json'] = '{"best": 4200, "runs": [1200, 3100, 4200]}\n',
    ['data/map.txt'] = '~~~..~~\n~.\"\".~\n~..^^.~\n~~~..~~\n',
}

-- Waits for a promise and raises its error, so one `pcall` around a step catches every failure.
local function need(promise)
    local value, failure = promise:await()
    if value == nil then
        error(failure, 0)
    end
    return value
end

function Zip:init(entry)
    Zip.super.init(self, entry)
    self.activity = Activity(self, 'activity')
    self.base = storage.root() .. '/zip-demo'
    self.archive = self.base .. '/bundle.zip'
end

function Zip:content()
    local actions = {
        {id = 'files', text = 'Write the files', run = self.writeFiles},
        {id = 'create', text = 'Create the archive', run = self.create},
        {id = 'list', text = 'List the archive', run = self.list},
        {id = 'extract', text = 'Extract the archive', run = self.extract},
        {id = 'postcards', text = 'Unpack the postcards', run = self.postcards},
        {id = 'remove', text = 'Remove everything', run = self.remove},
    }
    local buttons = {}
    for index, action in ipairs(actions) do
        buttons[index] = ui.button{id = action.id, text = action.text, align = 'stretch', onClick = function()
            self:run(action.text, action.run)
        end}
    end
    return {
        ui.panel{width = 460, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'Varn zip'},
            ui.column{gap = 12, children = buttons},
            ui.label{text = 'Folder: ' .. self.base, font = 'caption', color = 'textMuted'},
        },
        ui.panel{width = 560, align = 'stretch', gap = 12,
            ui.sectionTitle{id = 'entriesTitle', text = 'Entries'},
            ui.scroll{grow = 1, focusable = false, ui.list{id = 'entries', items = {}}},
            ui.label{id = 'preview', text = '', font = 'monospace', color = 'accentText'},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'Activity'},
            self.activity:node(),
        },
    }
end

function Zip:run(title, step)
    self:spawn(function()
        local ok, result = pcall(step, self)
        self.activity:add(ok and result or title .. ' failed', not ok and tostring(result) or nil)
    end)
end

-- Shows the names an archive holds, or the files a folder holds, in the middle panel.
function Zip:showEntries(title, names)
    local items = {}
    for index, name in ipairs(names) do
        items[index] = {id = 'entry-' .. index, text = name}
    end
    self:show('entriesTitle', {text = title})
    self:show('entries', {items = items})
end

function Zip:writeFiles()
    for name, text in pairs(kFiles) do
        storage.writeText('zip-demo/files/' .. name, text)
    end
    return string.format('Wrote %d files under "zip-demo/files" with "haylen.storage"', #storage.list('zip-demo/files'))
end

function Zip:create()
    local entries = {}
    for _, path in ipairs(storage.list('zip-demo/files')) do
        entries[#entries + 1] = {file = storage.root() .. '/' .. path, entry = path:sub(#'zip-demo/files/' + 1)}
    end
    if #entries == 0 then
        error('There are no files to pack yet. Write the files first.', 0)
    end
    need(zip.create(self.archive, entries))
    local info = need(fs.stat(self.archive))
    return string.format('zip.create packed %d entries into bundle.zip, %s', #entries, sample.bytes(info.size))
end

function Zip:list()
    local names = need(zip.list(self.archive))
    self:showEntries('zip.list(bundle.zip)', names)
    return string.format('zip.list found %d entries', #names)
end

function Zip:extract()
    need(zip.extract(self.archive, self.base .. '/extracted'))
    local files = storage.list('zip-demo/extracted')
    self:showEntries('Extracted files', files)
    self:show('preview', {text = storage.readText('zip-demo/extracted/data/map.txt')})
    return string.format('zip.extract unpacked %d files into zip-demo/extracted', #files)
end

function Zip:postcards()
    local archive = self.base .. '/postcards.zip'
    need(fs.mkdir(self.base))
    need(fs.writeFile(archive, assets.bytes('archives/postcards.zip')))
    local names = need(zip.list(archive))
    need(zip.extract(archive, self.base .. '/postcards'))
    self:showEntries('zip.list(postcards.zip)', names)
    self:show('preview', {text = storage.readText('zip-demo/postcards/postcards/palm-cove.txt')})
    return string.format('Copied "archives/postcards.zip" from the package and extracted its %d entries', #names)
end

function Zip:remove()
    need(fs.removeRecursive(self.base))
    self:showEntries('Entries', {})
    self:show('preview', {text = ''})
    return 'fs.removeRecursive removed zip-demo'
end

return Zip
