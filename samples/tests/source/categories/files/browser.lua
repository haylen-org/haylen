-- A file browser of the user folder: `fs.readdir` lists a folder, `fs.stat` tells folders from files with their sizes and dates, and `fs.open` reads the start of a file for the preview, all awaited in tasks of the scene.
local fs = require('fs')
local haylen = require('haylen')
local storage = require('haylen.storage')
local ui = require('haylen.ui')

local Test = require('harness.test')
local readout = require('categories.files.readout')

local Browser = haylen.class('Browser', Test)

Browser.up = '..'
Browser.previewBytes = 2048

Browser.examples = {
    ['browser-demo/readme.txt'] = 'Folders and files made by the file browser test.\n',
    ['browser-demo/levels/forest.json'] = '{"trees": 120, "wolves": 4}\n',
    ['browser-demo/levels/cave.json'] = '{"torches": 9, "bats": 30}\n',
    ['browser-demo/art/palette.bin'] = string.char(0x1B, 0x1E, 0x2B, 0x4C, 0x7D, 0xFF, 0xF2, 0xC1, 0x4E, 0x3D, 0xBE, 0x7A),
}

-- Waits for a promise and raises its error.
local function need(promise)
    local value, failure = promise:await()
    if value == nil then
        error(failure, 0)
    end
    return value
end

function Browser:init(entry)
    Browser.super.init(self, entry)
    self.folder = ''
    self.entries = {}
    self.selected = nil
    self.visit = 0
end

function Browser:enter()
    self:frame{
        hint = 'Pick a folder to open it and a file to preview it, with the mouse, a finger, the arrows or the directional pad. The first row goes up a folder.',
        focus = 'entries',
        content = {ui.row{grow = 1, gap = 24,
            ui.panel{width = 760, align = 'stretch', gap = 12,
                ui.label{id = 'path', text = '', font = 'monospace', color = 'accentText'},
                ui.scroll{grow = 1, ui.list{id = 'entries', items = {}, onSelect = function(event)
                    self:pick(event.item)
                end}},
                ui.row{gap = 12,
                    ui.button{id = 'newFolder', text = 'New folder', onClick = function()
                        self:create(true)
                    end},
                    ui.button{id = 'newFile', text = 'New file', onClick = function()
                        self:create(false)
                    end},
                    ui.button{id = 'delete', text = 'Delete', variant = 'destructive', onClick = function(event)
                        if self.selected then
                            event.gui:set('confirm', {open = true, message = 'The file "' .. self:relative(self.selected) .. '" will be removed.'})
                        end
                    end},
                    ui.button{id = 'examples', text = 'Add examples', onClick = function()
                        for path, data in pairs(Browser.examples) do
                            storage.writeText(path, data)
                        end
                        self:open(self.folder)
                    end},
                },
            },
            ui.panel{grow = 1, align = 'stretch', gap = 12,
                ui.sectionTitle{id = 'name', text = 'Nothing selected'},
                ui.label{id = 'details', text = 'Pick a file to see its size, its date and its first bytes.', color = 'textMuted'},
                ui.scroll{grow = 1, ui.label{id = 'preview', text = '', font = 'monospace'}},
            },
            ui.dialog{id = 'confirm', title = 'Delete it?', buttons = {{id = 'keep', text = 'Keep'}, {id = 'delete', text = 'Delete', variant = 'destructive'}}, onAnswer = function(event)
                if event.button == 'delete' then
                    self:remove(self.selected)
                end
            end},
        }},
    }
    self:open('')
end

-- Returns the path of an entry inside the user folder, such as `notes/today.txt`.
function Browser:relative(name)
    return self.folder == '' and name or self.folder .. '/' .. name
end

function Browser:absolute(name)
    local relative = name and self:relative(name) or self.folder
    return relative == '' and storage.root() or storage.root() .. '/' .. relative
end

-- Runs file work as a task of the scene and shows its error in place of the details when it fails.
function Browser:task(work)
    self:spawn(function()
        local ok, failure = pcall(work)
        if not ok then
            self:set('details', {text = tostring(failure), color = 'dangerText'})
        end
    end)
end

-- Lists a folder, folders first, and shows it. A newer visit makes the answer of an older one stale, so quick clicks never mix two folders. The user folder itself only exists once something was written, so the root is created first.
function Browser:open(folder)
    self.folder = folder
    self.visit = self.visit + 1
    local visit = self.visit
    self:task(function()
        if folder == '' then
            need(fs.mkdir(storage.root()))
        end
        local entries = {}
        for _, name in ipairs(need(fs.readdir(self:absolute()))) do
            entries[#entries + 1] = {name = name, info = need(fs.stat(self:absolute(name)))}
        end
        table.sort(entries, function(a, b)
            if a.info.isDir ~= b.info.isDir then
                return a.info.isDir
            end
            return a.name < b.name
        end)
        if visit ~= self.visit then
            return
        end
        self.entries = {}
        local items = folder ~= '' and {{id = Browser.up, text = Browser.up, caption = 'Up one folder'}} or {}
        for _, entry in ipairs(entries) do
            self.entries[entry.name] = entry.info
            items[#items + 1] = {id = entry.name, text = entry.info.isDir and entry.name .. '/' or entry.name, caption = entry.info.isDir and 'Folder' or readout.bytes(entry.info.size) .. ', ' .. readout.time(entry.info.mtime)}
        end
        self.selected = nil
        self:set('path', {text = 'User folder/' .. folder})
        self:set('entries', {items = items})
        self:set('name', {text = #entries == 0 and 'An empty folder' or 'Nothing selected'})
        self:set('details', {text = #entries == 0 and 'Add examples, a folder or a file with the buttons below the list.' or 'Pick a file to see its size, its date and its first bytes.', color = 'textMuted'})
        self:set('preview', {text = ''})
    end)
end

function Browser:pick(name)
    if name == Browser.up then
        self:open(self.folder:match('^(.*)/[^/]+$') or '')
        return
    end
    local info = self.entries[name]
    if info.isDir then
        self:open(self:relative(name))
        return
    end
    self.selected = name
    self:preview(name, info)
end

-- Reads only the start of the file through a handle, so a large file never loads whole for its preview.
function Browser:preview(name, info)
    self:task(function()
        local handle = need(fs.open(self:absolute(name), 'r'))
        local data = need(handle:read(Browser.previewBytes))
        need(handle:close())
        self:set('name', {text = 'File "' .. name .. '"'})
        self:set('details', {text = string.format('%s, changed %s\nPath "%s"', readout.bytes(info.size), readout.time(info.mtime), self:absolute(name)), color = 'textMuted'})
        self:set('preview', {text = readout.preview(data, Browser.previewBytes)})
    end)
end

function Browser:create(folder)
    local stamp = os.date('%H%M%S')
    self:task(function()
        if folder then
            need(fs.mkdir(self:absolute('folder-' .. stamp)))
        else
            need(fs.writeFile(self:absolute('note-' .. stamp .. '.txt'), 'Written by the file browser at ' .. os.date('%H:%M:%S') .. '\n'))
        end
        self:open(self.folder)
    end)
end

function Browser:remove(name)
    self:task(function()
        need(fs.removeRecursive(self:absolute(name)))
        self:open(self.folder)
    end)
end

return Browser
