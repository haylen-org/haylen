-- The function `assets.list` finds what the content folder holds, and `assets.text`, `assets.json` and `assets.bytes` read a file as text, as Lua values or as raw bytes, the same way from a folder, a zip file or the browser.
local assets = require('haylen.assets')
local haylen = require('haylen')
local json = require('json')
local ui = require('haylen.ui')

local Test = require('harness.test')
local readout = require('categories.files.readout')

local Package = haylen.class('Package', Test)

Package.folder = 'files/data'

-- The file that is not in the package, which shows the error a missing file raises.
Package.missing = 'files/data/missing.txt'

-- The tiles of the level format, one character per tile kind.
Package.tiles = {[0] = '~', '.', '"', '^'}

-- Reads the custom level format: the magic `HLVL`, a version, the width and height, then one byte per tile.
local function readLevel(path)
    local data = assets.bytes(path)
    local magic, version, width, height, offset = string.unpack('<c4I2I2I2', data)
    if magic ~= 'HLVL' then
        error('The file "' .. path .. '" is not a level file.')
    end
    local rows = {}
    for y = 0, height - 1 do
        local row = {}
        for x = 1, width do
            row[x] = Package.tiles[data:byte(offset + y * width + x - 1)]
        end
        rows[#rows + 1] = table.concat(row, ' ')
    end
    return string.format('Magic "%s", version %d, %d by %d tiles\n\n%s\n\n%s', magic, version, width, height, table.concat(rows, '\n'), readout.hex(data, 64))
end

-- How each kind of file is read, by extension: the call it shows and the text of its preview.
Package.readers = {
    txt = {call = "assets.text('%s')", read = function(path)
        return assets.text(path)
    end},
    json = {call = "assets.json('%s')", read = function(path)
        return json.encode(assets.json(path), {pretty = true})
    end},
    bin = {call = "assets.bytes('%s')", read = readLevel},
}

function Package:init(entry)
    Package.super.init(self, entry)
    self.paths = assets.list(Package.folder)
    self.paths[#self.paths + 1] = Package.missing
end

function Package:enter()
    local items = {}
    for index, path in ipairs(self.paths) do
        local caption = assets.exists(path) and readout.bytes(#assets.bytes(path)) or 'Not in the package'
        items[index] = {id = path, text = path, caption = caption}
    end
    self:frame{
        hint = 'Pick a file with the mouse, a finger, the arrows or the directional pad and Enter or the south button. The last file is missing on purpose.',
        focus = 'files',
        content = {ui.row{grow = 1, gap = 24,
            ui.panel{width = 560, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'The folder "content/' .. Package.folder .. '"'},
                ui.label{text = string.format('The call "assets.list(\'%s\')" found %d files.', Package.folder, #self.paths - 1), color = 'textMuted'},
                ui.list{id = 'files', items = items, selected = self.paths[1], onSelect = function(event)
                    self:open(event.item)
                end},
            },
            ui.panel{grow = 1, align = 'stretch', gap = 12,
                ui.label{id = 'call', text = '', font = 'monospace', color = 'accentText'},
                ui.label{id = 'summary', text = '', color = 'textMuted'},
                ui.scroll{grow = 1, ui.label{id = 'preview', text = '', font = 'monospace'}},
            },
        }},
    }
    self:open(self.paths[1])
end

-- Reads the file with the reader of its extension and shows the call, what it returned and its preview, or the error of a missing file.
function Package:open(path)
    local reader = Package.readers[path:match('%.(%w+)$')]
    local ok, result = pcall(reader.read, path)
    self:set('call', {text = string.format(reader.call, path)})
    if ok then
        self:set('summary', {text = string.format('Read %s at "content/%s".', readout.bytes(#assets.bytes(path)), path), color = 'textMuted'})
    else
        self:set('summary', {text = 'The call raised an error.', color = 'dangerText'})
    end
    self:set('preview', {text = result})
end

return Package
