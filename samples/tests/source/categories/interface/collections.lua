-- A list with pictures and captions, a draggable list the player reorders, a tree that opens and closes and a table with fixed and shared columns.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local Collections = haylen.class('Collections', Test)

function Collections:init(entry)
    Collections.super.init(self, entry)
    self.quests = {'Light the beacon', 'Chop ten trees', 'Build a raft', 'Catch a fish', 'Find the key'}
end

function Collections:enter()
    self:frame{
        hint = 'Pick rows with a click, a tap or Enter. Drag a quest onto another to reorder the list, or carry it with Enter or the south button, move with the arrows and drop it with Enter again.',
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
        items[index] = {id = 'quest-' .. index, text = index .. '. ' .. quest}
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
    event.document:set('quests', {items = self:questItems()})
    self:report(string.format('Moved "%s" to place %d.', self.quests[to], to))
end

function Collections:columns()
    local saves = {
        {id = 'slot1', text = 'Slot 1', caption = 'Day 12, 3 hours', image = 'interface/icons/star.png'},
        {id = 'slot2', text = 'Slot 2', caption = 'Day 4, 40 minutes', image = 'interface/icons/heart.png'},
        {id = 'slot3', text = 'Slot 3', caption = 'Corrupted', image = 'interface/icons/gear.png', enabled = false},
        {id = 'slot4', text = 'New game', caption = 'Empty slot'},
    }
    local tree = {
        {id = 'tools', text = 'Tools', children = {{id = 'hammer', text = 'Hammer', image = 'interface/icons/hammer.png'}, {id = 'key', text = 'Old key', image = 'interface/icons/key.png'}}},
        {id = 'food', text = 'Food', children = {{id = 'apple', text = 'Apple', image = 'interface/icons/apple.png'}, {id = 'fish', text = 'Fish', image = 'interface/icons/fish.png', caption = 'Raw'}}},
        {id = 'treasure', text = 'Treasure', children = {{id = 'coins', text = 'Coins', children = {{id = 'gold', text = 'Gold coin'}, {id = 'silver', text = 'Silver coin'}}}}},
    }
    return layout.columns{
        layout.section('Component "list"', {grow = 1,
            ui.list{id = 'saves', items = saves, selected = 'slot1', onSelect = function(event) self:report('The list selected "' .. event.item .. '".') end},
            ui.divider{},
            ui.label{text = 'Draggable', color = 'textMuted'},
            ui.list{id = 'quests', draggable = true, items = self:questItems(), onDrop = function(event) self:reorder(event) end},
        }),
        layout.section('Component "tree"', {grow = 1,
            ui.tree{items = tree, expanded = {'tools', 'food'}, onSelect = function(event)
                self:report('The tree selected "' .. event.item .. '".')
            end, onToggle = function(event)
                self:report(string.format('The node "%s" %s.', event.item, event.expanded and 'opened' or 'closed'))
            end},
        }),
        layout.section('Component "table"', {grow = 1,
            ui.table{
                columns = {{text = 'Captain'}, {text = 'Days', width = 120, align = 'center'}, {text = 'Score', width = 160, align = 'end'}},
                rows = {
                    {id = 'ana', cells = {'Ana', 31, 12040}},
                    {id = 'leo', cells = {'Leo', 27, 9950}},
                    {id = 'bia', cells = {'Bia', 19, 7300}},
                    {id = 'kai', cells = {'Kai', 8, 2210}},
                },
                selected = 'ana',
                onSelect = function(event) self:report('The table selected "' .. event.item .. '".') end,
            },
        }),
    }
end

return Collections
