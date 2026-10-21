-- The saved tab: the products kept with the heart, in the grid of the store.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Shell = require('shell')
local Style = require('style')
local Widgets = require('widgets')
local Wishlist = require('wishlist')

local Saved = haylen.class('Saved', Page)

function Saved:enter()
    local space = Style.space
    self:mountGui(Shell.frame(self, 'saved', ui.column{grow = 1, gap = 0,
        Shell.centered(Style.size.page, ui.column{gap = space.xxs, padding = {space.l, space.l, space.s, space.l},
            ui.label{text = 'Saved for later', font = 'title'},
            ui.label{id = 'count', text = '', color = 'textMuted'},
        }),
        Shell.centered(Style.size.page, ui.collection{id = 'saved', grow = 1, layout = 'grid', minCellSize = Widgets.cellWidth, cellAspect = Widgets.cellAspect, gap = space.m, padding = {space.xs, space.l, space.xl, space.l}, rememberFocus = true,
            types = {product = Widgets.productCard()},
            onSelect = function(event)
                Shell.open(require('screens.product'), {product = event.item})
            end,
            ui.emptyState{image = 'images/hero.png', imageSize = 240, title = 'Nothing saved yet', message = 'Tap the heart of a product to keep it here.'},
        }, 1),
    }, Shell.phoneBar(self, 'Saved')))
    self.list = self.gui:collection('saved')
    events.on('wishlistChanged', function()
        self:show()
    end, {owner = self})
    self:show()
end

function Saved:show()
    self.list:setItems(Wishlist.list)
    local count = #Wishlist.list
    self.gui:set('count', {text = count == 1 and '1 product' or count .. ' products'})
end

return Saved
