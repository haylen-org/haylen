-- The shop tab: the edit of the season, the categories, the promises of the store and the shelves of new arrivals, best sellers and offers, in a column that keeps the reading width of the store on wide windows.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Shell = require('shell')
local Store = require('data.store')
local Style = require('style')
local Widgets = require('widgets')

local Shop = haylen.class('Shop', Page)

Shop.shelfSize = 14

function Shop:enter()
    self:mountGui(Shell.frame(self, 'home', ui.scroll{id = 'scroll', grow = 1, height = 0,
        Shell.centered(Style.size.page, ui.column{id = 'page', gap = Style.space.m, padding = {Style.space.l, Style.space.l, Style.space.xxl, Style.space.l},
            ui.column{id = 'campaign'},
            Widgets.section('Shop by category'),
            ui.collection{id = 'categories', axis = 'horizontal', scrollbar = false, gap = Style.space.m, rememberFocus = true, types = {category = {template = ui.column{gap = Style.space.xs, width = Style.size.tile,
                ui.stack{width = Style.size.tile, height = Style.size.tile,
                    ui.image{part = 'backdrop', bind = {image = 'backdrop'}, fit = 'cover', align = 'stretch', radius = Style.radius.picture},
                    ui.image{part = 'image', bind = {image = 'image', tint = 'tint'}, fit = 'contain', align = 'stretch', margin = Style.space.l},
                },
                ui.label{part = 'name', bind = {text = 'name'}, textAlign = 'center', align = 'stretch', wrap = false},
            }}}, onSelect = function(event)
                Shell.showTab('browse', {category = event.item})
            end},
            Widgets.section('New in', 'See all', function() Shell.showTab('browse', {sort = 'new'}) end),
            Widgets.shelf('new'),
            ui.row{id = 'promises', gap = Style.space.m, wrap = true, margin = {Style.space.l, 0, 0, 0}, align = 'stretch',
                self:promise('truck', 'Free shipping over $75', 'Every order ships in one or two days.'),
                self:promise('return', 'Free returns for 30 days', 'Send it back in its box for a full refund.'),
                self:promise('tag', 'Ten percent off your first order', 'Use the code WELCOME10 in your bag.'),
            },
            Widgets.section('Best sellers', 'See all', function() Shell.showTab('browse') end),
            Widgets.shelf('best'),
            Widgets.section('On sale', 'See all', function() Shell.showTab('browse', {sale = true}) end),
            Widgets.shelf('sale'),
        }),
    }, Shell.phoneBar(self)))
    self.gui:collection('categories'):setItems(self:categoryItems())
    self.gui:collection('new'):setItems(self:pick(function(product) return product.reviews < 40 end))
    self.gui:collection('best'):setItems(self:pick(function(product) return product.popularity > 90000 end))
    self.gui:collection('sale'):setItems(self:pick(function(product) return product.oldPrice ~= nil end))
end

-- The edit of the season, with the illustration of the app beside the text on wide windows and above it on phones.
function Shop:layoutChanged(wide)
    local space = Style.space
    local text = ui.column{gap = space.s, grow = 1, padding = wide and {space.xl, 0} or 0, justify = 'center',
        ui.label{text = 'The summer edit', font = 'title', style = Style.text('title', Style.type.display)},
        ui.label{text = 'Light layers, easy colors and the bags to carry them. Up to 40% off hundreds of favorites.', color = 'textMuted', maxWidth = 520},
        ui.button{id = 'campaignButton', text = 'Shop the edit', variant = 'primary', align = 'start', margin = {space.s, 0, 0, 0}, onClick = function()
            Shell.showTab('browse', {sale = true})
        end},
    }
    local art = ui.image{image = 'images/hero.png', fit = 'contain', width = wide and 440 or 'auto', height = wide and 330 or 260, align = wide and 'center' or 'stretch', accessibilityHidden = true}
    local card = ui.card{padding = wide and {space.l, space.xxl} or {space.l, space.l, space.xl, space.l}, gap = space.l, style = {colors = {raised = Style.color.sand, border = Style.color.sand}},
        wide and ui.row{gap = space.xl, text, art} or ui.column{gap = space.m, art, text},
    }
    self.gui:replaceChildren('campaign', {card})
    self.gui:set('page', {padding = wide and {space.xl, space.xl, space.xxxl, space.xl} or {space.l, space.l, space.xxl, space.l}})
end

-- A promise of the store with its icon, a third of the row on wide windows and the whole width on phones.
function Shop:promise(icon, title, text)
    local space = Style.space
    return ui.card{grow = 1, minWidth = 300, padding = {space.m, space.l}, style = {colors = {raised = Style.color.surface}},
        ui.row{gap = space.m,
            ui.icon{image = Shell.icon(icon), size = 36, color = 'accentText'},
            ui.column{gap = space.xxs, grow = 1, ui.label{text = title, font = 'button'}, ui.label{text = text, font = 'caption', color = 'textMuted'}},
        },
    }
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
