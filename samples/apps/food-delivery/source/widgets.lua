-- The building blocks the screens share: the cart button with its count, and the templates of the cells that show restaurants, dishes and orders.
local events = require('haylen.events')
local ui = require('haylen.ui')

local Basket = require('basket')
local Shell = require('shell')

local Widgets = {}

-- The cart button of a header, with a badge that follows the count of the cart.
function Widgets.cartButton(page)
    events.on('basketChanged', function()
        Widgets.showCount(page.gui)
    end, {owner = page})
    return ui.stack{
        ui.button{id = 'cart', icon = Shell.icon('bag'), variant = 'icon', tooltip = 'Cart', accessibilityLabel = 'Cart', onClick = function()
            Shell.open(require('screens.cart'))
        end},
        ui.row{align = 'stretch', ui.spacer{grow = 1}, ui.badge{id = 'cartCount', text = '', tone = 'accent', solid = true, visible = false, align = 'start'}},
    }
end

function Widgets.showCount(gui)
    local count = Basket.count()
    gui:set('cartCount', {text = count, visible = count > 0})
end

-- The tall card of a restaurant in the shelves of the home screen.
function Widgets.restaurantCard(width)
    return {template = ui.column{gap = 6, width = width, padding = {0, 0, 8, 0},
        ui.stack{align = 'stretch',
            ui.image{part = 'image', bind = {image = 'image'}, fit = 'cover', height = width * 0.58, align = 'stretch', radius = 18},
            ui.row{align = 'stretch', padding = 10, ui.badge{part = 'promo', bind = {text = 'promo', visible = 'hasPromo'}, text = '', visible = false, tone = 'accent', solid = true, align = 'start'}},
        },
        ui.label{part = 'name', bind = {text = 'name'}, font = 'button', wrap = false},
        ui.label{part = 'details', bind = {text = 'details'}, font = 'caption', color = 'textMuted', wrap = false},
        ui.row{gap = 6,
            ui.icon{image = Shell.icon('star'), size = 18, color = 'warning'},
            ui.label{part = 'rating', bind = {text = 'ratingText'}, font = 'caption'},
            ui.label{part = 'neighborhood', bind = {text = 'neighborhood'}, font = 'caption', color = 'textMuted', wrap = false},
        },
    }}
end

-- The row of a restaurant in the long lists of the search tab.
function Widgets.restaurantRow()
    return {template = ui.row{gap = 16, padding = {8, 8},
        ui.image{part = 'image', bind = {image = 'image'}, fit = 'cover', width = 132, height = 96, radius = 16},
        ui.column{gap = 2, grow = 1,
            ui.row{gap = 8,
                ui.label{part = 'name', bind = {text = 'name'}, font = 'button', wrap = false, grow = 1},
                ui.badge{part = 'promo', bind = {text = 'promo', visible = 'hasPromo'}, text = '', visible = false, tone = 'accent'},
            },
            ui.label{part = 'details', bind = {text = 'details'}, font = 'caption', color = 'textMuted', wrap = false},
            ui.row{gap = 6,
                ui.icon{image = Shell.icon('star'), size = 18, color = 'warning'},
                ui.label{part = 'rating', bind = {text = 'ratingText'}, font = 'caption'},
                ui.label{part = 'neighborhood', bind = {text = 'neighborhood'}, font = 'caption', color = 'textMuted', wrap = false},
            },
        },
    }, estimatedSize = 112}
end

-- The row of a dish in a menu, with the mark that a press adds it to the cart.
function Widgets.dishRow()
    return {template = ui.row{gap = 16, padding = {10, 8},
        ui.column{gap = 4, grow = 1,
            ui.row{gap = 8,
                ui.label{part = 'name', bind = {text = 'name'}, font = 'button', wrap = false},
                ui.badge{part = 'tag', bind = {text = 'tag', visible = 'hasTag'}, text = '', visible = false, tone = 'warning'},
            },
            ui.label{part = 'description', bind = {text = 'description'}, font = 'caption', color = 'textMuted'},
            ui.label{part = 'price', bind = {text = 'priceText'}, font = 'button', color = 'accentText'},
        },
        ui.stack{
            ui.image{part = 'image', bind = {image = 'image'}, fit = 'cover', width = 120, height = 104, radius = 16},
            ui.row{align = 'stretch', padding = 6, ui.spacer{grow = 1}, ui.icon{image = Shell.icon('add-circle'), size = 40, color = 'accent', align = 'end'}},
        },
    }, estimatedSize = 140}
end

-- The row of an order, with its state and a button to track it or to order it again.
function Widgets.orderRow()
    return {template = ui.row{gap = 16, padding = {10, 8},
        ui.image{part = 'image', bind = {image = 'image'}, fit = 'cover', width = 96, height = 80, radius = 14},
        ui.column{gap = 4, grow = 1,
            ui.label{part = 'name', bind = {text = 'name'}, font = 'button', wrap = false},
            ui.label{part = 'summary', bind = {text = 'summary'}, font = 'caption', color = 'textMuted', wrap = false},
            ui.badge{part = 'status', bind = {text = 'status', tone = 'tone'}, text = '', tone = 'success'},
        },
        ui.button{part = 'action', bind = {text = 'action'}, text = 'Reorder'},
    }, estimatedSize = 116}
end

return Widgets
