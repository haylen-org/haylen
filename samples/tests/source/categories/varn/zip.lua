-- Archives with Varn's "zip" module, inside the storage folder of the app: packing files, listing their entries with their sizes and extracting them, and the names and archives it refuses because an entry would leave its folder, which it checks before it writes anything.
local assets = require('haylen.assets')
local fs = require('fs')
local haylen = require('haylen')
local storage = require('haylen.storage')
local zip = require('zip')

local VarnTest = require('categories.varn.varn-test')

local Zip = haylen.class('Zip', VarnTest)

Zip.hint = 'The archives live in a folder of the storage folder of the app, which the test removes when it ends. Run again repeats the lesson.'

Zip.excerpts = {
    {'Pack and unpack', [[
local pack = folder .. '/pack.zip'
zip.create(pack, {
    {file = folder .. '/map.json', entry = 'levels/map.json'},
    {file = folder .. '/notes.txt', entry = 'notes.txt'},
}):await()
for _, entry in ipairs(zip.list(pack):await()) do
    print(entry.name, entry.size, entry.compressedSize)
end
fs.mkdir(folder .. '/out'):await()
zip.extract(pack, folder .. '/out'):await()]]},
    {'Unsafe archives', [[
local bytes = assets.bytes('varn/unsafe_archive.zip')
fs.writeFile(folder .. '/unsafe.zip', bytes):await()
for _, entry in ipairs(zip.list(folder .. '/unsafe.zip'):await()) do
    print(entry.name, entry.unsafe)
end
local done, failure = zip.extract(
    folder .. '/unsafe.zip', folder .. '/trap'):await()]]},
}

function Zip:run()
    local folder, failure = fs.mkdtemp(storage.root() .. '/varn-'):await()
    if not folder then
        self:check('create', 'Pack', false, 'The folder could not be made: ' .. tostring(failure))
        return
    end
    local cleanup <close> = setmetatable({}, {__close = function()
        fs.removeRecursive(folder)
    end})

    local map = string.rep('[0, 0, 1, 1, 2, 2, 1, 0],\n', 1000)
    local notes = 'Bring the map to the cove.\n'
    fs.writeFile(folder .. '/map.json', map):await()
    fs.writeFile(folder .. '/notes.txt', notes):await()
    self:pack(folder, map, notes)
    self:unsafeName(folder)
    self:unsafeArchive(folder)

    local listed, reason = zip.list(folder .. '/missing.zip'):await()
    self:check('missing', 'A missing archive', listed == nil and reason ~= nil, 'The function "zip.list" returned nil and the reason: ' .. tostring(reason))
end

function Zip:pack(folder, map, notes)
    local pack = folder .. '/pack.zip'
    local done, failure = zip.create(pack, {
        {file = folder .. '/map.json', entry = 'levels/map.json'},
        {file = folder .. '/notes.txt', entry = 'notes.txt'},
    }):await()
    if not done then
        self:check('create', 'Pack', false, 'The archive could not be made: ' .. tostring(failure))
        return
    end
    local size = fs.stat(pack):await().size
    self:check('create', 'Pack', size < #map, string.format('Packed 2 files of %d bytes into an archive of %d bytes, since the repeated rows of the map compress well.', #map + #notes, size))

    local entries = zip.list(pack):await()
    local names, sizes = {}, {}
    for index, entry in ipairs(entries) do
        names[index] = entry.name
        sizes[entry.name] = entry
    end
    table.sort(names)
    local listing = table.concat(names, ', ')
    local listed = sizes['levels/map.json'] or {}
    self:check('list', 'List', listing == 'levels/map.json, notes.txt' and listed.size == #map and listed.compressedSize < listed.size, string.format('The archive holds "%s", and "levels/map.json" takes %s of its %s bytes.', listing, tostring(listed.compressedSize), tostring(listed.size)))

    fs.mkdir(folder .. '/out'):await()
    zip.extract(pack, folder .. '/out'):await()
    local again = fs.readFile(folder .. '/out/levels/map.json'):await()
    self:check('extract', 'Extract', again == map, 'The extracted "levels/map.json" matches the original byte for byte, in the folder its entry names.')
end

function Zip:unsafeName(folder)
    local done, failure = zip.create(folder .. '/bad.zip', {
        {file = folder .. '/notes.txt', entry = '../escape.txt'},
    }):await()
    self:check('name', 'Unsafe names', done == nil and failure ~= nil, 'The function "zip.create" refused the entry "../escape.txt": ' .. tostring(failure))
end

-- An archive of the package lives inside the package, so the test copies its bytes to the storage folder, where the files of "zip" live. The listing marks the entry that extracting refuses, and the refusal comes before any entry is written, the safe one before it included.
function Zip:unsafeArchive(folder)
    local bytes = assets.bytes('varn/unsafe_archive.zip')
    fs.writeFile(folder .. '/unsafe.zip', bytes):await()
    local unsafe = {}
    for _, entry in ipairs(zip.list(folder .. '/unsafe.zip'):await() or {}) do
        if entry.unsafe then
            unsafe[#unsafe + 1] = entry.name
        end
    end
    fs.mkdir(folder .. '/trap'):await()
    local done, failure = zip.extract(
        folder .. '/unsafe.zip', folder .. '/trap'):await()
    local untouched = not fs.exists(folder .. '/escape.txt') and not fs.exists(folder .. '/trap/notes.txt')
    self:check('archive', 'Unsafe archives', done == nil and untouched and table.concat(unsafe, ', ') == '../escape.txt', string.format('The listing marked "%s" as unsafe, extracting refused the archive, and nothing was written: %s', table.concat(unsafe, ', '), tostring(failure)))
end

return Zip
