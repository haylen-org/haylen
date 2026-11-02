-- Every way a container places its children: justify and alignItems, growing within size bounds, margins and padding, rows that wrap, grids that fit their columns to the width, stacks and the aspect ratio, with a width you change.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local Layouts = haylen.class('Layouts', Test)

Layouts.code = [[
ui.row{justify = 'spaceBetween', alignItems = 'center', padding = {12, 24}, gap = 8, children = boxes}
ui.card{grow = 1, minWidth = 160, maxWidth = 220, margin = {0, 40}}
ui.row{wrap = true, gap = 8, lineGap = 12, children = tags}
ui.grid{minColumnWidth = 160, gap = 8, alignItems = 'stretch', children = cells}
ui.card{aspectRatio = 16 / 9, height = 100}]]

Layouts.justify = {'start', 'center', 'end', 'spaceBetween', 'spaceAround', 'spaceEvenly'}
Layouts.align = {'start', 'center', 'end', 'stretch'}
-- The names the steppers show for the values of "justify" and "alignItems".
Layouts.names = {start = 'Start', center = 'Center', ['end'] = 'End', spaceBetween = 'Space between', spaceAround = 'Space around', spaceEvenly = 'Space evenly', stretch = 'Stretch'}
-- The captions of some cells of the grid, which make them taller, so the stretch shows on the cells beside them.
Layouts.captions = {[2] = 'Two lines', [4] = 'Three\nlines', [9] = 'Two lines'}
Layouts.tags = {'Wood', 'Stone', 'Rope', 'Iron ore', 'Fish', 'Coconut', 'Sail cloth', 'Gold', 'Clay', 'Shells', 'Torch', 'Map piece'}

function Layouts:init(entry)
    Layouts.super.init(self, entry)
    self.share = 0.85
    self.chosenJustify = Layouts.justify[4]
    self.chosenAlign = Layouts.align[2]
end

-- A box that shows where a node lands, with its name in its middle.
local function box(text, properties)
    local node = properties or {}
    node.padding = node.padding or 8
    node.justify = 'center'
    node[1] = ui.label{text = text, font = 'caption', textAlign = 'center', align = 'stretch'}
    return ui.card(node)
end

local function choices(names)
    local items = {}
    for index, name in ipairs(names) do
        items[index] = {id = name, text = Layouts.names[name]}
    end
    return items
end

function Layouts:enter()
    self:frame{
        hint = 'Change the width of the examples with the slider, and the justify and alignItems of the first row with the steppers. Every row, grid and stack below follows at once.',
        code = Layouts.code,
        focus = 'width',
        content = {self:columns()},
    }
end

function Layouts:columns()
    local tags = {}
    for index, name in ipairs(Layouts.tags) do
        tags[index] = ui.chip{text = name, selected = index % 4 == 0}
    end
    local cells = {}
    for index = 1, 9 do
        local caption = Layouts.captions[index]
        cells[index] = box(caption and 'Cell ' .. index .. '\n' .. caption or 'Cell ' .. index)
    end
    return layout.columns{
        ui.column{grow = 1, gap = 24,
            layout.section('Width, "justify" and "alignItems"', {
                ui.slider{id = 'width', value = self.share, min = 0.3, max = 1, step = 0.05, showValue = true, align = 'stretch', onChange = function(event)
                    self.share = event.value
                    self:apply()
                end},
                ui.row{gap = 16,
                    ui.formField{label = 'Justify', grow = 1, ui.stepper{id = 'justify', items = choices(Layouts.justify), selected = self.chosenJustify, onChange = function(event)
                        self.chosenJustify = event.value
                        self:apply()
                    end}},
                    ui.formField{label = 'Align items', grow = 1, ui.stepper{id = 'align', items = choices(Layouts.align), selected = self.chosenAlign, onChange = function(event)
                        self.chosenAlign = event.value
                        self:apply()
                    end}},
                },
                ui.row{id = 'line', height = 120, gap = 8, padding = 8, box('Short', {width = 110, height = 44}), box('Tall', {width = 110, height = 100}), box('Own end', {width = 110, height = 44, align = 'end'})},
            }),
            layout.section('Growing within "minWidth" and "maxWidth"', {grow = 1,
                ui.row{id = 'fill', gap = 8, box('At most 220', {grow = 1, maxWidth = 220}), box('At least 160', {grow = 1, minWidth = 160}), box('Grows twice', {grow = 2})},
            }),
        },
        ui.column{grow = 1, gap = 24,
            layout.section('Margins and padding', {
                ui.row{id = 'spaced', gap = 0, padding = {12, 24}, box('Margin 0'), box('Margin 24', {margin = 24}), box('Margin 0, 40', {margin = {0, 40}})},
            }),
            layout.section('A row with "wrap"', {grow = 1, ui.row{id = 'tags', wrap = true, gap = 8, lineGap = 12, children = tags}}),
        },
        ui.column{grow = 1, gap = 24,
            layout.section('A grid with "minColumnWidth" of 160', {ui.grid{id = 'cells', minColumnWidth = 160, gap = 8, alignItems = 'stretch', children = cells}}),
            layout.section('A stack and "aspectRatio"', {grow = 1,
                ui.row{id = 'shapes', gap = 12, align = 'start',
                    ui.stack{width = 180, height = 130, alignItems = 'center',
                        box('Stretched', {align = 'stretch'}),
                        box('Center', {width = 90}),
                        box('End', {align = 'end', width = 64}),
                    },
                    box('16 by 9', {aspectRatio = 16 / 9, height = 100}),
                    box('Square', {aspectRatio = 1, height = 100}),
                },
            }),
        },
    }
end

-- The examples take a share of the width of the cards, which the slider sets, and follow the cards when the window changes.
function Layouts:update(dt)
    Layouts.super.update(self, dt)
    local slider = self.gui:bounds('width')
    if slider and slider.width ~= self.room then
        self.room = slider.width
        self:apply()
    end
end

-- Applies the width and the choices to the examples and reports what they are.
function Layouts:apply()
    if not self.room then
        return
    end
    local width = math.floor(self.room * self.share)
    self:set('line', {width = width, justify = self.chosenJustify, alignItems = self.chosenAlign})
    for _, id in ipairs({'fill', 'spaced', 'tags', 'cells'}) do
        self:set(id, {width = width})
    end
    self:set('status', {text = string.format('The examples are %d units wide, and the first row has justify "%s" and alignItems "%s".', width, self.chosenJustify, self.chosenAlign)})
end

return Layouts
