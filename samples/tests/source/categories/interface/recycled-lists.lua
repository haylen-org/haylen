-- A list of mixed item types under sticky section headers, a grid with full width headers and multiple selection, and a horizontal shelf that snaps to its tiles, all built from a few recycled cells, with buttons that insert, remove, move, shuffle and scroll to an item.
local debug = require('haylen.debug')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local RecycledLists = haylen.class('RecycledLists', Test)

RecycledLists.sections = {'Harbor', 'Forest', 'Cliffs', 'Village', 'Caves', 'Lighthouse'}
RecycledLists.rowsPerSection = 40
RecycledLists.tiles = 600
RecycledLists.shelf = 300

function RecycledLists:init(entry)
    RecycledLists.super.init(self, entry)
    self.added = 0
    self.last = ''
end

-- Every section starts with a sticky header, and its rows alternate between notes, entries with a caption and switches that remember their state in the item.
function RecycledLists:buildRows()
    local rows = {}
    for section, title in ipairs(RecycledLists.sections) do
        rows[#rows + 1] = {id = 'section-' .. section, type = 'header', title = title}
        for row = 1, RecycledLists.rowsPerSection do
            local id = title:lower() .. '-' .. row
            if row % 5 == 0 then
                rows[#rows + 1] = {id = id, type = 'switch', title = title .. ' lantern ' .. row, on = row % 10 == 0}
            elseif row % 2 == 0 then
                rows[#rows + 1] = {id = id, type = 'entry', title = title .. ' trail ' .. row, caption = (row * 37 % 90 + 10) .. ' steps from camp'}
            else
                rows[#rows + 1] = {id = id, type = 'note', title = title .. ' note ' .. row}
            end
        end
    end
    return rows
end

function RecycledLists:buildTiles()
    local tiles = {}
    for index = 1, RecycledLists.tiles do
        if index % 25 == 1 then
            tiles[#tiles + 1] = {id = 'group-' .. index, type = 'header', title = 'Crates ' .. index .. ' to ' .. (index + 23)}
        end
        tiles[#tiles + 1] = {id = 'crate-' .. index, type = 'tile', title = 'Crate ' .. index, weight = (index * 13 % 40 + 5) .. ' kg'}
    end
    return tiles
end

function RecycledLists:buildShelf()
    local shelf = {}
    for index = 1, RecycledLists.shelf do
        shelf[index] = {id = 'shell-' .. index, title = 'Shell ' .. index}
    end
    return shelf
end

function RecycledLists:enter()
    self:frame{
        hint = 'Scroll with the wheel, the scrollbar or a finger, which flings the lists. Arrows, the directional pad and remotes move between items, Page Up, Page Down, Home, End and the shoulder buttons page, and Enter or the south button selects tiles.',
        focus = 'list',
        content = {self:columns()},
    }
    self.rows = self:buildRows()
    self.tileItems = self:buildTiles()
    self.list = self.gui:collection('list')
    self.list:setItems(self.rows)
    self.list:setBinder('entry', function(cell, item, index)
        cell:set('index', {text = '#' .. index})
    end)
    self.gui:collection('tiles'):setItems(self.tileItems)
    self.gui:collection('shelf'):setItems(self:buildShelf())
end

function RecycledLists:columns()
    return layout.columns{
        layout.section('Mixed list with sticky sections', {grow = 3, align = 'stretch',
            ui.row{gap = 12,
                ui.button{id = 'insert', text = 'Insert above', onClick = function() self:insertAbove() end},
                ui.button{id = 'remove', text = 'Remove', onClick = function() self:removeFocused() end},
                ui.button{id = 'move', text = 'Move', onClick = function() self:moveFirst() end},
                ui.button{id = 'shuffle', text = 'Shuffle', onClick = function() self:shuffle() end},
                ui.button{id = 'jump', text = 'Go to item 150', onClick = function() self.list:scrollTo(150, {align = 'center'}) end},
            },
            ui.collection{id = 'list', grow = 1, gap = 8, focusAlign = 'nearest', types = {
                header = {template = ui.sectionTitle{part = 'title', bind = {text = 'title'}}, sticky = true, interactive = false},
                note = {template = ui.label{part = 'title', bind = {text = 'title'}}},
                entry = {template = ui.row{gap = 16,
                    ui.label{part = 'index', text = '', width = 90, color = 'textMuted'},
                    ui.column{grow = 1, ui.label{part = 'title', bind = {text = 'title'}}, ui.label{part = 'caption', bind = {text = 'caption'}, font = 'caption', color = 'textMuted'}},
                }},
                switch = {template = ui.settingsRow{part = 'row', bind = {label = 'title'}, ui.toggle{part = 'on', bind = {checked = 'on'}}}},
            },
                onSelect = function(event) self.last = ' Pressed "' .. event.item .. '".' end,
                onChange = function(event) self.last = string.format(' The switch of "%s" is %s in its item.', event.cell.item, self.rows[event.cell.index].on and 'on' or 'off') end,
            },
        }),
        ui.column{grow = 2, gap = 24, align = 'stretch',
            layout.section('Grid with full width headers', {grow = 1,
                ui.collection{id = 'tiles', grow = 1, layout = 'grid', minCellSize = 150, cellAspect = 0.8, gap = 10, selection = 'multiple', types = {
                    header = {template = ui.label{part = 'title', bind = {text = 'title'}, font = 'heading'}, span = 'full', sticky = true, interactive = false},
                    tile = {template = ui.card{gap = 4,
                        ui.label{part = 'title', bind = {text = 'title'}},
                        ui.label{part = 'weight', bind = {text = 'weight'}, font = 'caption', color = 'textMuted'},
                        ui.badge{part = 'chosen', text = 'Chosen', tone = 'accent', bind = {visible = '$selected'}},
                    }},
                }, onSelect = function(event)
                    self.last = string.format(' The tile "%s" is %s.', event.item, event.selected and 'chosen' or 'not chosen')
                end},
            }),
            layout.section('Horizontal shelf', {
                ui.collection{id = 'shelf', axis = 'horizontal', height = 120, gap = 12, snap = 'item', rememberFocus = true, types = {
                    shell = {template = ui.card{width = 180, ui.label{part = 'title', bind = {text = 'title'}}}},
                }},
            }),
        },
    }
end

function RecycledLists:insertAbove()
    local added = {}
    for index = 1, 3 do
        self.added = self.added + 1
        added[index] = {id = 'new-' .. self.added, type = 'note', title = 'New note ' .. self.added}
    end
    local first = self.list:visibleRange() or 1
    self.list:insert(first, added)
    self.last = ' Inserted three notes above the first row in view, which stays where it was.'
end

function RecycledLists:removeFocused()
    local id = self.list.focusedItem
    local index = id and self.list:indexOf(id) or (self.list:visibleRange() or 1) + 1
    if index > self.list.count then
        return
    end
    local removed = self.rows[index].id
    self.list:remove(index)
    self.last = ' Removed "' .. removed .. '".'
end

function RecycledLists:moveFirst()
    local first = self.list:visibleRange() or 1
    local target = math.min(first + 3, self.list.count)
    local moved = self.rows[first].id
    self.list:move(first, target)
    self.last = string.format(' Moved "%s" to place %d.', moved, target)
end

-- A shuffled copy of the list compares with the list shown by id, so every row keeps its cell, its focus and its switch.
function RecycledLists:shuffle()
    local shuffled = {}
    for index, row in ipairs(self.rows) do
        shuffled[index] = row
    end
    for index = #shuffled, 2, -1 do
        local other = math.random(index)
        shuffled[index], shuffled[other] = shuffled[other], shuffled[index]
    end
    self.rows = shuffled
    self.list:setItems(shuffled)
    self.last = ' Shuffled the rows.'
end

-- Shows how many cells exist for every item and where the focus is.
function RecycledLists:update(dt)
    RecycledLists.super.update(self, dt)
    local cells = debug.stats().objects.UiCell or {alive = 0, created = 0}
    local _, id, item, part = ui.focused()
    local first, last = self.list:visibleRange()
    local focus = item and string.format('"%s" of "%s"%s', item, id, part and (' on "' .. part .. '"') or '') or tostring(id)
    self:status(string.format('Rows %d, in view %s to %s. Cells alive %d, created %d. Focus %s.%s', self.list.count, tostring(first), tostring(last), cells.alive, cells.created, focus, self.last))
end

return RecycledLists
