-- Package files: `assets.list` finds what the content folder holds, and `assets.text`, `assets.json` and `assets.bytes` read a file as text, as Lua values or as raw bytes, the same way from a folder, a zip file or the browser.
local assets = require('haylen.assets')
local haylen = require('haylen')
local json = require('json')
local ui = require('haylen.ui')

local sample = require('sample')

local Package = haylen.class('Package', sample.Test)

Package.hints = 'Pick a file with the mouse, a finger, the arrows or the directional pad and Enter or A. The last file is missing on purpose.'
Package.focus = 'files'

-- The file that is not in the package, which shows the error a missing file raises.
local kMissing = 'data/missing.txt'

-- The tiles of the level format, one character per tile kind.
local kTiles = {[0] = '~', '.', '"', '^'}

-- Reads the custom level format: the magic `HLVL`, a version, the width and height, then one byte per tile.
local function readLevel(data)
    local magic, version, width, height, offset = string.unpack('<c4I2I2I2', data)
    if magic ~= 'HLVL' then
        error('The file "data/level.bin" is not a level file.')
    end
    local rows = {}
    for y = 0, height - 1 do
        local row = {}
        for x = 1, width do
            row[x] = kTiles[data:byte(offset + y * width + x - 1)]
        end
        rows[#rows + 1] = table.concat(row, ' ')
    end
    return string.format('%s version %d, %d by %d tiles\n\n%s\n\n%s', magic, version, width, height, table.concat(rows, '\n'), sample.hex(data, 64))
end

-- How each kind of file is read, by extension: the call it shows and the text of its preview.
local kReaders = {
    txt = {call = "assets.text('%s')", read = function(path)
        return assets.text(path)
    end},
    json = {call = "assets.json('%s')", read = function(path)
        return json.encode(assets.json(path), {pretty = true})
    end},
    bin = {call = "assets.bytes('%s')", read = function(path)
        return readLevel(assets.bytes(path))
    end},
}

function Package:init(entry)
    Package.super.init(self, entry)
    self.paths = assets.list('data')
    self.paths[#self.paths + 1] = kMissing
end

function Package:content()
    local items = {}
    for index, path in ipairs(self.paths) do
        local caption = assets.exists(path) and sample.bytes(#assets.bytes(path)) or 'Not in the package'
        items[index] = {id = path, text = path, caption = caption}
    end
    return {
        ui.panel{width = 560, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'content/data'},
            ui.label{text = string.format('The call "assets.list(\'data\')" found %d files.', #self.paths - 1), color = 'textMuted'},
            ui.list{id = 'files', items = items, selected = self.paths[1], onSelect = function(event)
                self:open(event.item)
            end},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.label{id = 'call', text = '', font = 'monospace', color = 'accentText'},
            ui.label{id = 'summary', text = '', color = 'textMuted'},
            ui.scroll{grow = 1, ui.label{id = 'preview', text = '', font = 'monospace'}},
        },
    }
end

function Package:enter()
    Package.super.enter(self)
    self:open(self.paths[1])
end

-- Reads the file with the reader of its extension and shows the call, what it returned and its preview, or the error of a missing file.
function Package:open(path)
    local reader = kReaders[path:match('%.(%w+)$')]
    local ok, result = pcall(reader.read, path)
    self:show('call', {text = string.format(reader.call, path)})
    if ok then
        self:show('summary', {text = string.format('Read %s at "%s".', sample.bytes(#assets.bytes(path)), 'content/' .. path), color = 'textMuted'})
        self:show('preview', {text = result})
    else
        self:show('summary', {text = 'The call raised an error.', color = 'dangerText'})
        self:show('preview', {text = result})
    end
end

return Package
