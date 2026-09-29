-- Slot grid: an inventory, a hotbar mounted as another document and a draggable chest list that trade items. The grids only report moves, so the test keeps the items and sets the new slots after every drop.
local haylen = require('haylen')
local ui = require('haylen.ui')

local sample = require('sample')

local SlotGrid = haylen.class('SlotGrid', sample.Test)

SlotGrid.hints = 'Drag items between slots, the hotbar and the chest. With keys, a gamepad or a remote, Enter or south picks up and drops, and Escape puts back.'
SlotGrid.focus = 'bag'

local kBagSize = 12
local kHotbarSize = 6

function SlotGrid:init(entry)
    SlotGrid.super.init(self, entry)
    self.bag = {['bag-1'] = {icon = 'sword'}, ['bag-2'] = {icon = 'apple', count = 5}, ['bag-3'] = {icon = 'coin', count = 40}, ['bag-6'] = {icon = 'potion', count = 3}, ['bag-9'] = {icon = 'key'}}
    self.hotbar = {['hotbar-1'] = {icon = 'hammer'}, ['hotbar-2'] = {icon = 'fish', count = 2}}
    self.chest = {{icon = 'gem', count = 1}, {icon = 'shield'}, {icon = 'star', count = 3}}
end

local function slots(grid, prefix, size)
    local list = {}
    for index = 1, size do
        local id = prefix .. '-' .. index
        local item = grid[id]
        list[index] = {id = id, image = item and ('icons/' .. item.icon .. '.png') or nil, count = item and item.count or nil}
    end
    return list
end

function SlotGrid:chestItems()
    local items = {}
    for index, item in ipairs(self.chest) do
        items[index] = {id = 'chest-' .. index, text = item.icon, caption = item.count and ('x' .. item.count) or 'one', image = 'icons/' .. item.icon .. '.png'}
    end
    return items
end

-- Takes the item out of a slot or a chest row and returns it with where it was.
function SlotGrid:take(source, place)
    if source == 'chest' then
        local index = tonumber(place:match('%d+'))
        return table.remove(self.chest, index), index
    end
    local item = self[source][place]
    self[source][place] = nil
    return item
end

function SlotGrid:drop(event)
    local source, target = event.source, event.id
    if source == target and event.sourceItem == event.item then
        return
    end
    local item, chestIndex = self:take(source, event.sourceItem)
    if item == nil then
        return
    end
    if target == 'chest' then
        table.insert(self.chest, math.min(tonumber(event.item:match('%d+')), #self.chest + 1), item)
    else
        local displaced = self[target][event.item]
        self[target][event.item] = item
        if displaced and source == 'chest' then
            table.insert(self.chest, chestIndex, displaced)
        elseif displaced then
            self[source][event.sourceItem] = displaced
        end
    end
    self:refresh()
    self:setStatus(string.format('moved the %s from %s to %s', item.icon, event.sourceItem, event.item))
end

function SlotGrid:refresh()
    self.document:set('bag', {slots = slots(self.bag, 'bag', kBagSize)})
    self.document:set('chest', {items = self:chestItems()})
    self.hotbarDocument:set('hotbar', {slots = slots(self.hotbar, 'hotbar', kHotbarSize)})
end

function SlotGrid:content()
    local function onDrop(event)
        self:drop(event)
    end
    local function onSelect(event)
        self:setStatus('selected ' .. event.item)
    end
    return sample.columns{
        justify = 'center',
        sample.section('inventory', {
            ui.slotGrid{id = 'bag', columns = 4, slots = slots(self.bag, 'bag', kBagSize), onDrop = onDrop, onSelect = onSelect, onDrag = function(event)
                self:setStatus('picked up ' .. event.item)
            end},
        }),
        sample.section('chest', {width = 460,
            ui.list{id = 'chest', draggable = true, items = self:chestItems(), onDrop = onDrop},
        }),
    }
end

-- The hotbar is a document of its own at the bottom of the safe area, and items still move between it and the other document.
function SlotGrid:started()
    self.hotbarDocument = sample.mount(self, ui.slotGrid{id = 'hotbar', anchor = 'bottom', margin = {0, 0, 72, 0}, columns = kHotbarSize, slotSize = 88, slots = slots(self.hotbar, 'hotbar', kHotbarSize), onDrop = function(event)
        self:drop(event)
    end})
end

return SlotGrid
