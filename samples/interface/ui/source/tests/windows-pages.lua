-- Windows and pages: a floating window the pointer drags by its title bar, accordions with one or several open sections, a carousel of pages and a horizontal scroll that snaps to its cards.
local haylen = require('haylen')
local ui = require('haylen.ui')

local sample = require('sample')

local WindowsPages = haylen.class('WindowsPages', sample.Test)

WindowsPages.hints = 'Drag the window by its title bar and close it with its button or Escape while the focus is inside. Swipe or drag the carousel, or focus its dots and press left and right. Drag the level row and let go: it settles on the nearest card.'
WindowsPages.focus = 'open-window'

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

function WindowsPages:content()
    local sections = {{id = 'controls', text = 'Controls'}, {id = 'goals', text = 'Goals'}, {id = 'secrets', text = 'Secrets', enabled = false}}
    return sample.columns{
        ui.column{grow = 1, gap = 24,
            sample.section('window', {
                ui.label{text = 'The window floats over the page and takes no room in it.', color = 'textMuted'},
                ui.button{id = 'open-window', text = 'Open the window', onClick = function(event) event.document:set('bag', {open = true}) end},
            }),
            sample.section('accordion', {
                ui.accordion{items = sections, expanded = {'controls'}, onToggle = function(event)
                    self:setStatus(event.item .. (event.expanded and ' opened' or ' closed'))
                end,
                    ui.label{text = 'Move with the left stick and chop with the south button.'},
                    ui.label{text = 'Survive ten nights on the island.'},
                    ui.label{text = 'Hidden until the lighthouse is lit.'},
                },
            }),
        },
        ui.column{grow = 2, gap = 24,
            sample.section('carousel', {
                ui.carousel{height = 230, loop = true, interval = 5, onChange = function(event) self:setStatus('carousel page ' .. event.page) end,
                    page('images/landscape_day.png', 'Day'),
                    page('images/landscape_dusk.png', 'Dusk'),
                    page('images/landscape_night.png', 'Night'),
                },
            }),
            sample.section('scroll with snapping', {
                ui.scroll{direction = 'horizontal', snap = true, height = 150, ui.row{gap = 24, children = levels()}},
            }),
        },
        ui.window{id = 'bag', title = 'Bag', x = 1100, y = 560, width = 620, closable = true,
            onMove = function(event) self:setStatus(string.format('window moved to %.0f, %.0f', event.x, event.y)) end,
            onClose = function() self:setStatus('window closed') end,
            ui.label{text = 'An accordion with multiple = true keeps several sections open.', color = 'textMuted'},
            ui.accordion{multiple = true, items = {{id = 'food', text = 'Food'}, {id = 'tools', text = 'Tools'}}, expanded = {'food', 'tools'},
                ui.row{gap = 12, ui.icon{image = 'icons/apple.png', size = 56}, ui.icon{image = 'icons/fish.png', size = 56}, ui.button{text = 'Eat an apple', onClick = function() self:setStatus('ate an apple') end}},
                ui.row{gap = 12, ui.icon{image = 'icons/hammer.png', size = 56}, ui.icon{image = 'icons/key.png', size = 56}},
            },
        },
    }
end

return WindowsPages
