-- The function `storage.writeSlot` keeps the data of a game with a summary and the time, `storage.slotInfo` and `storage.listSlots` feed a load menu, and `storage.readSlot` and `storage.removeSlot` load and delete. The game also saves itself to `auto_save` when the test closes.
local haylen = require('haylen')
local storage = require('haylen.storage')
local ui = require('haylen.ui')

local Test = require('harness.test')
local readout = require('categories.files.readout')

local SaveSlots = haylen.class('SaveSlots', Test)

SaveSlots.slots = {'slot-1', 'slot-2', 'slot-3', 'auto_save'}
SaveSlots.places = {'Beach', 'Forest', 'Cave', 'Lighthouse'}

function SaveSlots:init(entry)
    SaveSlots.super.init(self, entry)
    local ok, saved = pcall(storage.readSlot, 'auto_save')
    self.game = ok and saved or {day = 1, wood = 0, place = 1, inventory = {'axe'}}
    self.selected = SaveSlots.slots[1]
end

function SaveSlots:enter()
    self:frame{
        hint = 'Change the game, pick a slot in the table and save, load or delete it. Damage a slot to see how a broken file is reported.',
        focus = 'day',
        content = {ui.row{grow = 1, gap = 24,
            ui.panel{width = 460, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'The game'},
                ui.label{id = 'game', text = ''},
                ui.button{id = 'day', text = 'Sleep until tomorrow', align = 'stretch', onClick = function()
                    self.game.day = self.game.day + 1
                    self:showGame()
                end},
                ui.button{id = 'wood', text = 'Chop wood', align = 'stretch', onClick = function()
                    self.game.wood = self.game.wood + 3
                    self:showGame()
                end},
                ui.button{id = 'travel', text = 'Travel', align = 'stretch', onClick = function()
                    self.game.place = self.game.place % #SaveSlots.places + 1
                    self:showGame()
                end},
            },
            ui.panel{grow = 1, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'Slots'},
                ui.table{id = 'slots', columns = {{text = 'Slot', width = 200}, {text = 'Place'}, {text = 'Day', width = 110, align = 'end'}, {text = 'Wood', width = 110, align = 'end'}, {text = 'Saved', width = 330}}, rows = {}, selected = self.selected, onSelect = function(event)
                    self.selected = event.item
                    self:refresh()
                end},
                ui.label{id = 'newest', text = '', color = 'textMuted'},
                ui.alert{id = 'problem', tone = 'danger', title = 'The slot could not be read', message = '', visible = false},
                ui.row{gap = 12,
                    ui.button{id = 'save', text = 'Save', variant = 'primary', onClick = function()
                        self:save()
                    end},
                    ui.button{id = 'load', text = 'Load', onClick = function()
                        self:loadSlot()
                    end},
                    ui.button{id = 'delete', text = 'Delete', variant = 'destructive', onClick = function(event)
                        event.gui:set('confirm', {open = true, message = 'The slot "' .. self.selected .. '" will be gone for good.'})
                    end},
                    ui.button{id = 'damage', text = 'Damage the file', onClick = function()
                        storage.writeText('saves/' .. self.selected .. '.json', '{"data": ')
                        self:refresh()
                    end},
                },
            },
            ui.dialog{id = 'confirm', title = 'Delete the slot?', buttons = {{id = 'keep', text = 'Keep it'}, {id = 'delete', text = 'Delete', variant = 'destructive'}}, onAnswer = function(event)
                if event.button == 'delete' then
                    self:delete()
                end
            end},
        }},
    }
    self:showGame()
    self:refresh()
end

-- Leaving the test saves the game to `auto_save`, which the test reads back the next time it opens.
function SaveSlots:exit()
    storage.writeSlot('auto_save', self.game, self:summary())
    SaveSlots.super.exit(self)
end

function SaveSlots:report(text)
    self:set('status', {text = text})
end

function SaveSlots:summary()
    return {day = self.game.day, wood = self.game.wood, place = SaveSlots.places[self.game.place]}
end

function SaveSlots:showGame()
    local game = self.game
    self:set('game', {text = string.format('Day %d at the %s\nWood %d, carrying %s', game.day, SaveSlots.places[game.place], game.wood, table.concat(game.inventory, ', '))})
end

-- Fills the table from `storage.slotInfo`, which raises for a damaged file, so every slot is read on its own and a broken one shows its error.
function SaveSlots:refresh()
    local rows = {}
    local problem
    for index, slot in ipairs(SaveSlots.slots) do
        local ok, info = pcall(storage.slotInfo, slot)
        if not ok then
            rows[index] = {id = slot, cells = {slot, 'Damaged', '', '', ''}}
            problem = slot == self.selected and info or problem
        elseif info then
            rows[index] = {id = slot, cells = {slot, info.summary.place, info.summary.day, info.summary.wood, readout.time(info.savedAt)}}
        else
            rows[index] = {id = slot, cells = {slot, 'Empty', '', '', ''}}
        end
    end
    self:set('slots', {rows = rows, selected = self.selected})
    self:set('problem', {visible = problem ~= nil, message = problem or ''})

    local ok, slots = pcall(storage.listSlots)
    if not ok then
        self:set('newest', {text = 'The call "storage.listSlots()" raised: ' .. slots})
        return
    end
    local names = {}
    for index, info in ipairs(slots) do
        names[index] = info.slot
    end
    local listed = #names > 0 and ', newest first: "' .. table.concat(names, '", "') .. '".' or '.'
    self:set('newest', {text = string.format('The call "storage.listSlots()" returns %d slots%s', #slots, listed)})
end

function SaveSlots:save()
    storage.writeSlot(self.selected, self.game, self:summary())
    self:report(string.format('Called "storage.writeSlot(\'%s\', game, summary)".', self.selected))
    self:refresh()
end

function SaveSlots:loadSlot()
    local ok, game = pcall(storage.readSlot, self.selected)
    if not ok then
        self:report('The call "storage.readSlot" raised: ' .. game)
        return
    end
    if not game then
        self:report('The call "storage.readSlot" returned "nil", so the slot is empty.')
        return
    end
    self.game = game
    self:showGame()
    self:report(string.format('The call "storage.readSlot(\'%s\')" loaded the game.', self.selected))
end

function SaveSlots:delete()
    self:report(string.format('The call "storage.removeSlot(\'%s\')" returned "%s".', self.selected, tostring(storage.removeSlot(self.selected))))
    self:refresh()
end

return SaveSlots
