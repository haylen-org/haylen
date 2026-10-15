-- A seller: the profile with the rating, the sales and the reply time, and a switch between their listings and their reviews in one recycled collection.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Market = require('data.market')
local Page = require('page')
local Shell = require('shell')
local Widgets = require('widgets')

local Seller = haylen.class('Seller', Page)

function Seller:enter()
    local seller = Market.seller(self.params.seller)
    self.seller = seller
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header(seller.name),
        Shell.centered(1100, ui.column{gap = 12, padding = {0, 20, 8, 20},
            ui.card{
                ui.row{gap = 16,
                    ui.avatar{name = seller.name, size = 80},
                    ui.column{gap = 2, grow = 1,
                        ui.label{text = seller.name, font = 'heading'},
                        ui.row{gap = 6, ui.icon{image = Shell.icon('star'), size = 20, color = 'warning'}, ui.label{text = string.format('%s · %d sales', seller.rating, seller.sales), color = 'textMuted'}},
                        ui.label{text = seller.joined .. ' · ' .. seller.reply, font = 'caption', color = 'textMuted'},
                    },
                },
            },
            ui.segmentedControl{id = 'section', items = {{id = 'listings', text = string.format('Listings, %d', #seller.listings)}, {id = 'reviews', text = 'Reviews'}}, selected = 'listings', onChange = function(event)
                self:show(event.value)
            end},
        }),
        ui.collection{id = 'items', grow = 1, layout = 'grid', minCellSize = 230, gap = 14, padding = {4, 20, 24, 20}, rememberFocus = true,
            types = {listing = Widgets.listingCard(), review = Widgets.reviewRow()},
            onSelect = function(event)
                Shell.open(require('screens.listing'), {listing = event.item})
            end,
        },
    })
    self.items = self.gui:collection('items')
    self:show('listings')
end

function Seller:show(section)
    self.items:setItems(section == 'listings' and self.seller.listings or Market.reviews(self.seller))
    self.items.scrollOffset = 0
end

return Seller
