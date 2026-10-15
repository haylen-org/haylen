-- The shop tab: the campaign of the season, the categories and the shelves of trending products, new arrivals and offers.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Shell = require('shell')
local Store = require('data.store')
local Widgets = require('widgets')

local Shop = haylen.class('Shop', Page)

Shop.shelfSize = 14
Shop.cardWidth = 230

function Shop:enter()
    self:mountGui(Shell.frame('home', ui.column{grow = 1, gap = 0,
        ui.row{gap = 12, padding = {10, 16, 6, 20},
            ui.image{image = 'images/logo.svg', width = 44, height = 44},
            ui.label{text = 'Online Store', font = 'heading', grow = 1, wrap = false},
            ui.button{id = 'search', icon = Shell.icon('search'), variant = 'icon', tooltip = 'Search', accessibilityLabel = 'Search', onClick = function()
                Shell.showTab('browse', {focus = true})
            end},
            Widgets.bagButton(self),
        },
        ui.scroll{grow = 1, height = 0,
            ui.column{maxWidth = 1240, align = 'center', gap = 16, padding = {6, 20, 32, 20},
                self:campaign(),
                ui.sectionTitle{text = 'Categories'},
                ui.collection{id = 'categories', axis = 'horizontal', scrollbar = false, gap = 8, rememberFocus = true, types = {category = {template = ui.column{gap = 8, width = 112, padding = {6, 4},
                    ui.stack{width = 96, height = 96, align = 'center',
                        ui.image{part = 'backdrop', bind = {image = 'backdrop'}, fit = 'cover', align = 'stretch', radius = 48},
                        ui.image{part = 'image', bind = {image = 'image', tint = 'tint'}, fit = 'contain', align = 'stretch', margin = 14},
                    },
                    ui.label{part = 'name', bind = {text = 'name'}, font = 'caption', textAlign = 'center', align = 'stretch', wrap = false},
                }}}, onSelect = function(event)
                    Shell.showTab('browse', {category = event.item})
                end},
                ui.sectionTitle{text = 'Trending now'},
                self:shelf('trending'),
                ui.sectionTitle{text = 'New in'},
                self:shelf('new'),
                ui.sectionTitle{text = 'On sale'},
                self:shelf('sale'),
            },
        },
    }))
    Widgets.showCount(self.gui)
    self.gui:collection('categories'):setItems(self:categoryItems())
    self.gui:collection('trending'):setItems(self:pick(function(product) return product.popularity > 90000 end))
    self.gui:collection('new'):setItems(self:pick(function(product) return product.reviews < 40 end))
    self.gui:collection('sale'):setItems(self:pick(function(product) return product.oldPrice ~= nil end))
end

-- The card of the campaign of the season, in the accent color with the illustration of the app.
function Shop:campaign()
    local accent = ui.themeColor('accent'):toHex()
    return ui.card{padding = {20, 12, 20, 26}, style = {colors = {raised = accent, border = accent}},
        ui.row{gap = 8,
            ui.column{gap = 6, grow = 1,
                ui.label{text = 'New season, fresh picks', font = 'heading', color = 'onAccent'},
                ui.label{text = 'Up to 40% off hundreds of favorites, and free shipping over $75.', font = 'caption', color = 'onAccent'},
                ui.button{id = 'campaign', text = 'Shop the sale', align = 'start', margin = {8, 0, 0, 0}, style = {colors = {raised = '#FFFFFFFF', border = '#FFFFFFFF', text = '#FF1F2A44'}}, onClick = function()
                    Shell.showTab('browse', {sale = true})
                end},
            },
            ui.image{image = 'images/hero.png', width = 220, height = 165, fit = 'contain', accessibilityHidden = true},
        },
    }
end

function Shop:shelf(id)
    return ui.collection{id = id, axis = 'horizontal', scrollbar = false, gap = 16, rememberFocus = true, types = {product = Widgets.productCard(Shop.cardWidth)}, onSelect = function(event)
        Shell.open(require('screens.product'), {product = event.item})
    end}
end

function Shop:categoryItems()
    local items = {}
    for index, category in ipairs(Store.categories) do
        items[index] = {id = category.id, type = 'category', name = category.name, image = 'products/' .. category.art .. '.svg', backdrop = 'products/backdrops/' .. Store.backdrops[(index - 1) % #Store.backdrops + 1] .. '.svg', tint = Store.colors[(index - 1) % #Store.colors + 1].value}
    end
    return items
end

function Shop:pick(test)
    local items = {}
    for _, product in ipairs(Store.products) do
        if test(product) then
            items[#items + 1] = product
            if #items == Shop.shelfSize then
                break
            end
        end
    end
    return items
end

return Shop
