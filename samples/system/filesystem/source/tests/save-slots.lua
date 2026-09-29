-- Save slots: storage.writeSlot keeps the data of a game with a summary and the time, storage.slotInfo and storage.listSlots feed a load menu, and storage.readSlot and storage.removeSlot load and delete. The game also saves itself to auto_save when the player leaves.
local haylen = require('haylen')
local storage = require('haylen.storage')
local ui = require('haylen.ui')

local sample = require('sample')

local SaveSlots = haylen.class('SaveSlots', sample.Test)

SaveSlots.hints = 'Change the game, pick a slot in the table and save, load or delete it. Damage a slot to see how a broken file is reported.'
SaveSlots.focus = 'day'

local kSlots = {'slot-1', 'slot-2', 'slot-3', 'auto_save'}
local kPlaces = {'Beach', 'Forest', 'Cave', 'Lighthouse'}

function SaveSlots:init(entry)
    SaveSlots.super.init(self, entry)
    local ok, saved = pcall(storage.readSlot, 'auto_save')
    self.game = ok and saved or {day = 1, wood = 0, place = 1, inventory = {'axe'}}
    self.selected = kSlots[1]
end

function SaveSlots:content()
    return {
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
                self.game.place = self.game.place % #kPlaces + 1
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
                    event.document:set('confirm', {open = true, message = 'The slot ' .. self.selected .. ' will be gone for good.'})
                end},
                ui.button{id = 'damage', text = 'Damage the file', onClick = function()
                    storage.write('saves/' .. self.selected .. '.json', '{"data": ')
                    self:refresh()
                end},
            },
            ui.label{id = 'status', text = '', color = 'accentText'},
        },
        ui.dialog{id = 'confirm', title = 'Delete the slot?', buttons = {{id = 'keep', text = 'Keep it'}, {id = 'delete', text = 'Delete', variant = 'destructive'}}, onAnswer = function(event)
            if event.button == 'delete' then
                self:delete()
            end
        end},
    }
end

function SaveSlots:enter()
    SaveSlots.super.enter(self)
    self:showGame()
    self:refresh()
end

-- Leaving the test saves the game to auto_save, which the test reads back the next time it opens.
function SaveSlots:exit()
    storage.writeSlot('auto_save', self.game, self:summary())
    SaveSlots.super.exit(self)
end

function SaveSlots:summary()
    return {day = self.game.day, wood = self.game.wood, place = kPlaces[self.game.place]}
end

function SaveSlots:showGame()
    local game = self.game
    self:show('game', {text = string.format('Day %d at the %s\n%d wood, carrying %s', game.day, kPlaces[game.place], game.wood, table.concat(game.inventory, ', '))})
end

-- Fills the table from storage.slotInfo, which raises for a damaged file, so every slot is read on its own and a broken one shows its error.
function SaveSlots:refresh()
    local rows = {}
    local problem
    for index, slot in ipairs(kSlots) do
        local ok, info = pcall(storage.slotInfo, slot)
        if not ok then
            rows[index] = {id = slot, cells = {slot, 'Damaged', '', '', ''}}
            problem = slot == self.selected and info or problem
        elseif info then
            rows[index] = {id = slot, cells = {slot, info.summary.place, info.summary.day, info.summary.wood, sample.time(info.savedAt)}}
        else
            rows[index] = {id = slot, cells = {slot, 'Empty', '', '', ''}}
        end
    end
    self:show('slots', {rows = rows, selected = self.selected})
    self:show('problem', {visible = problem ~= nil, message = problem or ''})

    local ok, slots = pcall(storage.listSlots)
    local names = {}
    for index, info in ipairs(ok and slots or {}) do
        names[index] = info.slot
    end
    self:show('newest', {text = ok and string.format('storage.listSlots() returns %d slots, newest first: %s', #slots, table.concat(names, ', ')) or 'storage.listSlots() raised: ' .. slots})
end

function SaveSlots:save()
    storage.writeSlot(self.selected, self.game, self:summary())
    self:show('status', {text = string.format("storage.writeSlot('%s', game, summary)", self.selected)})
    self:refresh()
end

function SaveSlots:loadSlot()
    local ok, game = pcall(storage.readSlot, self.selected)
    if ok and game then
        self.game = game
        self:showGame()
    end
    self:show('status', {text = not ok and 'storage.readSlot raised: ' .. game or (game and "storage.readSlot('" .. self.selected .. "') loaded the game" or 'storage.readSlot returned nil, the slot is empty')})
end

function SaveSlots:delete()
    self:show('status', {text = string.format("storage.removeSlot('%s') returned %s", self.selected, tostring(storage.removeSlot(self.selected)))})
    self:refresh()
end

return SaveSlots
