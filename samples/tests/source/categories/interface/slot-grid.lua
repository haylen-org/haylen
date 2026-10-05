-- An inventory, a hotbar mounted as another document and a draggable chest list that trade items. The grids only report moves, so the test keeps the items and sets the new slots after every drop.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local SlotGrid = haylen.class('SlotGrid', Test)

SlotGrid.bagSize = 12
SlotGrid.hotbarSize = 6

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
        list[index] = {id = id, image = item and ('interface/icons/' .. item.icon .. '.png') or nil, count = item and item.count or nil}
    end
    return list
end

function SlotGrid:chestItems()
    local items = {}
    for index, item in ipairs(self.chest) do
        items[index] = {id = 'chest-' .. index, text = item.icon:sub(1, 1):upper() .. item.icon:sub(2), caption = item.count and ('Count ' .. item.count) or 'One', image = 'interface/icons/' .. item.icon .. '.png'}
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
    self:set('status', {text = string.format('Moved the %s from "%s" to "%s".', item.icon, event.sourceItem, event.item)})
end

function SlotGrid:refresh()
    self.document:set('bag', {slots = slots(self.bag, 'bag', SlotGrid.bagSize)})
    self.document:set('chest', {items = self:chestItems()})
    self.hotbarDocument:set('hotbar', {slots = slots(self.hotbar, 'hotbar', SlotGrid.hotbarSize)})
end

-- The hotbar is a document of its own above the hint line, and items still move between it and the frame.
function SlotGrid:enter()
    local function onDrop(event)
        self:drop(event)
    end
    self:frame{
        hint = 'Drag items between slots, the hotbar and the chest. With keys, a gamepad or a remote, Enter or the south button picks up and drops, and Escape puts back.',
        focus = 'bag',
        content = {layout.columns{
            justify = 'center',
            layout.section('Inventory', {
                ui.slotGrid{id = 'bag', columns = 4, slots = slots(self.bag, 'bag', SlotGrid.bagSize), onDrop = onDrop, onSelect = function(event)
                    self:set('status', {text = 'Selected "' .. event.item .. '".'})
                end, onDrag = function(event)
                    self:set('status', {text = 'Picked up "' .. event.item .. '".'})
                end},
            }),
            layout.section('Chest', {width = 460,
                ui.list{id = 'chest', draggable = true, items = self:chestItems(), onDrop = onDrop},
            }),
        }},
    }
    self.hotbarDocument = ui.mount(ui.slotGrid{id = 'hotbar', anchor = 'bottom', margin = {0, 0, 150, 0}, columns = SlotGrid.hotbarSize, slotSize = 88, slots = slots(self.hotbar, 'hotbar', SlotGrid.hotbarSize), onDrop = onDrop, onCancel = function()
        self:cancel()
    end}, {owner = self})
end

return SlotGrid
