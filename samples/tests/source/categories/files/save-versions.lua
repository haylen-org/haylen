-- Save versions: a slot written by version 1 of a game read by version 3 through two migrations, the version, revision and backup of every write, the export of a slot and its import as another device with and without newer progress, and the event of every write.
local haylen = require('haylen')
local storage = require('haylen.storage')
local ui = require('haylen.ui')

local Test = require('harness.test')

local SaveVersions = haylen.class('SaveVersions', Test)

local kSlot = 'versions'
local kCopy = 'versions-copy'

function SaveVersions:enter()
    self.lines = {}
    self:listen('saveSlotWritten', function(info)
        self:note(string.format('Event "saveSlotWritten" for "%s", revision %d, version %d.', info.slot, info.revision, info.version))
    end)
    self:frame{
        hint = 'Write a slot as version 1, then read it as version 3, damage it, and move it to another device.',
        content = {ui.row{wrap = true, grow = 1, gap = 24,
            ui.panel{width = 520, align = 'stretch', gap = 12,
                ui.button{id = 'old', text = 'Write as version 1', align = 'stretch', onClick = function() self:writeOld() end},
                ui.button{id = 'upgrade', text = 'Read as version 3', align = 'stretch', onClick = function() self:readUpgraded() end},
                ui.button{id = 'write', text = 'Write again as version 3', align = 'stretch', onClick = function() self:writeNew() end},
                ui.button{id = 'damage', text = 'Damage the file and read it', align = 'stretch', onClick = function() self:damage() end},
                ui.button{id = 'export', text = 'Import on another device', align = 'stretch', onClick = function() self:transfer() end},
                ui.label{font = 'monospace', text = "storage.setSaveVersion(3)\nstorage.addMigration(1, fn)\nstorage.exportSlot(slot)\nstorage.importSlot(slot, bytes)"},
            },
            ui.scroll{grow = 1, ui.label{id = 'log', text = '', grow = 1}},
        }},
    }
end

function SaveVersions:exit()
    SaveVersions.super.exit(self)
    storage.setSaveVersion(1)
    storage.removeSlot(kSlot)
    storage.removeSlot(kCopy)
end

function SaveVersions:note(line)
    table.insert(self.lines, 1, line)
    self.lines[13] = nil
    self:set('log', {text = table.concat(self.lines, '\n')})
end

function SaveVersions:describe(slot)
    local info = storage.slotInfo(slot)
    return info and string.format('version %d, revision %d%s', info.version, info.revision, info.recovered and ', recovered from the backup' or '') or 'empty'
end

function SaveVersions:writeOld()
    storage.setSaveVersion(1)
    storage.writeSlot(kSlot, {items = {'axe', 'rope'}, hp = 3}, {title = 'Version 1'})
    self:note('Wrote the slot as ' .. self:describe(kSlot) .. '.')
end

-- Version 2 renamed the items, and version 3 added stamina.
function SaveVersions:readUpgraded()
    storage.setSaveVersion(3)
    storage.addMigration(1, function(data)
        data.inventory, data.items = data.items, nil
        return data
    end)
    storage.addMigration(2, function(data)
        data.stamina = 100
        return data
    end)
    local ok, data = pcall(storage.readSlot, kSlot)
    if not ok or not data then
        self:note(ok and 'The slot is empty, so write it first.' or 'Reading raised: ' .. data)
        return
    end
    self:note(string.format('Read %s as version 3: inventory "%s", stamina %d.', self:describe(kSlot), table.concat(data.inventory or {}, '", "'), data.stamina or 0))
end

function SaveVersions:writeNew()
    storage.setSaveVersion(3)
    storage.writeSlot(kSlot, {inventory = {'axe'}, stamina = 80, hp = 3}, {title = 'Version 3'})
    self:note('Wrote the slot as ' .. self:describe(kSlot) .. '.')
end

function SaveVersions:damage()
    if not storage.slotExists(kSlot) then
        self:note('The slot is empty, so write it first.')
        return
    end
    storage.writeText('saves/' .. kSlot .. '.json', '{"data": ')
    local ok, result = pcall(storage.slotInfo, kSlot)
    self:note(ok and 'After the damage the slot reads as ' .. self:describe(kSlot) .. '.' or 'After the damage reading raised: ' .. result)
end

function SaveVersions:transfer()
    local bytes = storage.exportSlot(kSlot)
    if not bytes then
        self:note('The slot is empty, so write it first.')
        return
    end
    storage.removeSlot(kCopy)
    storage.writeSlot(kCopy, {inventory = {}, stamina = 1}, {title = 'Older'})
    local imported = storage.importSlot(kCopy, bytes)
    self:note(string.format('Exported %d bytes. The other device kept newer progress: %s. Now it holds %s.', #bytes, tostring(not imported), self:describe(kCopy)))
    imported = storage.importSlot(kCopy, bytes, {policy = 'replace'})
    self:note(string.format('With the policy "replace" it imported: %s, and holds %s.', tostring(imported), self:describe(kCopy)))
end

return SaveVersions
