-- The building blocks the screens share: the bag button with its count, the picture of a product in its color over a backdrop, and the templates of the cells of products and bag lines.
local events = require('haylen.events')
local ui = require('haylen.ui')

local Bag = require('bag')
local Shell = require('shell')

local Widgets = {}

-- The bag button of a header, with a badge that follows the count of the bag.
function Widgets.bagButton(page)
    events.on('bagChanged', function()
        Widgets.showCount(page.gui)
    end, {owner = page})
    return ui.stack{
        ui.button{id = 'bag', icon = Shell.icon('bag'), variant = 'icon', tooltip = 'Bag', accessibilityLabel = 'Bag', onClick = function()
            Shell.open(require('screens.bag'))
        end},
        ui.row{align = 'stretch', ui.spacer{grow = 1}, ui.badge{id = 'bagCount', text = '', tone = 'accent', solid = true, visible = false, align = 'start'}},
    }
end

function Widgets.showCount(gui)
    local count = Bag.count()
    gui:set('bagCount', {text = count, visible = count > 0})
end

-- The picture of a product: the product in its color over a pastel backdrop with rounded corners. With `parts` the images bind to the fields of an item.
function Widgets.picture(properties)
    return ui.stack{align = properties.align or 'stretch', aspectRatio = properties.aspectRatio, width = properties.width, height = properties.height,
        ui.image{part = properties.parts and 'backdrop' or nil, bind = properties.parts and {image = 'backdrop'} or nil, id = properties.backdropId, image = properties.backdrop, fit = 'cover', align = 'stretch', radius = properties.radius or 18},
        ui.image{part = properties.parts and 'image' or nil, bind = properties.parts and {image = 'image', tint = 'tint'} or nil, id = properties.imageId, image = properties.image, tint = properties.tint or '#FFFFFFFF', fit = 'contain', align = 'stretch', margin = properties.inset or 16},
    }
end

-- The card of a product in the grids and shelves: the picture with the discount, the name, the price with the old price struck through and the rating.
function Widgets.productCard(width)
    return {template = ui.column{gap = 4, width = width, padding = {0, 0, 8, 0},
        ui.stack{align = 'stretch',
            Widgets.picture({parts = true, aspectRatio = 0.85, image = 'products/tee.svg', backdrop = 'products/backdrops/peach.svg'}),
            ui.row{align = 'stretch', padding = 10, ui.badge{part = 'sale', bind = {text = 'saleText', visible = 'hasSale'}, text = '', visible = false, tone = 'accent', solid = true, align = 'start'}},
        },
        ui.label{part = 'name', bind = {text = 'name'}, margin = {4, 0, 0, 0}, wrap = false},
        ui.richText{part = 'price', bind = {text = 'priceMarkup'}, text = '', wrap = false},
        ui.row{gap = 6,
            ui.icon{image = Shell.icon('star'), size = 18, color = 'warning'},
            ui.label{part = 'rating', bind = {text = 'ratingText'}, font = 'caption', color = 'textMuted', wrap = false},
        },
    }}
end

return Widgets
