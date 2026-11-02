-- A list with pictures and captions, a long draggable list the player chooses rows of and reorders, a tree that opens and closes and a table with fixed and shared columns.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local Collections = haylen.class('Collections', Test)

Collections.code = [[
ui.list{items = {{id = 'slot1', text = 'Slot 1', caption = 'Day 12, 3 hours', image = 'interface/themes/fantasy/icons/star.png'}}, onSelect = function(event) load(event.item) end}
ui.list{id = 'quests', draggable = true, items = quests, onDrop = function(event) move(event.sourceItem, event.item) end}  -- The app reorders and sets the items.
ui.tree{items = {{id = 'tools', text = 'Tools', children = {{id = 'hammer', text = 'Hammer'}}}}, expanded = {'tools'}}
ui.table{columns = {{text = 'Captain'}, {text = 'Score', width = 160, align = 'end'}}, rows = {{id = 'ana', cells = {'Ana', 12040}}}, selected = 'ana'}]]

function Collections:init(entry)
    Collections.super.init(self, entry)
    self.quests = {'Light the beacon', 'Chop ten trees', 'Build a raft', 'Catch a fish', 'Find the key'}
    for index = 1, 195 do
        self.quests[#self.quests + 1] = string.format('Chart the reef %d', index)
    end
end

function Collections:enter()
    self:frame{
        hint = 'Pick rows with a click, a tap or Enter. Drag a quest onto another to reorder the list of 200 quests, which draws only the rows in view, or choose it with Enter or the south button, pick it up with a second press, move with the arrows and drop it with Enter again.',
        code = Collections.code,
        focus = 'saves',
        content = {self:columns()},
    }
end

function Collections:report(text)
    self:set('status', {text = text})
end

function Collections:questItems()
    local items = {}
    for index, quest in ipairs(self.quests) do
        items[index] = {id = 'quest-' .. index, text = index .. '. ' .. quest, image = 'interface/icons/grip.svg', iconColor = 'textMuted'}
    end
    return items
end

-- Moves the dragged quest to the row it was dropped on.
function Collections:reorder(event)
    local from = tonumber(event.sourceItem:match('%d+'))
    local to = tonumber(event.item:match('%d+'))
    if event.source ~= 'quests' or from == to then
        return
    end
    table.insert(self.quests, to, table.remove(self.quests, from))
    event.gui:set('quests', {items = self:questItems()})
    self:report(string.format('Moved "%s" to place %d.', self.quests[to], to))
end

function Collections:columns()
    local saves = {
        {id = 'slot1', text = 'Slot 1', caption = 'Day 12, 3 hours', image = 'interface/themes/fantasy/icons/star.png', iconColor = false},
        {id = 'slot2', text = 'Slot 2', caption = 'Day 4, 40 minutes', image = 'interface/themes/fantasy/icons/heart.png', iconColor = false},
        {id = 'slot3', text = 'Slot 3', caption = 'Corrupted', image = 'interface/themes/fantasy/icons/gear.png', iconColor = false, enabled = false},
        {id = 'autosave', text = 'Autosave', caption = 'Day 12, 5 hours', image = 'interface/themes/fantasy/icons/coin.png', iconColor = false},
        {id = 'cloud', text = 'Cloud save', caption = 'Day 9, 2 hours', image = 'interface/themes/fantasy/icons/gem.png', iconColor = false},
        {id = 'new', text = 'New game', caption = 'Empty slot', image = 'interface/icons/save.svg', iconColor = false},
    }
    local tree = {
        {id = 'tools', text = 'Tools', children = {{id = 'hammer', text = 'Hammer', image = 'interface/icons/hammer.png', iconColor = false}, {id = 'key', text = 'Old key', image = 'interface/themes/fantasy/icons/key.png', iconColor = false}}},
        {id = 'food', text = 'Food', children = {{id = 'apple', text = 'Apple', image = 'interface/icons/apple.png', iconColor = false}, {id = 'fish', text = 'Fish', image = 'interface/icons/fish.png', iconColor = false, caption = 'Raw'}}},
        {id = 'treasure', text = 'Treasure', children = {{id = 'coins', text = 'Coins', children = {{id = 'gold', text = 'Gold coin'}, {id = 'silver', text = 'Silver coin'}}}}},
    }
    local captains = {{'Ana', 31, 12040}, {'Leo', 27, 9950}, {'Bia', 19, 7300}, {'Rui', 14, 5120}, {'Kai', 8, 2210}, {'Lia', 3, 640}}
    local rows = {}
    for index, captain in ipairs(captains) do
        rows[index] = {id = captain[1]:lower(), cells = captain}
    end
    return layout.columns{
        layout.section('Component "list"', {grow = 1, align = 'stretch',
            ui.list{id = 'saves', items = saves, selected = 'slot1', onSelect = function(event) self:report('The list selected "' .. event.item .. '".') end},
        }),
        layout.section('Component "tree"', {grow = 1, align = 'stretch',
            ui.tree{items = tree, expanded = {'tools', 'food'}, onSelect = function(event)
                self:report('The tree selected "' .. event.item .. '".')
            end, onToggle = function(event)
                self:report(string.format('The node "%s" %s.', event.item, event.expanded and 'opened' or 'closed'))
            end},
        }),
        layout.section('Component "table"', {grow = 1, align = 'stretch',
            ui.table{
                columns = {{text = 'Captain'}, {text = 'Days', width = 90, align = 'center'}, {text = 'Score', width = 120, align = 'end'}},
                rows = rows,
                selected = 'ana',
                onSelect = function(event) self:report('The table selected "' .. event.item .. '".') end,
            },
        }),
        layout.section('A draggable list of 200 quests', {grow = 1, align = 'stretch',
            ui.scroll{grow = 1, height = 0, ui.list{id = 'quests', draggable = true, items = self:questItems(), onDrop = function(event) self:reorder(event) end, onSelect = function(event)
                self:report('The draggable list chose "' .. event.item .. '". Press again to pick it up.')
            end}},
        }),
    }
end

return Collections
