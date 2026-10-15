-- The building blocks the screens share: the picture of a listing on its backdrop, and the templates of the cells of listings, conversations and reviews.
local ui = require('haylen.ui')

local Shell = require('shell')

local Widgets = {}

-- The picture of a listing: its good drawn over a pastel backdrop with rounded corners.
function Widgets.picture(properties)
    return ui.stack{align = properties.align or 'stretch', aspectRatio = properties.aspectRatio, width = properties.width, height = properties.height,
        ui.image{part = properties.parts and 'backdrop' or nil, bind = properties.parts and {image = 'backdrop'} or nil, image = properties.backdrop, fit = 'cover', align = 'stretch', radius = properties.radius or 18},
        ui.image{part = properties.parts and 'image' or nil, bind = properties.parts and {image = 'image'} or nil, image = properties.image, fit = 'contain', align = 'stretch', margin = properties.inset or 14},
    }
end

-- The card of a listing in the grids: the picture, the price, the title and where and when it was posted.
function Widgets.listingCard()
    return {template = ui.column{gap = 4, padding = {0, 0, 8, 0},
        Widgets.picture({parts = true, aspectRatio = 1, image = 'goods/lamp.svg', backdrop = 'goods/backdrops/mint.svg'}),
        ui.row{gap = 6, margin = {4, 0, 0, 0},
            ui.label{part = 'price', bind = {text = 'priceText'}, font = 'heading', grow = 1, wrap = false},
            ui.badge{part = 'condition', bind = {text = 'condition'}, text = '', tone = 'accent'},
        },
        ui.label{part = 'title', bind = {text = 'title'}, wrap = false},
        ui.label{part = 'place', bind = {text = 'place'}, font = 'caption', color = 'textMuted', wrap = false},
    }}
end

-- The small card of a listing in the strip over the map.
function Widgets.listingStrip()
    return {template = ui.card{gap = 10, width = 340, padding = 10,
        ui.row{gap = 12,
            Widgets.picture({parts = true, width = 96, height = 96, radius = 14, inset = 8, image = 'goods/lamp.svg', backdrop = 'goods/backdrops/mint.svg'}),
            ui.column{gap = 2, grow = 1,
                ui.label{part = 'price', bind = {text = 'priceText'}, font = 'heading', wrap = false},
                ui.label{part = 'title', bind = {text = 'title'}, wrap = false},
                ui.label{part = 'place', bind = {text = 'place'}, font = 'caption', color = 'textMuted', wrap = false},
            },
        },
    }}
end

-- The row of a conversation in the inbox.
function Widgets.conversationRow()
    return {template = ui.row{gap = 14, padding = {10, 8},
        Widgets.picture({parts = true, width = 72, height = 72, radius = 36, inset = 8, image = 'goods/lamp.svg', backdrop = 'goods/backdrops/mint.svg'}),
        ui.column{gap = 0, grow = 1,
            ui.row{gap = 8,
                ui.label{part = 'name', bind = {text = 'name'}, font = 'button', grow = 1, wrap = false},
                ui.label{part = 'time', bind = {text = 'time'}, font = 'caption', color = 'textMuted'},
            },
            ui.label{part = 'title', bind = {text = 'title'}, font = 'caption', color = 'accentText', wrap = false},
            ui.row{gap = 8,
                ui.label{part = 'last', bind = {text = 'last'}, color = 'textMuted', grow = 1, wrap = false},
                ui.badge{part = 'unread', bind = {text = 'unreadText', visible = 'hasUnread'}, text = '', visible = false, tone = 'accent', solid = true},
            },
        },
    }, estimatedSize = 96}
end

-- The row of a review of a seller.
function Widgets.reviewRow()
    return {template = ui.column{gap = 4, padding = {12, 8},
        ui.row{gap = 10,
            ui.avatar{part = 'avatar', bind = {name = 'author'}, size = 40},
            ui.label{part = 'author', bind = {text = 'author'}, font = 'button', grow = 1, wrap = false},
            ui.icon{image = Shell.icon('star'), size = 18, color = 'warning'},
            ui.label{part = 'stars', bind = {text = 'stars'}, font = 'caption', color = 'textMuted'},
        },
        ui.label{part = 'text', bind = {text = 'text'}},
        ui.label{part = 'age', bind = {text = 'age'}, font = 'caption', color = 'textMuted'},
    }, span = 'full', interactive = false, estimatedSize = 120}
end

return Widgets
