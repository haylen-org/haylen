-- Files with Varn's "fs" module, always inside the storage folder of the app: a private folder, writes, appends, reads, a streaming handle, copies, renames, listings and removal, all on the I/O pool.
local datetime = require('datetime')
local fs = require('fs')
local haylen = require('haylen')
local storage = require('haylen.storage')

local VarnTest = require('categories.varn.varn-test')

local Files = haylen.class('Files', VarnTest)

Files.hint = 'Every path starts at "storage.root()", the folder the platform gives the app, and the test removes its folder when it ends. Run again repeats the lesson.'

Files.excerpts = {
    {'Write and read', [[
local root = storage.root()
local folder = fs.mkdtemp(root .. '/varn-'):await()
local notes = folder .. '/notes.txt'
fs.writeFile(notes, 'Day 1.\n'):await()
fs.append(notes, 'Day 2.\n'):await()
print(fs.readFile(notes):await())
print(fs.stat(notes):await().size)]]},
    {'Stream', [[
local file = fs.open(folder .. '/log.bin', 'w'):await()
for _, chunk in ipairs({'Tide ', 'rises ', 'twice.'}) do
    file:write(chunk):await()
end
file:close():await()]]},
    {'Organize', [[
fs.copy(notes, folder .. '/copy.txt'):await()
fs.rename(folder .. '/copy.txt', folder .. '/backup.txt'):await()
print(table.concat(fs.readdir(folder):await(), ', '))
fs.removeRecursive(folder):await()]]},
}

function Files:run()
    local root = storage.root()
    local folder, failure = fs.mkdtemp(root .. '/varn-'):await()
    if not folder then
        self:check('folder', 'A private folder', false, 'The folder could not be made: ' .. tostring(failure))
        return
    end
    local cleanup <close> = setmetatable({}, {__close = function()
        fs.removeRecursive(folder)
    end})
    local name = folder:sub(#root + 2)
    self:check('folder', 'A private folder', fs.exists(folder) and folder:sub(1, #root) == root, string.format('Made "%s" in the storage folder of the app with "fs.mkdtemp".', name))

    local notes = folder .. '/notes.txt'
    fs.writeFile(notes, 'Day 1.\n'):await()
    fs.append(notes, 'Day 2.\n'):await()
    local text = fs.readFile(notes):await()
    self:check('write', 'Write and append', text == 'Day 1.\nDay 2.\n', string.format('Read back "%s" after "fs.writeFile" and "fs.append".', (text or ''):gsub('\n', ' '):gsub(' $', '')))

    local info = fs.stat(notes):await()
    self:check('stat', 'Stat', info.size == #text and info.isFile and not info.isDir, string.format('A file of %d bytes, changed at %s.', info.size, datetime.fromUnix(info.mtime):iso()))

    self:stream(folder)

    fs.copy(notes, folder .. '/copy.txt'):await()
    fs.rename(folder .. '/copy.txt', folder .. '/backup.txt'):await()
    local names = fs.readdir(folder):await()
    table.sort(names)
    local listing = table.concat(names, ', ')
    self:check('organize', 'Copy, rename and list', listing == 'backup.txt, log.bin, notes.txt', string.format('The folder lists "%s".', listing))

    local same = storage.readText(name .. '/notes.txt')
    self:check('storage', 'The same folder as "haylen.storage"', same == text, 'The function "storage.readText" read the same notes, because both modules work in the storage folder.')

    local missing, reason = fs.readFile(folder .. '/missing.txt'):await()
    self:check('missing', 'A missing file', missing == nil and reason ~= nil, 'The function "fs.readFile" returned nil and the reason: ' .. tostring(reason))

    fs.removeRecursive(folder):await()
    self:check('remove', 'Remove', not fs.exists(folder), 'The folder and everything in it are gone after "fs.removeRecursive".')
end

-- Writes a file in chunks through a handle and reads it back a few bytes at a time, so a large file never sits in memory whole.
function Files:stream(folder)
    local path = folder .. '/log.bin'
    local file = fs.open(path, 'w'):await()
    for _, chunk in ipairs({'Tide ', 'rises ', 'twice.'}) do
        file:write(chunk):await()
    end
    file:close():await()

    local reader = fs.open(path, 'r'):await()
    local parts = {}
    while true do
        local piece = reader:read(4):await()
        if piece == '' then
            break
        end
        parts[#parts + 1] = piece
    end
    reader:close():await()
    local text = table.concat(parts)
    self:check('stream', 'A streaming handle', text == 'Tide rises twice.', string.format('Wrote 3 chunks through a handle and read "%s" back in %d reads of up to 4 bytes.', text, #parts))
end

return Files
