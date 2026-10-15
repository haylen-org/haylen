-- The wishlist tab: the products saved with the heart.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Shell = require('shell')
local Widgets = require('widgets')
local Wishlist = require('wishlist')

local WishlistPage = haylen.class('WishlistPage', Page)

function WishlistPage:enter()
    self:mountGui(Shell.frame('wishlist', ui.column{grow = 1, gap = 0,
        ui.row{gap = 12, padding = {14, 16, 8, 20}, ui.label{text = 'Wishlist', font = 'title', grow = 1}, Widgets.bagButton(self)},
        ui.collection{id = 'wishlist', grow = 1, layout = 'grid', minCellSize = 210, cellAspect = 1.62, gap = 16, padding = {4, 20, 24, 20}, rememberFocus = true,
            types = {product = Widgets.productCard()},
            onSelect = function(event)
                Shell.open(require('screens.product'), {product = event.item})
            end,
            ui.emptyState{image = 'images/hero.png', imageSize = 220, title = 'Your wishlist is empty', message = 'Tap the heart of a product to keep it here.'},
        },
    }))
    Widgets.showCount(self.gui)
    self.list = self.gui:collection('wishlist')
    self.list:setItems(Wishlist.list)
    events.on('wishlistChanged', function()
        self.list:setItems(Wishlist.list)
    end, {owner = self})
end

return WishlistPage
