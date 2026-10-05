-- Every way a container places its children: justify and alignItems, growing within size bounds, margins and padding, rows that wrap, grids that fit their columns to the width, stacks and the aspect ratio, with a width you change.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local Layouts = haylen.class('Layouts', Test)

Layouts.justify = {'start', 'center', 'end', 'spaceBetween', 'spaceAround', 'spaceEvenly'}
Layouts.align = {'start', 'center', 'end', 'stretch'}
Layouts.tags = {'Wood', 'Stone', 'Rope', 'Iron ore', 'Fish', 'Coconut', 'Sail cloth', 'Gold', 'Clay', 'Shells', 'Torch', 'Map piece'}

function Layouts:init(entry)
    Layouts.super.init(self, entry)
    self.share = 0.8
    self.chosenJustify = Layouts.justify[4]
    self.chosenAlign = Layouts.align[2]
end

-- A box that shows where a node lands, with its name on it.
local function box(text, properties)
    local node = properties or {}
    node.padding = node.padding or 8
    node[1] = ui.label{text = text, font = 'caption', textAlign = 'center', align = 'stretch'}
    return ui.card(node)
end

local function choices(names)
    local items = {}
    for index, name in ipairs(names) do
        items[index] = {id = name, text = name}
    end
    return items
end

function Layouts:enter()
    self:frame{
        hint = 'Change the width of the examples with the slider, and the justify and alignItems of the first row with the steppers. Every row, grid and stack below follows at once.',
        focus = 'width',
        content = {self:columns()},
    }
    self:apply()
end

function Layouts:columns()
    local tags = {}
    for index, name in ipairs(Layouts.tags) do
        tags[index] = ui.chip{text = name, selected = index % 4 == 0}
    end
    local cells = {}
    for index = 1, 9 do
        cells[index] = box('Cell ' .. index, {height = 60 + (index % 3) * 20, align = index == 5 and 'center' or nil})
    end
    return layout.columns{
        ui.column{grow = 1, gap = 24,
            layout.section('Width, "justify" and "alignItems"', {
                ui.slider{id = 'width', value = self.share, min = 0.3, max = 1, step = 0.05, showValue = true, onChange = function(event)
                    self.share = event.value
                    self:apply()
                end},
                ui.stepper{id = 'justify', items = choices(Layouts.justify), selected = self.chosenJustify, onChange = function(event)
                    self.chosenJustify = event.value
                    self:apply()
                end},
                ui.stepper{id = 'align', items = choices(Layouts.align), selected = self.chosenAlign, onChange = function(event)
                    self.chosenAlign = event.value
                    self:apply()
                end},
                ui.row{id = 'line', height = 140, gap = 8, padding = 8, box('Short', {width = 120, height = 50}), box('Tall', {width = 120, height = 110}), box('Own end', {width = 120, height = 50, align = 'end'})},
            }),
            layout.section('Growing within "minWidth" and "maxWidth"', {
                ui.row{id = 'fill', gap = 8, box('Grows to 220 at most', {grow = 1, maxWidth = 220}), box('Grows, 160 at least', {grow = 1, minWidth = 160}), box('Grows twice', {grow = 2})},
            }),
            layout.section('Margins and padding', {
                ui.row{id = 'spaced', gap = 0, padding = {12, 24}, box('Margin 0'), box('Margin 24', {margin = 24}), box('Margin 0, 40', {margin = {0, 40}})},
            }),
        },
        ui.column{grow = 1, gap = 24,
            layout.section('A row with "wrap"', {ui.row{id = 'tags', wrap = true, gap = 8, lineGap = 12, children = tags}}),
            layout.section('A grid with "minColumnWidth" of 160', {ui.grid{id = 'cells', minColumnWidth = 160, gap = 8, alignItems = 'stretch', children = cells}}),
            layout.section('A stack and "aspectRatio"', {
                ui.row{id = 'shapes', gap = 12, align = 'start',
                    ui.stack{width = 220, height = 160, alignItems = 'center',
                        box('Stretched', {align = 'stretch'}),
                        box('Center', {width = 100}),
                        box('End', {align = 'end', width = 70}),
                    },
                    box('16 by 9', {aspectRatio = 16 / 9, height = 120}),
                    box('Square', {aspectRatio = 1, height = 120}),
                },
            }),
        },
    }
end

-- Applies the width and the choices to the examples and reports what they are.
function Layouts:apply()
    local width = math.floor(820 * self.share)
    self:set('line', {width = width, justify = self.chosenJustify, alignItems = self.chosenAlign})
    for _, id in ipairs({'fill', 'spaced', 'tags', 'cells'}) do
        self:set(id, {width = width})
    end
    self:set('status', {text = string.format('Width %d, justify "%s", alignItems "%s".', width, self.chosenJustify, self.chosenAlign)})
end

return Layouts
