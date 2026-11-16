-- The browse tab: every product of the store in a recycled grid that fits its columns to the width, narrowed by a typed text, a category and a popover of filters with the price, the colors and the sale, in the chosen order. The route of a category, such as "category/sneakers", shows the same screen as a page of its own with the category picked and a way back.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Shell = require('shell')
local Store = require('data.store')
local Style = require('style')
local Widgets = require('widgets')

local Browse = haylen.class('Browse', Page)

Browse.sorts = {
    {id = 'popular', text = 'Most popular', less = function(a, b) return a.popularity > b.popularity end},
    {id = 'new', text = 'Newest', less = function(a, b) return a.reviews < b.reviews end},
    {id = 'cheap', text = 'Price, low to high', less = function(a, b) return a.price < b.price end},
    {id = 'expensive', text = 'Price, high to low', less = function(a, b) return a.price > b.price end},
    {id = 'rated', text = 'Best rated', less = function(a, b) return a.rating > b.rating end},
}
Browse.maxPrice = 250

-- The filters in a popover that points at their button, or a sheet on a phone, which hands back the filters to use or a request to reset them.
local Filters = haylen.class('Filters', ui.Screen)

function Filters:init(current)
    self.pending = {low = current.low, high = current.high, sale = current.sale, colors = {}}
    for id, picked in pairs(current.colors) do
        self.pending.colors[id] = picked
    end
end

function Filters:build()
    local space = Style.space
    local colors = {gap = space.xs, wrap = true}
    for _, color in ipairs(Store.colors) do
        colors[#colors + 1] = ui.chip{id = 'color-' .. color.id, text = color.name, selected = self.pending.colors[color.id] == true, onChange = function(event)
            self.pending.colors[color.id] = event.selected or nil
        end}
    end
    return ui.column{gap = space.s, padding = {space.m, space.l, space.l, space.l},
        ui.label{text = 'Filters', font = 'heading'},
        ui.label{text = 'Price', font = 'button'},
        ui.rangeSlider{id = 'price', min = 0, max = Browse.maxPrice, low = self.pending.low, high = self.pending.high, step = 5, showValue = true, decimals = 0, autofocus = true, onChange = function(event)
            self.pending.low, self.pending.high = event.low, event.high
        end},
        ui.label{text = 'Colors', font = 'button', margin = {space.s, 0, 0, 0}},
        ui.row(colors),
        ui.toggle{id = 'sale', text = 'Only products on sale', checked = self.pending.sale, margin = {space.s, 0, 0, 0}, onChange = function(event)
            self.pending.sale = event.checked
        end},
        ui.row{gap = space.s, margin = {space.s, 0, 0, 0},
            ui.button{id = 'reset', text = 'Reset', grow = 1, onClick = function() self.presentation:dismiss({result = {reset = true}}) end},
            ui.button{id = 'apply', text = 'Show products', variant = 'primary', grow = 1, onClick = function() self.presentation:dismiss({result = self.pending}) end},
        },
    }
end

function Browse:tree()
    local space = Style.space
    self.query = ''
    self.category = self.params.category
    self.sort = Browse.sorts[1]
    for _, sort in ipairs(Browse.sorts) do
        if sort.id == self.params.sort then
            self.sort = sort
        end
    end
    self.filters = {low = 0, high = Browse.maxPrice, sale = self.params.sale == true, colors = {}}
    local chips = {gap = space.xs, padding = {0, space.l}, ui.chip{id = 'category-all', text = 'All', selected = self.category == nil, onChange = function()
        self:pickCategory(nil)
    end}}
    for _, category in ipairs(Store.categories) do
        chips[#chips + 1] = ui.chip{id = 'category-' .. category.id, text = category.name, selected = self.category == category.id, onChange = function()
            self:pickCategory(category.id)
        end}
    end
    local sorts = {}
    for _, sort in ipairs(Browse.sorts) do
        sorts[#sorts + 1] = {id = sort.id, text = sort.text}
    end
    local content = ui.column{grow = 1, gap = space.s, padding = {space.m, 0, 0, 0},
        Shell.centered(Style.size.page, ui.column{gap = space.s,
            ui.row{gap = space.xs, padding = {0, space.l},
                ui.filterField{id = 'query', placeholder = 'Search the store', grow = 1, returnKey = 'search', onChange = function(event)
                    self.query = string.lower(event.value)
                    self:refresh()
                end},
                ui.button{id = 'filters', text = 'Filters', icon = Shell.icon('filter'), iconColor = 'text', onClick = function()
                    self:openFilters()
                end},
            },
            ui.scroll{axis = 'horizontal', scrollbar = false, ui.row(chips)},
            ui.row{gap = space.s, padding = {0, space.m, 0, space.l},
                ui.label{id = 'count', text = '', color = 'textMuted', grow = 1, wrap = false},
                ui.menuButton{id = 'sort', text = self.sort.text, variant = 'toolbar', items = sorts, onSelect = function(event)
                    for _, sort in ipairs(Browse.sorts) do
                        if sort.id == event.item then
                            self.sort = sort
                        end
                    end
                    event.gui:set('sort', {text = self.sort.text})
                    self:refresh()
                end},
            },
        }),
        Shell.centered(Style.size.page, ui.collection{id = 'grid', grow = 1, layout = 'grid', minCellSize = Widgets.cellWidth, cellAspect = Widgets.cellAspect, gap = space.m, padding = {space.xs, space.l, space.xl, space.l}, rememberFocus = true,
            types = {product = Widgets.productCard()},
            onSelect = function(event)
                Widgets.openProduct(event.item)
            end,
            ui.emptyState{image = 'images/hero.png', imageSize = 220, title = 'Nothing matches', message = 'Try other words or fewer filters.'},
        }, 1),
    }
    if self.params.category then
        return ui.column{gap = 0, Shell.header(Store.category(self.params.category).name, {Shell.bagButton(self, 'headerBag')}), content}
    end
    return Shell.tabPage(content, Shell.phoneBar(self, 'Browse'))
end

function Browse:start()
    self.grid = self.gui:collection('grid')
    self:refresh()
end

-- Shows what another screen asks for in every category, such as the newest products or the products on sale, or puts the focus in the search.
function Browse:apply(params)
    if params.sort or params.sale ~= nil then
        self:pickCategory(nil)
    end
    for _, sort in ipairs(Browse.sorts) do
        if sort.id == (params.sort or Browse.sorts[1].id) then
            self.sort = sort
        end
    end
    self.gui:set('sort', {text = self.sort.text})
    self.filters.sale = params.sale == true
    self:refresh()
    if params.focus then
        self.gui:command('query', 'focus')
    end
end

-- Opens the filters with the values in use, so filters closed without applying them change nothing.
function Browse:openFilters()
    self.filtersSheet = ui.present(Filters(self.filters), {style = 'popover', anchor = {gui = self.gui, id = 'filters'}, width = 560, detents = {'fit'}, owner = self, onDismiss = function(event)
        if not event.result then
            return
        end
        self.filters = event.result.reset and {low = 0, high = Browse.maxPrice, sale = false, colors = {}} or event.result
        self:refresh()
    end})
end

function Browse:pickCategory(id)
    local previous = self.category
    self.category = id
    self.gui:set('category-' .. (previous or 'all'), {selected = false})
    self.gui:set('category-' .. (id or 'all'), {selected = true})
    self:refresh()
end

function Browse:matches(product)
    local filters = self.filters
    if self.category and product.category ~= self.category then
        return false
    end
    if product.price < filters.low or product.price > filters.high or (filters.sale and not product.oldPrice) then
        return false
    end
    if next(filters.colors) then
        local found = false
        for _, color in ipairs(product.colors) do
            found = found or filters.colors[color] == true
        end
        if not found then
            return false
        end
    end
    return self.query == '' or string.find(product.searchText, self.query, 1, true) ~= nil
end

function Browse:refresh()
    local shown = {}
    for _, product in ipairs(Store.products) do
        if self:matches(product) then
            shown[#shown + 1] = product
        end
    end
    table.sort(shown, self.sort.less)
    self.grid:setItems(shown)
    self.gui:set('count', {text = #shown == 1 and '1 product' or Store.thousands(#shown) .. ' products'})
end

return Browse
