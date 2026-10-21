-- The building blocks the screens share: the picture of a product in its color over a pastel backdrop, the template of the product card, the swatches of the colors and the shelves of products.
local graphics = require('haylen.graphics')
local ui = require('haylen.ui')

local Shell = require('shell')
local Store = require('data.store')
local Style = require('style')

local Widgets = {}

-- The picture of a product: the product in its color over a pastel backdrop with rounded corners. With `parts` the images bind to the fields of an item.
function Widgets.picture(properties)
    return ui.stack{align = properties.align or 'stretch', aspectRatio = properties.aspectRatio, width = properties.width, height = properties.height, grow = properties.grow,
        ui.image{part = properties.parts and 'backdrop' or nil, bind = properties.parts and {image = 'backdrop'} or nil, id = properties.backdropId, image = properties.backdrop or 'products/backdrops/sand.svg', fit = 'cover', align = 'stretch', radius = properties.radius or Style.radius.picture},
        ui.image{part = properties.parts and 'image' or nil, bind = properties.parts and {image = 'image', tint = 'tint'} or nil, id = properties.imageId, image = properties.image or 'products/tee.svg', tint = properties.tint or '#FFFFFFFF', fit = 'contain', align = 'stretch', margin = properties.inset or Style.space.l},
    }
end

-- The card of a product: the picture with the discount, the name, the price with the old price struck through and the count of colors. A card of a shelf has a width and a picture of a fixed shape, and the card of a grid fills its cell, where the picture takes the room the text leaves.
function Widgets.productCard(width)
    local space = Style.space
    return {template = ui.column{gap = space.xxs, width = width, padding = {0, 0, space.xs, 0},
        ui.stack{align = 'stretch', grow = width and 0 or 1,
            Widgets.picture({parts = true, aspectRatio = width and 0.84 or nil}),
            ui.row{align = 'stretch', alignItems = 'start', padding = space.s, ui.badge{part = 'sale', bind = {text = 'saleText', visible = 'hasSale'}, text = '', visible = false, tone = 'danger', solid = true}},
        },
        ui.label{part = 'name', bind = {text = 'name'}, wrap = false, margin = {space.xs, 0, 0, 0}},
        ui.richText{part = 'price', bind = {text = 'priceMarkup'}, text = '', wrap = false},
        ui.label{part = 'colors', bind = {text = 'colorsText'}, font = 'caption', color = 'textMuted', wrap = false},
    }}
end

-- The length of a grid cell of a product for its width, so the picture keeps about the shape of the cards of the shelves.
Widgets.cellAspect = 1.78
Widgets.cellWidth = 200

-- A shelf of products that scrolls sideways, and opens a product when one is picked.
function Widgets.shelf(id)
    return ui.collection{id = id, axis = 'horizontal', scrollbar = false, gap = Style.space.m, rememberFocus = true, types = {product = Widgets.productCard(Style.size.card)}, onSelect = function(event)
        Shell.open(require('screens.product'), {product = event.item})
    end}
end

-- A title of a section in the serif with an optional link at its end.
function Widgets.section(text, link, onLink)
    return ui.row{gap = Style.space.s, margin = {Style.space.l, 0, 0, 0},
        ui.label{text = text, font = 'heading', grow = 1},
        link and ui.button{text = link, variant = 'link', style = {metrics = {controlPaddingX = 0}}, onClick = onLink} or nil,
    }
end

function Widgets.swatchName(colorId, selected)
    return '@swatch-' .. colorId .. (selected and '-on' or '')
end

-- Names a round swatch of every color for the UI, and a second one inside a dark ring for the color that is picked.
function Widgets.registerSwatches()
    for _, color in ipairs(Store.colors) do
        local fill = '#' .. color.value:sub(4)
        local dot = string.format('<circle cx="24" cy="24" r="16" fill="%s" stroke="#D8CFC3" stroke-width="1.5"/>', fill)
        ui.setImage(Widgets.swatchName(color.id, false), graphics.newVectorImage('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 48" width="48" height="48">' .. dot .. '</svg>'))
        ui.setImage(Widgets.swatchName(color.id, true), graphics.newVectorImage('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 48" width="48" height="48"><circle cx="24" cy="24" r="22" fill="none" stroke="#1D1B18" stroke-width="2.5"/>' .. dot .. '</svg>'))
    end
end

-- A row of costs of the bag or the checkout, with the total in the serif.
function Widgets.costRow(id, text, total)
    return ui.row{gap = Style.space.s,
        ui.label{text = text, font = total and 'heading' or 'body', color = total and 'text' or 'textMuted', grow = 1},
        ui.label{id = id, text = '', font = total and 'heading' or 'body'},
    }
end

function Widgets.showCosts(gui, totals)
    gui:set('subtotal', {text = Store.money(totals.subtotal)})
    gui:set('discount', {text = totals.discount > 0 and '-' .. Store.money(totals.discount) or 'None'})
    gui:set('shipping', {text = totals.shipping == 0 and 'Free' or Store.money(totals.shipping)})
    gui:set('total', {text = Store.money(totals.total)})
end

return Widgets
