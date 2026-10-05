-- Every component that scrolls, side by side, each with its scroll bar in a lane of its own a gap away from its content: a scroll in both directions, a recycled collection, a list, a tree and a table in a scroll, a text area, the popups of a combo, a menu button, a context menu and a popover, a dialog and an immediate window, at the scale picked here.
local haylen = require('haylen')
local imgui = require('haylen.imgui')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local ScrollBars = haylen.class('ScrollBars', Test)

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
        hint = 'Pick a scale and look at every bar: its lane runs beside the content, which ends the gap before it. Open the combo, the menu, the popover and the dialog to see their bars, and right click or long press the quest card for its menu. The wheel, a drag of a thumb and a press on a track scroll each one.',
        focus = 'scale',
        content = {
            ui.row{gap = 24,
                ui.segmentedControl{id = 'scale', items = ScrollBars.scales, selected = 'design', onChange = function(event) self:applyScale(event.value) end},
                ui.label{id = 'gap', grow = 1, color = 'textMuted'},
            },
            ui.scroll{id = 'cards', grow = 1, height = 0, ui.grid{minColumnWidth = 400, gap = 16, children = self:cards()}},
            ui.dialog{id = 'credits', title = 'Credits', message = 'The body of a dialog taller than the screen scrolls beside a bar at its edge.', buttons = {{id = 'close', text = 'Close', variant = 'primary'}},
                ui.column{gap = 8, children = ScrollBars.labels('Credit line', 80)},
            },
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

function ScrollBars:cards()
    local buttons = {}
    for index = 1, 16 do
        buttons[index] = ui.button{text = 'Item ' .. index}
    end
    return {
        layout.section('Component "scroll"', {
            ui.scroll{height = 160, ui.column{gap = 8, children = ScrollBars.rows('Quest log entry', 30)}},
            ui.scroll{axis = 'horizontal', align = 'stretch', ui.row{gap = 12, children = buttons}},
        }),
        layout.section('Component "collection"', {
            ui.collection{id = 'entries', height = 160, gap = 8, types = {
                row = {template = ui.panel{padding = {6, 12}, ui.label{part = 'title', bind = {text = 'title'}}}},
            }},
            ui.collection{id = 'shelf', axis = 'horizontal', align = 'stretch', gap = 12, types = {
                tile = {template = ui.card{width = 140, ui.label{part = 'title', bind = {text = 'title'}}}},
            }},
        }),
        layout.section('List, tree and table in a scroll', {
            ui.scroll{height = 230, ui.column{gap = 12,
                ui.list{items = ScrollBars.choices('Save slot', 5), selected = 'save-slot-1'},
                ui.tree{items = {{id = 'tools', text = 'Tools', children = ScrollBars.choices('Tool', 3)}, {id = 'food', text = 'Food', children = ScrollBars.choices('Fruit', 3)}}, expanded = {'tools', 'food'}},
                ui.table{columns = {{text = 'Player'}, {text = 'Score', width = 120, align = 'end'}}, rows = {{id = 'ana', cells = {'Ana', 1200}}, {id = 'bia', cells = {'Bia', 950}}, {id = 'caio', cells = {'Caio', 870}}}},
            }},
        }),
        layout.section('Component "textArea"', {
            ui.textArea{rows = 5, value = 'A line of the diary long enough to run past the end of the field\n' .. table.concat(ScrollBars.lines('A line of the diary', 24), '\n')},
        }),
        layout.section('Popups and the dialog', {
            ui.combo{id = 'combo', items = ScrollBars.choices('Choice', 80), selected = 'choice-1'},
            ui.row{gap = 12,
                ui.menuButton{id = 'menu', text = 'Menu', items = ScrollBars.choices('Command', 60)},
                ui.popover{id = 'popover', text = 'Popover', contentWidth = 360, ui.column{gap = 8, children = ScrollBars.labels('Popover line', 60)}},
                ui.button{text = 'Dialog', onClick = function(event) event.gui:set('credits', {open = true}) end},
            },
            ui.contextMenu{id = 'actions', items = ScrollBars.choices('Action', 60), ui.card{ui.label{text = 'A quest card with a long context menu'}}},
        }),
        layout.section('Immediate window', {
            ui.spacer{id = 'immediate', height = 200},
        }),
        layout.section('A style that asks for no gap', {
            ui.label{text = 'This scroll asks for a gap of 0, and the engine keeps 4 points.', wrap = true, color = 'textMuted'},
            ui.scroll{height = 120, style = {metrics = {scrollbarGap = 0}}, ui.column{gap = 8, children = ScrollBars.rows('Close entry', 20)}},
        }),
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
    self:status(string.format('Mode "%s", factor %.2f, %.2f design units and %.2f points per UI unit.', ui.scaleMode(), factor, scale, points))
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
