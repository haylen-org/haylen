-- The user folder with Varn `fs`: every call runs on the I/O pool and returns a promise, which a task of the scene awaits, so the frame never waits for the disk and nothing resumes once the player left.
local fs = require('fs')
local haylen = require('haylen')
local storage = require('haylen.storage')
local ui = require('haylen.ui')

local Activity = require('activity')
local sample = require('sample')

local Fs = haylen.class('Fs', sample.Test)

Fs.hints = 'Press the buttons with the mouse, a finger, or the arrows and Enter or A. Each result shows how many frames the operation took.'
Fs.focus = 'mkdir'

local kChunk = 64 * 1024
local kChunks = 32

-- Waits for a promise and raises its error, so one `pcall` around an operation catches every failure of its steps.
local function need(promise)
    local value, failure = promise:await()
    if value == nil then
        error(failure, 0)
    end
    return value
end

function Fs:init(entry)
    Fs.super.init(self, entry)
    self.activity = Activity(self, 'activity')
    self.base = storage.root() .. '/fs-demo'
    self.log = self.base .. '/logs/run.txt'
end

function Fs:content()
    local actions = {
        {id = 'mkdir', text = 'Create folders', run = self.mkdir},
        {id = 'writeFile', text = 'Write a file', run = self.writeFile},
        {id = 'append', text = 'Append a line', run = self.append},
        {id = 'readFile', text = 'Read the file', run = self.readFile},
        {id = 'stat', text = 'Stat the file', run = self.stat},
        {id = 'readdir', text = 'List the folders', run = self.readdir},
        {id = 'copy', text = 'Copy and rename', run = self.copy},
        {id = 'stream', text = 'Stream 2 MB', run = self.stream},
        {id = 'missing', text = 'Read a missing file', run = self.missing},
        {id = 'remove', text = 'Remove everything', run = self.remove},
    }
    local buttons = {}
    for index, action in ipairs(actions) do
        buttons[index] = ui.button{id = action.id, text = action.text, align = 'stretch', onClick = function()
            self:run(action.text, action.run)
        end}
    end
    return {
        ui.panel{width = 560, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'Varn module "fs"'},
            ui.grid{columns = 2, gap = 12, children = buttons},
            ui.progress{id = 'progress', value = 0, text = 'No stream yet'},
            ui.label{text = 'Paths are built from "storage.root()": ' .. self.base, font = 'caption', color = 'textMuted'},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'Activity'},
            self.activity:node(),
        },
    }
end

-- Runs an operation as a task of the scene and reports its result, or its error, with the frames it took.
function Fs:run(title, operation)
    self:spawn(function()
        local started = haylen.frameIndex()
        local ok, result = pcall(operation, self)
        local frames = haylen.frameIndex() - started
        local timing = string.format('after %d frame%s', frames, frames == 1 and '' or 's')
        if ok then
            self.activity:add(result, timing)
        else
            self.activity:add(title .. ' failed: ' .. tostring(result), timing)
        end
    end)
end

function Fs:mkdir()
    need(fs.mkdir(self.base .. '/logs/archive'))
    return 'The call "fs.mkdir(root .. \'/fs-demo/logs/archive\')" created every missing folder'
end

function Fs:writeFile()
    need(fs.mkdir(self.base .. '/logs'))
    need(fs.writeFile(self.log, string.format('Run started %s\n', os.date('%H:%M:%S'))))
    return 'Called "fs.writeFile(root .. \'/fs-demo/logs/run.txt\', text)"'
end

function Fs:append()
    need(fs.append(self.log, string.format('%s all is well\n', os.date('%H:%M:%S'))))
    return 'Called "fs.append(root .. \'/fs-demo/logs/run.txt\', line)"'
end

function Fs:readFile()
    local text = need(fs.readFile(self.log))
    return string.format('The call "fs.readFile" read %s: %s', sample.bytes(#text), text:gsub('\n', ' | '):sub(1, 90))
end

function Fs:stat()
    local info = need(fs.stat(self.log))
    return string.format('The call "fs.stat" gave %s, file %s, changed %s', sample.bytes(info.size), tostring(info.isFile), sample.time(info.mtime))
end

function Fs:readdir()
    local names = need(fs.readdir(self.base))
    table.sort(names)
    local logs = fs.exists(self.base .. '/logs') and need(fs.readdir(self.base .. '/logs')) or {}
    table.sort(logs)
    return string.format('The call "fs.readdir" says "fs-demo" holds %s, "logs" holds %s', table.concat(names, ', '), #logs > 0 and table.concat(logs, ', ') or 'nothing')
end

function Fs:copy()
    local copy = self.base .. '/logs/run-copy.txt'
    need(fs.copy(self.log, copy))
    need(fs.rename(copy, self.base .. '/logs/archive/run-' .. os.date('%H%M%S') .. '.txt'))
    return 'Called "fs.copy" to "run-copy.txt", then "fs.rename" into "logs/archive"'
end

-- Writes a large file through an open handle one chunk at a time and reads it back the same way, with the bar showing how far each pass got.
function Fs:stream()
    local path = self.base .. '/stream.bin'
    need(fs.mkdir(self.base))
    local writer = need(fs.open(path, 'w'))
    local chunk = string.rep(string.char(0x48, 0x61, 0x79, 0x6C, 0x65, 0x6E, 0x21, 0x0A), kChunk // 8)
    for index = 1, kChunks do
        need(writer:write(chunk))
        self:show('progress', {value = index / kChunks / 2, text = 'Writing ' .. sample.bytes(index * kChunk)})
    end
    need(writer:close())

    local reader = need(fs.open(path, 'r'))
    local total = 0
    repeat
        local piece = need(reader:read(kChunk))
        total = total + #piece
        self:show('progress', {value = 0.5 + total / (kChunk * kChunks) / 2, text = 'Reading ' .. sample.bytes(total)})
    until piece == ''
    need(reader:close())
    return string.format('Streamed %s out and %s back in chunks of %s', sample.bytes(kChunk * kChunks), sample.bytes(total), sample.bytes(kChunk))
end

function Fs:missing()
    local _, message = fs.readFile(self.base .. '/missing.txt'):await()
    return 'The call "fs.readFile" of a missing file resolved with "nil" and: ' .. tostring(message)
end

function Fs:remove()
    need(fs.removeRecursive(self.base))
    self:show('progress', {value = 0, text = 'No stream yet'})
    return string.format('The call "fs.removeRecursive" removed "fs-demo", "fs.exists" is now %s', tostring(fs.exists(self.base)))
end

return Fs
