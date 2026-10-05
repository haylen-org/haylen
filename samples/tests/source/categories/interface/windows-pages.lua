-- A floating window the pointer drags by its title bar, accordions with one or several open sections, a carousel of pages and a horizontal scroll that snaps to its cards.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local WindowsPages = haylen.class('WindowsPages', Test)

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

-- A carousel page: a picture with its caption, narrower than the page so the arrows on the sides stay clear.
local function page(image, caption)
    return ui.column{gap = 8, ui.image{image = image, height = 140, fit = 'contain', align = 'center'}, ui.label{text = caption, align = 'center'}}
end

function WindowsPages:enter()
    self:frame{
        hint = 'Drag the window by its title bar and close it with its button or Escape while the focus is inside. Swipe or drag the carousel, or focus its dots and press left and right. Drag the level row and let go: it settles on the nearest card.',
        focus = 'open-window',
        content = {self:columns()},
    }
end

function WindowsPages:report(text)
    self:set('status', {text = text})
end

function WindowsPages:columns()
    local sections = {{id = 'controls', text = 'Controls'}, {id = 'goals', text = 'Goals'}, {id = 'secrets', text = 'Secrets', enabled = false}}
    return layout.columns{
        ui.column{grow = 1, gap = 24,
            layout.section('Component "window"', {
                ui.label{text = 'The window floats over the page and takes no room in it.', color = 'textMuted'},
                ui.button{id = 'open-window', text = 'Open the window', onClick = function(event) event.gui:set('bag', {open = true}) end},
            }),
            layout.section('Component "accordion"', {
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
                ui.carousel{height = 230, loop = true, interval = 5, onChange = function(event) self:report('The carousel shows page ' .. event.page .. '.') end,
                    page('interface/images/landscape_day.png', 'Day'),
                    page('interface/images/landscape_dusk.png', 'Dusk'),
                    page('interface/images/landscape_night.png', 'Night'),
                },
            }),
            layout.section('Component "scroll" with snapping', {
                ui.scroll{axis = 'horizontal', snap = true, height = 150, ui.row{gap = 24, children = levels()}},
            }),
        },
        ui.window{id = 'bag', title = 'Bag', x = 1240, y = 380, width = 620, closable = true,
            onMove = function(event) self:report(string.format('The window moved to %.0f, %.0f.', event.x, event.y)) end,
            onClose = function() self:report('The window closed.') end,
            ui.label{text = 'An accordion with "multiple = true" keeps several sections open.', color = 'textMuted'},
            ui.accordion{multiple = true, items = {{id = 'food', text = 'Food'}, {id = 'tools', text = 'Tools'}}, expanded = {'food', 'tools'},
                ui.row{gap = 12, ui.icon{image = 'interface/icons/apple.png', size = 56}, ui.icon{image = 'interface/icons/fish.png', size = 56}, ui.button{text = 'Eat an apple', onClick = function() self:report('Ate an apple.') end}},
                ui.row{gap = 12, ui.icon{image = 'interface/icons/hammer.png', size = 56}, ui.icon{image = 'interface/icons/key.png', size = 56}},
            },
        },
    }
end

return WindowsPages
