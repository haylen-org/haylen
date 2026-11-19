-- Every component that asks for a scroll bar, side by side, each with its bar in a lane of its own a gap away from its content: a scroll in both directions, a recycled collection, a list, a tree and a table in a scroll, a text area and an immediate window, at the scale picked here, beside a scroll that shows no bar, as areas do by default.
local haylen = require('haylen')
local imgui = require('haylen.imgui')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local ScrollBars = haylen.class('ScrollBars', Test)

ScrollBars.code = [[
ui.scroll{height = 90, scrollbar = true, ui.column{gap = 8, children = rows}}  -- The bar runs in a lane of its own beside the rows.
ui.scroll{axis = 'horizontal', scrollbar = true, ui.row{gap = 12, children = buttons}}
ui.scroll{height = 90, scrollbar = true, style = {metrics = {scrollbarGap = 0}}, ui.column{children = rows}}  -- The engine still keeps 4 points.
ui.scroll{height = 90, ui.column{children = rows}}  -- Without "scrollbar", the rows take the whole width.
ui.setScaleMode('physical')  ui.setScale(2)
local gap = ui.themeMetric('scrollbarGap')]]

ScrollBars.scales = {{id = 'design', text = 'Design 1x'}, {id = 'double', text = 'Design 2x'}, {id = 'physical', text = 'Physical'}}
ScrollBars.settings = {design = {mode = 'design', factor = 1}, double = {mode = 'design', factor = 2}, physical = {mode = 'physical', factor = 1}}
ScrollBars.minimumGap = 4

-- Lines of text for the components to scroll through.
function ScrollBars.lines(prefix, count)
    local lines = {}
    for index = 1, count do
        lines[index] = prefix .. ' ' .. index
    end
    return lines
end

function ScrollBars.choices(prefix, count)
    local items = {}
    for index, text in ipairs(ScrollBars.lines(prefix, count)) do
        items[index] = {id = prefix:lower():gsub(' ', '-') .. '-' .. index, text = text}
    end
    return items
end

function ScrollBars.labels(prefix, count)
    local labels = {}
    for index, text in ipairs(ScrollBars.lines(prefix, count)) do
        labels[index] = ui.label{text = text}
    end
    return labels
end

-- Rows on a surface as wide as the content, so the end of the content shows next to the bar.
function ScrollBars.rows(prefix, count)
    local rows = {}
    for index, text in ipairs(ScrollBars.lines(prefix, count)) do
        rows[index] = ui.panel{padding = {6, 12}, ui.label{text = text}}
    end
    return rows
end

function ScrollBars:enter()
    self.started = {mode = ui.scaleMode(), factor = ui.scale()}
    self.immediate = ScrollBars.lines('Immediate line', 40)
    self:frame{
        hint = 'Pick a scale and look at every bar, whose lane runs beside the content. The last card shows a scroll without a bar, whose rows take the whole width. The wheel, a finger, a thumb and a press on a track scroll each one.',
        code = ScrollBars.code,
        focus = 'scale',
        fixed = true,
        content = {
            ui.row{gap = 24,
                ui.segmentedControl{id = 'scale', items = ScrollBars.scales, selected = 'design', onChange = function(event) self:applyScale(event.value) end},
                ui.label{id = 'gap', grow = 1, color = 'textMuted'},
            },
            ui.scroll{id = 'cards', grow = 1, height = 0, ui.grid{minColumnWidth = 400, gap = 16, children = self:cards()}},
        },
    }
    local entries = {}
    for index, text in ipairs(ScrollBars.lines('Recycled entry', 200)) do
        entries[index] = {id = 'entry' .. index, title = text}
    end
    self.gui:collection('entries'):setItems(entries)
    local tiles = {}
    for index = 1, 30 do
        tiles[index] = {id = 'tile' .. index, title = 'Tile ' .. index}
    end
    self.gui:collection('shelf'):setItems(tiles)
end

-- The cards stand in four stacks, which the grid places side by side at the normal scale and two by two when the scale doubles, so every card shows whole at the normal scale. Every area but the last asks for its bar.
function ScrollBars:cards()
    local buttons = {}
    for index = 1, 16 do
        buttons[index] = ui.button{text = 'Item ' .. index}
    end
    local scroll = layout.section('Component "scroll"', {
        ui.scroll{height = 90, scrollbar = true, ui.column{gap = 8, children = ScrollBars.rows('Quest log entry', 30)}},
        ui.scroll{axis = 'horizontal', align = 'stretch', scrollbar = true, ui.row{gap = 12, children = buttons}},
    })
    local noGap = layout.section('A style that asks for no gap', {
        ui.label{text = 'It asks for 0, and the engine keeps 4 points.', font = 'caption', color = 'textMuted'},
        ui.scroll{height = 90, scrollbar = true, style = {metrics = {scrollbarGap = 0}}, ui.column{gap = 8, children = ScrollBars.rows('Close entry', 20)}},
    })
    local collection = layout.section('Component "collection"', {
        ui.collection{id = 'entries', height = 90, gap = 8, scrollbar = true, types = {
            row = {template = ui.panel{padding = {6, 12}, ui.label{part = 'title', bind = {text = 'title'}}}},
        }},
        ui.collection{id = 'shelf', axis = 'horizontal', align = 'stretch', gap = 12, scrollbar = true, types = {
            tile = {template = ui.card{width = 140, ui.label{part = 'title', bind = {text = 'title'}}}},
        }},
    })
    local lists = layout.section('Lists in a scroll', {
        ui.scroll{height = 110, scrollbar = true, ui.column{gap = 12,
            ui.list{items = ScrollBars.choices('Save slot', 5), selected = 'save-slot-1'},
            ui.tree{items = {{id = 'tools', text = 'Tools', children = ScrollBars.choices('Tool', 3)}, {id = 'food', text = 'Food', children = ScrollBars.choices('Fruit', 3)}}, expanded = {'tools', 'food'}},
            ui.table{columns = {{text = 'Player'}, {text = 'Score', width = 120, align = 'end'}}, rows = {{id = 'ana', cells = {'Ana', 1200}}, {id = 'bia', cells = {'Bia', 950}}, {id = 'caio', cells = {'Caio', 870}}}},
        }},
    })
    local immediate = layout.section('Immediate window', {
        ui.spacer{id = 'immediate', height = 150},
    })
    local textArea = layout.section('Component "textArea"', {
        ui.textArea{rows = 4, scrollbar = true, value = 'A line of the diary long enough to wrap onto the next line of the field\n' .. table.concat(ScrollBars.lines('A line of the diary', 24), '\n')},
    })
    local plain = layout.section('Without "scrollbar"', {
        ui.label{text = 'The rows take the whole width and fade where they go on.', font = 'caption', color = 'textMuted'},
        ui.scroll{height = 90, ui.column{gap = 8, children = ScrollBars.rows('Plain entry', 20)}},
    })
    return {
        ui.column{gap = 16, scroll, noGap},
        ui.column{gap = 16, collection, lists},
        ui.column{gap = 16, immediate, textArea},
        ui.column{gap = 16, plain},
    }
end

function ScrollBars:applyScale(id)
    local setting = ScrollBars.settings[id]
    ui.setScaleMode(setting.mode)
    ui.setScale(setting.factor)
end

function ScrollBars:exit()
    ui.setScaleMode(self.started.mode)
    ui.setScale(self.started.factor)
    ScrollBars.super.exit(self)
end

function ScrollBars:update(dt)
    ScrollBars.super.update(self, dt)
    local factor, scale, points = ui.scale()
    local gap = math.max(ui.themeMetric('scrollbarGap'), ScrollBars.minimumGap / points)
    self:status(string.format('The %s mode at a factor of %.2f draws %.2f design units and %.2f points for each UI unit.', ui.scaleMode(), factor, scale, points))
    self:set('gap', {text = string.format('The gap is %.1f UI units, %.1f points on this screen.', gap, gap * points)})
end

-- The immediate window covers the space of its card while the card shows whole, and scrolls a child without a border inside it.
function ScrollBars:renderUi()
    local bounds = self.gui and self.gui:bounds('immediate')
    local cards = self.gui and self.gui:bounds('cards')
    if not bounds or not cards or bounds.y < cards.y or bounds.y + bounds.height > cards.y + cards.height then
        return
    end
    local _, scale = ui.scale()
    local visible = viewport.visibleRect()
    imgui.setNextWindowPos((bounds.x - visible.x) / scale, (bounds.y - visible.y) / scale, true)
    imgui.setNextWindowSize(bounds.width / scale, bounds.height / scale, true)
    if imgui.beginWindow('Immediate window', {noResize = true, noMove = true}) then
        if imgui.beginChild('lines', 0, 0) then
            for _, line in ipairs(self.immediate) do
                imgui.text(line)
            end
        end
        imgui.endChild()
    end
    imgui.endWindow()
end

return ScrollBars
