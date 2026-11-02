-- A floating window the pointer drags by its title bar, accordions with one or several open sections, a carousel of pages and a horizontal scroll that snaps to its cards.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local WindowsPages = haylen.class('WindowsPages', Test)

WindowsPages.code = [[
ui.window{id = 'bag', title = 'Bag', closable = true, onClose = function() log.info('The bag closed.') end, bagSections}
local dock = gui:bounds('dock')
gui:set('bag', {open = true, x = dock.x, y = dock.y, width = dock.width})
ui.accordion{multiple = true, items = {{id = 'food', text = 'Food'}, {id = 'tools', text = 'Tools'}}, expanded = {'food'}, foodRow, toolsRow}
ui.carousel{loop = true, interval = 5, dayPage, duskPage, nightPage}
ui.scroll{axis = 'horizontal', snap = true, ui.row{gap = 24, children = levelCards}}]]

local function levels()
    local cards = {}
    for index = 1, 10 do
        cards[index] = ui.card{width = 240, gap = 6,
            ui.label{text = 'Level ' .. index, font = 'heading'},
            ui.label{text = index <= 3 and 'Cleared' or 'Locked', color = index <= 3 and 'successText' or 'textMuted'},
        }
    end
    return cards
end

-- A page of the carousel: the landscape at a time of day, which a tint gives the light of that hour, kept clear of the arrows on the sides.
local function page(tint, caption)
    return ui.column{gap = 8, padding = {0, 80}, ui.image{image = 'interface/images/landscape.png', tint = tint, height = 160, fit = 'cover', radius = 12, align = 'stretch'}, ui.label{text = caption, align = 'center'}}
end

function WindowsPages:enter()
    self:frame{
        hint = 'Drag the window by its title bar and close it with its button or Escape while the focus is inside. Swipe or drag the carousel, or focus its dots and press left and right. Drag the level row and let go: it settles on the nearest card.',
        code = WindowsPages.code,
        focus = 'open-window',
        content = {self:columns()},
    }
end

function WindowsPages:report(text)
    self:set('status', {text = text})
end

-- The window opens docked over its place in the first card, and follows that place through changes of the layout until the player moves it.
function WindowsPages:update(dt)
    WindowsPages.super.update(self, dt)
    local dock = self.gui:bounds('dock')
    if not dock or self.moved then
        return
    end
    local key = string.format('%.0f %.0f %.0f', dock.x, dock.y, dock.width)
    if key ~= self.docked then
        self.docked = key
        self.gui:set('bag', {x = dock.x, y = dock.y, width = dock.width})
    end
end

-- Opens the window again in its place.
function WindowsPages:openWindow()
    self.moved = false
    self.docked = nil
    self.gui:set('bag', {open = true})
end

-- Three columns: the window over its place in the first card, the accordion, and the carousel over the scroll of levels.
function WindowsPages:columns()
    local sections = {{id = 'controls', text = 'Controls'}, {id = 'goals', text = 'Goals'}, {id = 'secrets', text = 'Secrets', enabled = false}}
    return layout.columns{
        ui.column{grow = 1, align = 'stretch',
            layout.section('Component "window"', {grow = 1,
                ui.panel{id = 'dock', height = 330, align = 'stretch', justify = 'center', alignItems = 'center', ui.label{text = 'The window opens here.', color = 'textMuted'}},
                ui.label{text = 'The window floats over the page and takes no room in it.', color = 'textMuted'},
                ui.button{id = 'open-window', text = 'Open the window', onClick = function() self:openWindow() end},
            }),
        },
        ui.column{grow = 1, align = 'stretch',
            layout.section('Component "accordion"', {grow = 1,
                ui.accordion{items = sections, expanded = {'controls'}, onToggle = function(event)
                    self:report(string.format('The section "%s" %s.', event.item, event.expanded and 'opened' or 'closed'))
                end,
                    ui.label{text = 'Move with the left stick and chop with the south button.'},
                    ui.label{text = 'Survive ten nights on the island.'},
                    ui.label{text = 'Hidden until the lighthouse is lit.'},
                },
            }),
        },
        ui.column{grow = 2, gap = 24,
            layout.section('Component "carousel"', {
                ui.carousel{loop = true, interval = 5, onChange = function(event) self:report('The carousel shows page ' .. event.page .. '.') end,
                    page('#FFFFFFFF', 'Day'),
                    page('#FFFFB08A', 'Dusk'),
                    page('#FF5A6AB8', 'Night'),
                },
            }),
            layout.section('Component "scroll" with snapping', {
                ui.scroll{axis = 'horizontal', snap = true, height = 140, ui.row{gap = 24, children = levels()}},
            }),
        },
        ui.window{id = 'bag', title = 'Bag', closable = true,
            onMove = function(event)
                self.moved = true
                self:report(string.format('The window moved to %.0f, %.0f.', event.x, event.y))
            end,
            onClose = function() self:report('The window closed.') end,
            ui.accordion{multiple = true, items = {{id = 'food', text = 'Food'}, {id = 'tools', text = 'Tools'}}, expanded = {'food'},
                ui.row{gap = 12, ui.icon{image = 'interface/icons/apple.png', size = 56}, ui.icon{image = 'interface/icons/fish.png', size = 56}, ui.button{text = 'Eat an apple', onClick = function() self:report('Ate an apple.') end}},
                ui.row{gap = 12, ui.icon{image = 'interface/icons/hammer.png', size = 56}, ui.icon{image = 'interface/themes/fantasy/icons/key.png', size = 56}},
            },
        },
    }
end

return WindowsPages
